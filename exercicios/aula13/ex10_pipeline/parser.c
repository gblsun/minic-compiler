/* Implementação de parser.h — mesma gramática e mesmas ações de parser.py.
 *
 * O erro de sintaxe interrompe a análise inteira com longjmp (o equivalente
 * da exceção ErroSintaxe do Python). Nada vaza nesse salto: os nós estão na
 * arena do Programa e os escopos numa lista do Parser, e os dois são
 * liberados por quem instalou o setjmp.
 */

#include "parser.h"

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *nome;
    Tipo tipo;
    int linha, coluna;
} Simbolo;

typedef struct Scope {
    char *nome;
    struct Scope *pai;
    Simbolo *v;
    size_t n, cap;
} Scope;

typedef struct {
    TokenStream *ts;
    Programa *prog;
    Diagnosticos *diags;
    Scope **escopos; /* todos os escopos criados (liberados no fim) */
    size_t n_escopos, cap_escopos;
    int blocos;
    jmp_buf sintaxe;
} Parser;

/* -- utilitários ------------------------------------------------------------ */

static void diag(Diagnosticos *d, char *msg) {
    if (d->n == d->cap) {
        d->cap = d->cap ? d->cap * 2 : 8;
        d->v = xrealloc(d->v, d->cap * sizeof(char *));
    }
    d->v[d->n++] = msg;
}

void diagnosticos_free(Diagnosticos *d) {
    for (size_t i = 0; i < d->n; i++) free(d->v[i]);
    free(d->v);
    *d = (Diagnosticos){0};
}

static void semantico(Parser *p, int linha, int coluna, char *msg) {
    diag(p->diags, xformat("Erro semântico na linha %d, coluna %d: %s.", linha, coluna, msg));
    free(msg);
}

static void erro_sintaxe(Parser *p, const Token *t, const char *esperado) {
    char *encontrado = t->type == TOK_EOF ? xstrdup("fim da entrada") : xformat("'%s'", t->lexeme);
    diag(p->diags, xformat("Erro de sintaxe na linha %d, coluna %d: esperado %s; encontrado %s.", t->line, t->column,
                           esperado, encontrado));
    free(encontrado);
    longjmp(p->sintaxe, 1);
}

static const Token *exige(Parser *p, TokenType tipo, const char *esperado) {
    if (!ts_check(p->ts, tipo)) erro_sintaxe(p, ts_lookahead(p->ts, 0), esperado);
    return ts_next_token(p->ts);
}

static Scope *novo_escopo(Parser *p, char *nome, Scope *pai) {
    Scope *s = xmalloc(sizeof(Scope));
    *s = (Scope){nome, pai, NULL, 0, 0};
    if (p->n_escopos == p->cap_escopos) {
        p->cap_escopos = p->cap_escopos ? p->cap_escopos * 2 : 8;
        p->escopos = xrealloc(p->escopos, p->cap_escopos * sizeof(Scope *));
    }
    p->escopos[p->n_escopos++] = s;
    return s;
}

static Simbolo *procura_local(Scope *s, const char *nome) {
    for (size_t i = 0; i < s->n; i++) {
        if (strcmp(s->v[i].nome, nome) == 0) return &s->v[i];
    }
    return NULL;
}

static Simbolo *lookup(Scope *s, const char *nome) {
    for (; s; s = s->pai) {
        Simbolo *sim = procura_local(s, nome);
        if (sim) return sim;
    }
    return NULL;
}

static int compativel(Tipo destino, Tipo origem) {
    return origem == TY_ERRO || destino == origem || (destino == TY_FLOAT && origem == TY_INT);
}

static int numerico(Tipo t) { return t == TY_INT || t == TY_FLOAT; }

static Tipo tipo_de_token(TokenType t) {
    return t == TOK_KW_INT ? TY_INT : t == TOK_KW_FLOAT ? TY_FLOAT : t == TOK_KW_BOOL ? TY_BOOL : TY_ERRO;
}

/* -- expressões ----------------------------------------------------------------- */

static const TokenType NIVEIS[][4] = {
    {TOK_OR}, {TOK_AND}, {TOK_EQ, TOK_NEQ}, {TOK_LT, TOK_LE, TOK_GT, TOK_GE}, {TOK_PLUS, TOK_MINUS},
    {TOK_STAR, TOK_SLASH, TOK_PERCENT},
};
static const int N_POR_NIVEL[] = {1, 1, 2, 4, 2, 3};
#define N_NIVEIS 6

static Expr *expr(Parser *p, Scope *escopo, int nivel);

static Expr *identificador(Parser *p, const Token *t, Scope *escopo) {
    Simbolo *s = lookup(escopo, t->lexeme);
    if (!s) semantico(p, t->line, t->column, xformat("identificador '%s' não declarado", t->lexeme));
    return expr_novo(&p->prog->arena, EX_ID, t->lexeme, s ? s->tipo : TY_ERRO, t->line, t->column, NULL, NULL);
}

static Tipo tipo_binario(Parser *p, const Token *op, Tipo a, Tipo b) {
    if (a == TY_ERRO || b == TY_ERRO) return TY_ERRO;
    const char *s = op->lexeme;
    const char *exigido;
    int nums = numerico(a) && numerico(b);
    if (strcmp(s, "+") == 0 || strcmp(s, "-") == 0 || strcmp(s, "*") == 0 || strcmp(s, "/") == 0) {
        if (nums) return (a == TY_FLOAT || b == TY_FLOAT) ? TY_FLOAT : TY_INT;
        exigido = "numéricos";
    } else if (strcmp(s, "%") == 0) {
        if (a == TY_INT && b == TY_INT) return TY_INT;
        exigido = "int";
    } else if (strcmp(s, "<") == 0 || strcmp(s, "<=") == 0 || strcmp(s, ">") == 0 || strcmp(s, ">=") == 0) {
        if (nums) return TY_BOOL;
        exigido = "numéricos";
    } else if (strcmp(s, "==") == 0 || strcmp(s, "!=") == 0) {
        if (nums || (a == TY_BOOL && b == TY_BOOL)) return TY_BOOL;
        exigido = "do mesmo tipo (numéricos ou bool)";
    } else {
        if (a == TY_BOOL && b == TY_BOOL) return TY_BOOL;
        exigido = "bool";
    }
    semantico(p, op->line, op->column,
              xformat("operador '%s' exige operandos %s; recebeu %s e %s", s, exigido, NOME_TIPO[a], NOME_TIPO[b]));
    return TY_ERRO;
}

static Expr *primaria(Parser *p, Scope *escopo) {
    const Token *t = ts_lookahead(p->ts, 0);
    Tipo lit = t->type == TOK_INT                                ? TY_INT
               : t->type == TOK_FLOAT                            ? TY_FLOAT
               : (t->type == TOK_KW_TRUE || t->type == TOK_KW_FALSE) ? TY_BOOL
                                                                  : TY_ERRO;
    if (lit != TY_ERRO) {
        ts_next_token(p->ts);
        return expr_novo(&p->prog->arena, EX_LITERAL, t->lexeme, lit, t->line, t->column, NULL, NULL);
    }
    if (t->type == TOK_IDENT) return identificador(p, ts_next_token(p->ts), escopo);
    if (ts_accept(p->ts, TOK_LPAREN)) {
        Expr *e = expr(p, escopo, 0);
        exige(p, TOK_RPAREN, "operador ou ')'");
        return e;
    }
    erro_sintaxe(p, t, "expressão");
    return NULL; /* inalcançável */
}

static Expr *unaria(Parser *p, Scope *escopo) {
    const Token *op = ts_accept(p->ts, TOK_MINUS);
    if (!op) op = ts_accept(p->ts, TOK_NOT);
    if (!op) return primaria(p, escopo);
    Expr *o = unaria(p, escopo);
    Tipo tipo = TY_ERRO;
    if (o->tipo == TY_ERRO) {
        /* erro já reportado */
    } else if (op->type == TOK_MINUS && numerico(o->tipo)) {
        tipo = o->tipo;
    } else if (op->type == TOK_NOT && o->tipo == TY_BOOL) {
        tipo = TY_BOOL;
    } else {
        semantico(p, op->line, op->column,
                  xformat("operador '%s' exige operando %s; recebeu %s", op->lexeme,
                          op->type == TOK_MINUS ? "numérico" : "bool", NOME_TIPO[o->tipo]));
    }
    return expr_novo(&p->prog->arena, EX_UNARY, op->lexeme, tipo, op->line, op->column, o, NULL);
}

static int no_nivel(TokenType t, int nivel) {
    for (int i = 0; i < N_POR_NIVEL[nivel]; i++) {
        if (NIVEIS[nivel][i] == t) return 1;
    }
    return 0;
}

static Expr *expr(Parser *p, Scope *escopo, int nivel) {
    if (nivel == N_NIVEIS) return unaria(p, escopo);
    Expr *e = expr(p, escopo, nivel + 1);
    while (no_nivel(ts_lookahead(p->ts, 0)->type, nivel)) {
        const Token *op = ts_next_token(p->ts);
        Expr *dir = expr(p, escopo, nivel + 1);
        Tipo tipo = tipo_binario(p, op, e->tipo, dir->tipo);
        e = expr_novo(&p->prog->arena, EX_BINARY, op->lexeme, tipo, op->line, op->column, e, dir);
    }
    return e;
}

/* -- declarações e comandos ------------------------------------------------------ */

static void item(Parser *p, Scope *escopo, No *bloco);

/* decl_item -> id [ = expr ]; `inh` é o tipo herdado da declaração. */
static No *decl_item(Parser *p, Tipo inh, Scope *escopo) {
    const Token *t = exige(p, TOK_IDENT, "identificador");
    Expr *init = ts_accept(p->ts, TOK_ASSIGN) ? expr(p, escopo, 0) : NULL;
    if (init && !compativel(inh, init->tipo)) {
        semantico(p, init->linha, init->coluna,
                  xformat("não é possível inicializar '%s' (%s) com %s", t->lexeme, NOME_TIPO[inh],
                          NOME_TIPO[init->tipo]));
    }
    Simbolo *ant = procura_local(escopo, t->lexeme);
    if (ant) {
        semantico(p, t->line, t->column,
                  xformat("redeclaração de '%s' no escopo %s (declarado na linha %d, coluna %d)", t->lexeme,
                          escopo->nome, ant->linha, ant->coluna));
    } else {
        if (escopo->n == escopo->cap) {
            escopo->cap = escopo->cap ? escopo->cap * 2 : 8;
            escopo->v = xrealloc(escopo->v, escopo->cap * sizeof(Simbolo));
        }
        escopo->v[escopo->n++] = (Simbolo){xstrdup(t->lexeme), inh, t->line, t->column};
    }
    No *d = no_novo(&p->prog->arena, ND_VARDECL, t->line, t->column);
    d->nome = xstrdup(t->lexeme);
    d->tipo = inh;
    d->escopo = xstrdup(escopo->nome);
    d->e1 = init;
    return d;
}

static No *cmd(Parser *p, Scope *escopo) {
    const Token *t = ts_lookahead(p->ts, 0);
    Arena *ar = &p->prog->arena;
    switch (t->type) {
    case TOK_LBRACE: {
        ts_next_token(p->ts);
        Scope *interno = novo_escopo(p, xformat("bloco%d", ++p->blocos), escopo);
        No *b = no_novo(ar, ND_BLOCK, t->line, t->column);
        b->escopo = xstrdup(interno->nome);
        while (!ts_check(p->ts, TOK_RBRACE)) {
            if (ts_check(p->ts, TOK_EOF)) erro_sintaxe(p, ts_lookahead(p->ts, 0), "'}'");
            item(p, interno, b);
        }
        ts_next_token(p->ts);
        return b;
    }
    case TOK_KW_IF:
    case TOK_KW_WHILE: {
        ts_next_token(p->ts);
        exige(p, TOK_LPAREN, "'('");
        Expr *cond = expr(p, escopo, 0);
        exige(p, TOK_RPAREN, "operador ou ')'");
        const char *nome = t->type == TOK_KW_IF ? "if" : "while";
        if (cond->tipo != TY_BOOL && cond->tipo != TY_ERRO) {
            semantico(p, cond->linha, cond->coluna,
                      xformat("condição do %s deve ser bool; recebeu %s", nome, NOME_TIPO[cond->tipo]));
        }
        No *n = no_novo(ar, t->type == TOK_KW_IF ? ND_IF : ND_WHILE, t->line, t->column);
        n->e1 = cond;
        n->c1 = cmd(p, escopo);
        if (t->type == TOK_KW_IF && ts_accept(p->ts, TOK_KW_ELSE)) n->c2 = cmd(p, escopo);
        return n;
    }
    case TOK_KW_RETURN: {
        ts_next_token(p->ts);
        No *n = no_novo(ar, ND_RETURN, t->line, t->column);
        if (!ts_check(p->ts, TOK_SEMICOLON)) n->e1 = expr(p, escopo, 0);
        exige(p, TOK_SEMICOLON, "operador ou ';'");
        return n;
    }
    case TOK_IDENT: {
        Expr *alvo = identificador(p, ts_next_token(p->ts), escopo);
        exige(p, TOK_ASSIGN, "'='");
        Expr *valor = expr(p, escopo, 0);
        exige(p, TOK_SEMICOLON, "operador ou ';'");
        if (alvo->tipo != TY_ERRO && valor->tipo != TY_ERRO && !compativel(alvo->tipo, valor->tipo)) {
            semantico(p, valor->linha, valor->coluna,
                      xformat("não é possível atribuir %s a '%s' (%s)", NOME_TIPO[valor->tipo], alvo->texto,
                              NOME_TIPO[alvo->tipo]));
        }
        No *n = no_novo(ar, ND_ASSIGN, alvo->linha, alvo->coluna);
        n->e1 = alvo;
        n->e2 = valor;
        return n;
    }
    default: erro_sintaxe(p, t, "comando"); return NULL; /* inalcançável */
    }
}

/* item -> decl | cmd; acrescenta os nós em `bloco` (ou no programa, se NULL). */
static void item(Parser *p, Scope *escopo, No *bloco) {
    Tipo tipo = tipo_de_token(ts_lookahead(p->ts, 0)->type);
    if (tipo == TY_ERRO) {
        No *c = cmd(p, escopo);
        if (bloco) no_add(bloco, c);
        else programa_add(p->prog, c);
        return;
    }
    ts_next_token(p->ts); /* tipo.type, herdado por cada decl_item */
    do {
        No *d = decl_item(p, tipo, escopo);
        if (bloco) no_add(bloco, d);
        else programa_add(p->prog, d);
    } while (ts_accept(p->ts, TOK_COMMA));
    exige(p, TOK_SEMICOLON, "',' ou ';'");
}

int parse(TokenStream *ts, Programa **out, Diagnosticos *diags) {
    Parser *p = xmalloc(sizeof(Parser)); /* no heap: sobrevive intacto ao longjmp */
    *p = (Parser){.ts = ts, .diags = diags};
    p->prog = xmalloc(sizeof(Programa));
    *p->prog = (Programa){0};
    int codigo;
    if (setjmp(p->sintaxe) == 0) {
        Scope *global = novo_escopo(p, xstrdup("global"), NULL);
        while (!ts_check(ts, TOK_EOF)) item(p, global, NULL);
        *out = p->prog;
        codigo = diags->n ? ERRO_SEMANTICO : OK;
    } else {
        /* Erro de sintaxe: descarta o programa parcial e os erros semânticos
         * vistos até aqui; fica só a mensagem de sintaxe (a última). */
        programa_free(p->prog);
        char *msg = diags->v[diags->n - 1];
        for (size_t i = 0; i + 1 < diags->n; i++) free(diags->v[i]);
        diags->v[0] = msg;
        diags->n = 1;
        *out = NULL;
        codigo = ERRO_SINTAXE;
    }
    for (size_t i = 0; i < p->n_escopos; i++) {
        Scope *s = p->escopos[i];
        for (size_t k = 0; k < s->n; k++) free(s->v[k].nome);
        free(s->v), free(s->nome), free(s);
    }
    free(p->escopos);
    free(p);
    return codigo;
}
