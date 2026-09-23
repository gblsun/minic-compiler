/* Exercício 05 — parser descendente com contexto herdado (escopos).
 *
 * Mesma gramática, mesmas regras e mesma saída de escopos.py (ver a docstring
 * de lá). Em C:
 *
 * - `Scope` tem ponteiro para o pai e um vetor dinâmico de símbolos;
 *   enter_scope(pai) cria o filho e leave_scope(s) o destrói e devolve o pai;
 * - **posse**: o Scope é dono dos seus símbolos e do seu nome, e morre no
 *   leave_scope. Por isso a AST **não aponta** para símbolos: cada nó copia o
 *   que precisa da declaração resolvida (nome do escopo, tipo, posição). Assim
 *   a árvore continua válida depois que os escopos foram destruídos;
 * - o escopo atual é sempre um parâmetro das funções do parser (nada global);
 * - em erro de sintaxe, cada parse_bloco fecha o próprio escopo antes de
 *   propagar o erro, e o main libera a AST parcial.
 *
 * Uso:  ./escopos <arquivo | ->
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comum/fluxo.h"

/* -- escopos ------------------------------------------------------------ */

typedef struct {
    char *nome;
    const char *tipo;
    int linha, coluna;
} Simbolo;

typedef struct Scope {
    char *nome;
    struct Scope *parent;
    Simbolo *v;
    size_t n, cap;
} Scope;

static Scope *enter_scope(Scope *parent, char *nome) {
    Scope *s = aloca(sizeof(Scope));
    *s = (Scope){nome, parent, NULL, 0, 0};
    return s;
}

static Scope *leave_scope(Scope *s) {
    Scope *pai = s->parent;
    for (size_t i = 0; i < s->n; i++) free(s->v[i].nome);
    free(s->v);
    free(s->nome);
    free(s);
    return pai;
}

static Simbolo *procura_local(Scope *s, const char *nome) {
    for (size_t i = 0; i < s->n; i++) {
        if (strcmp(s->v[i].nome, nome) == 0) return &s->v[i];
    }
    return NULL;
}

/* Procura no escopo atual e depois nos ancestrais; *onde recebe o escopo. */
static Simbolo *lookup(Scope *s, const char *nome, Scope **onde) {
    for (; s; s = s->parent) {
        Simbolo *sim = procura_local(s, nome);
        if (sim) {
            *onde = s;
            return sim;
        }
    }
    return NULL;
}

static void declare(Scope *s, const Token *t, const char *tipo) {
    if (s->n == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->v = realoca(s->v, s->cap * sizeof(Simbolo));
    }
    s->v[s->n++] = (Simbolo){copia(t->lexeme), tipo, t->line, t->column};
}

/* -- AST ------------------------------------------------------------------ */

typedef enum { K_BLOCK, K_DECL, K_ASSIGN, K_ID, K_NUM } Kind;

typedef struct No {
    Kind k;
    int linha, coluna;
    char *nome; /* BLOCK: nome do escopo; DECL/ID: identificador; NUM: lexema */
    /* DECL: tipo/escopo da própria declaração. ID: da declaração resolvida
     * (escopo NULL = não declarado), com a posição dela em dl:dc. */
    const char *tipo;
    char *escopo;
    int dl, dc;
    /* DECL: declaração sombreada (sombra NULL = nenhuma) e redeclaração. */
    char *sombra;
    int sl, sc;
    int redecl;
    struct No **f;
    size_t n, cap;
} No;

static No *no_novo(Kind k, int linha, int coluna, char *nome) {
    No *n = aloca(sizeof(No));
    *n = (No){.k = k, .linha = linha, .coluna = coluna, .nome = nome};
    return n;
}

static void no_filho(No *pai, No *f) {
    if (pai->n == pai->cap) {
        pai->cap = pai->cap ? pai->cap * 2 : 4;
        pai->f = realoca(pai->f, pai->cap * sizeof(No *));
    }
    pai->f[pai->n++] = f;
}

static void no_free(No *n) {
    if (!n) return;
    for (size_t i = 0; i < n->n; i++) no_free(n->f[i]);
    free(n->f), free(n->nome), free(n->escopo), free(n->sombra), free(n);
}

static void imprime(const No *n, int nivel) {
    printf("%*s", 2 * nivel, "");
    switch (n->k) {
    case K_BLOCK:
        printf("Block escopo=%s\n", n->nome);
        for (size_t i = 0; i < n->n; i++) imprime(n->f[i], nivel + 1);
        break;
    case K_DECL:
        printf("VarDecl %s : %s @%d:%d [%s]", n->nome, n->tipo, n->linha, n->coluna, n->escopo);
        if (n->sombra) printf(" sombreia %s de %s (%d:%d)", n->nome, n->sombra, n->sl, n->sc);
        if (n->redecl) printf(" redeclaração");
        putchar('\n');
        break;
    case K_ASSIGN:
        printf("Assign @%d:%d\n", n->linha, n->coluna);
        imprime(n->f[0], nivel + 1);
        imprime(n->f[1], nivel + 1);
        break;
    case K_ID:
        printf("Id %s @%d:%d -> ", n->nome, n->linha, n->coluna);
        if (n->escopo) printf("%s [%s, declarado em %d:%d]\n", n->tipo, n->escopo, n->dl, n->dc);
        else printf("não declarado\n");
        break;
    case K_NUM: printf("Num %s @%d:%d\n", n->nome, n->linha, n->coluna); break;
    }
}

/* -- parser --------------------------------------------------------------- */

typedef struct {
    Fluxo *fl;
    char **erros;
    size_t n_erros;
    int blocos;
} Parser;

static void erro_sem(Parser *p, char *msg) {
    p->erros = realoca(p->erros, (p->n_erros + 1) * sizeof(char *));
    p->erros[p->n_erros++] = msg;
}

static const Token *exige(Parser *p, TokenType tipo, const char *esperado) {
    if (fluxo_verifica(p->fl, tipo)) return fluxo_proximo(p->fl);
    erro_sintaxe(fluxo_atual(p->fl), esperado);
    return NULL;
}

static const char *tipo_de(TokenType t) {
    switch (t) {
    case TOK_KW_INT: return "int";
    case TOK_KW_FLOAT: return "float";
    case TOK_KW_CHAR: return "char";
    case TOK_KW_BOOL: return "bool";
    default: return NULL;
    }
}

static No *declara(Parser *p, const Token *t, const char *tipo, Scope *escopo) {
    No *d = no_novo(K_DECL, t->line, t->column, copia(t->lexeme));
    d->tipo = tipo;
    d->escopo = copia(escopo->nome);
    Scope *onde = NULL;
    Simbolo *sombra = escopo->parent ? lookup(escopo->parent, t->lexeme, &onde) : NULL;
    Simbolo *anterior = procura_local(escopo, t->lexeme);
    if (anterior) {
        erro_sem(p, formata("Erro semântico na linha %d, coluna %d: redeclaração de '%s' no escopo %s "
                            "(declarado na linha %d, coluna %d).",
                            t->line, t->column, t->lexeme, escopo->nome, anterior->linha, anterior->coluna));
        d->redecl = 1;
        return d;
    }
    if (sombra) {
        d->sombra = copia(onde->nome);
        d->sl = sombra->linha;
        d->sc = sombra->coluna;
    }
    declare(escopo, t, tipo);
    return d;
}

static No *resolve(Parser *p, const Token *t, Scope *escopo) {
    No *id = no_novo(K_ID, t->line, t->column, copia(t->lexeme));
    Scope *onde = NULL;
    Simbolo *s = lookup(escopo, t->lexeme, &onde);
    if (s) {
        id->tipo = s->tipo;
        id->escopo = copia(onde->nome);
        id->dl = s->linha;
        id->dc = s->coluna;
    } else {
        erro_sem(p, formata("Erro semântico na linha %d, coluna %d: identificador '%s' não declarado.", t->line,
                            t->column, t->lexeme));
    }
    return id;
}

static int parse_item(Parser *p, Scope *escopo, No *destino);

static int parse_decl(Parser *p, Scope *escopo, No *destino) {
    const char *tipo = tipo_de(fluxo_proximo(p->fl)->type);
    const Token *id = exige(p, TOK_IDENT, "identificador");
    if (!id) return ERRO_SINTAXE;
    no_filho(destino, declara(p, id, tipo, escopo));
    while (fluxo_aceita(p->fl, TOK_COMMA)) {
        if (!(id = exige(p, TOK_IDENT, "identificador"))) return ERRO_SINTAXE;
        no_filho(destino, declara(p, id, tipo, escopo));
    }
    return exige(p, TOK_SEMICOLON, "',' ou ';'") ? OK : ERRO_SINTAXE;
}

static int parse_bloco(Parser *p, Scope *pai, No *destino) {
    fluxo_proximo(p->fl); /* '{' (conferido por parse_item) */
    Scope *escopo = enter_scope(pai, formata("bloco%d", ++p->blocos));
    No *bloco = no_novo(K_BLOCK, 0, 0, copia(escopo->nome));
    no_filho(destino, bloco); /* já pendurado: o main libera tudo em caso de erro */
    int codigo = OK;
    while (codigo == OK && !fluxo_verifica(p->fl, TOK_RBRACE)) {
        if (fluxo_verifica(p->fl, TOK_EOF)) {
            erro_sintaxe(fluxo_atual(p->fl), "'}'");
            codigo = ERRO_SINTAXE;
        } else {
            codigo = parse_item(p, escopo, bloco);
        }
    }
    if (codigo == OK) fluxo_proximo(p->fl);
    leave_scope(escopo);
    return codigo;
}

static int parse_uso(Parser *p, Scope *escopo, No *destino) {
    const Token *t = fluxo_proximo(p->fl);
    No *a = no_novo(K_ASSIGN, t->line, t->column, NULL);
    no_filho(destino, a);
    no_filho(a, resolve(p, t, escopo));
    if (!exige(p, TOK_ASSIGN, "'='")) return ERRO_SINTAXE;
    const Token *v = fluxo_atual(p->fl);
    if (v->type == TOK_IDENT) {
        no_filho(a, resolve(p, fluxo_proximo(p->fl), escopo));
    } else if (v->type == TOK_INT) {
        fluxo_proximo(p->fl);
        no_filho(a, no_novo(K_NUM, v->line, v->column, copia(v->lexeme)));
    } else {
        erro_sintaxe(v, "identificador ou número");
        return ERRO_SINTAXE;
    }
    return exige(p, TOK_SEMICOLON, "';'") ? OK : ERRO_SINTAXE;
}

static int parse_item(Parser *p, Scope *escopo, No *destino) {
    const Token *t = fluxo_atual(p->fl);
    if (tipo_de(t->type)) return parse_decl(p, escopo, destino);
    if (t->type == TOK_LBRACE) return parse_bloco(p, escopo, destino);
    if (t->type == TOK_IDENT) return parse_uso(p, escopo, destino);
    erro_sintaxe(t, "declaração, bloco ou atribuição");
    return ERRO_SINTAXE;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "uso: %s <arquivo | ->\n", argv[0]);
        return ERRO_USO;
    }
    Fluxo fl;
    int codigo = fluxo_abrir(&fl, argv[1]);
    if (codigo != OK) return codigo;

    Parser p = {&fl, NULL, 0, 0};
    Scope *global = enter_scope(NULL, copia("global"));
    No *raiz = no_novo(K_BLOCK, 0, 0, copia("global"));
    while (codigo == OK && !fluxo_verifica(&fl, TOK_EOF)) {
        codigo = parse_item(&p, global, raiz);
    }
    leave_scope(global);
    if (codigo == OK) {
        imprime(raiz, 0);
        for (size_t i = 0; i < p.n_erros; i++) fprintf(stderr, "%s\n", p.erros[i]);
        codigo = p.n_erros ? ERRO_SEMANTICO : OK;
    }
    for (size_t i = 0; i < p.n_erros; i++) free(p.erros[i]);
    free(p.erros);
    no_free(raiz);
    fluxo_fechar(&fl);
    return codigo;
}
