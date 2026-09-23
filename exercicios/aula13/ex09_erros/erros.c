/* Exercício 09 — execução segura de ações e tratamento de erros.
 *
 * Mesma linguagem, mesmas regras de recuperação e publicação e mesma saída de
 * erros.py (ver a docstring de lá). Diferenças de mecânica em C:
 *
 * - sem exceções: cada função de parse devolve um `Status` (OK ou ST_SINTAXE)
 *   e entrega o nó por parâmetro de saída. O erro pendente (token + o que era
 *   esperado) fica guardado no Parser até o laço de itens transformá-lo em
 *   Diagnostic;
 * - em **todo** caminho de falha, a função destrói os nós temporários que já
 *   tinha construído antes de devolver o status — nenhum nó parcialmente
 *   ligado sobrevive;
 * - os construtores `mk_*` conferem as pré-condições (filhos presentes, alvo
 *   do Assign é identificador) e devolvem NULL, liberando os filhos
 *   recebidos, se alguma falhar. Como no Python, isso indicaria defeito do
 *   parser: vira um diagnóstico interno e aborta a análise.
 *
 * Uso:  ./erros <arquivo | ->
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ex06_expressoes_minic/expr.h"

/* -- diagnósticos ----------------------------------------------------------- */

typedef enum { CAT_LEXICO, CAT_SINTATICO, CAT_SEMANTICO } Categoria;
static const char *NOME_CAT[] = {"léxico", "sintático", "semântico"};

typedef struct {
    Categoria categoria;
    char *mensagem;
    char *lexema; /* NULL = fim da entrada */
    int linha, coluna;
    size_t seq; /* ordem de registro, para a ordenação ser estável */
} Diagnostic;

typedef struct {
    Diagnostic *v;
    size_t n, cap;
} Diags;

static void diag_add(Diags *d, Categoria c, char *msg, const char *lexema, int linha, int coluna) {
    if (d->n == d->cap) {
        d->cap = d->cap ? d->cap * 2 : 8;
        d->v = realoca(d->v, d->cap * sizeof(Diagnostic));
    }
    d->v[d->n] = (Diagnostic){c, msg, lexema ? copia(lexema) : NULL, linha, coluna, d->n};
    d->n++;
}

static int compara(const void *a, const void *b) {
    const Diagnostic *x = a, *y = b;
    if (x->linha != y->linha) return x->linha < y->linha ? -1 : 1;
    if (x->coluna != y->coluna) return x->coluna < y->coluna ? -1 : 1;
    return x->seq < y->seq ? -1 : (x->seq > y->seq);
}

/* -- AST -------------------------------------------------------------------- */

typedef enum { N_DECL, N_BLOCK, N_ASSIGN, N_IF, N_WHILE, N_RETURN, N_EXPR } NoKind;

typedef struct No {
    NoKind k;
    int linha, coluna;
    char *nome;        /* DECL */
    const char *tipo;  /* DECL */
    Expr *e1, *e2;     /* ASSIGN: alvo/valor; IF/WHILE: cond; RETURN/EXPR: valor */
    struct No *c1, *c2; /* IF: entao/senao; WHILE: corpo */
    struct No **itens;  /* BLOCK */
    size_t n, cap;
} No;

static void no_free(No *n) {
    if (!n) return;
    free(n->nome);
    expr_free(n->e1), expr_free(n->e2);
    no_free(n->c1), no_free(n->c2);
    for (size_t i = 0; i < n->n; i++) no_free(n->itens[i]);
    free(n->itens);
    free(n);
}

static No *no_novo(NoKind k, int linha, int coluna) {
    No *n = aloca(sizeof(No));
    *n = (No){.k = k, .linha = linha, .coluna = coluna};
    return n;
}

static void no_add(No *bloco, No *item) {
    if (bloco->n == bloco->cap) {
        bloco->cap = bloco->cap ? bloco->cap * 2 : 4;
        bloco->itens = realoca(bloco->itens, bloco->cap * sizeof(No *));
    }
    bloco->itens[bloco->n++] = item;
}

/* Construtores com pré-condições: NULL (e filhos liberados) se falharem. */
static No *mk_assign(Expr *alvo, Expr *valor) {
    if (!alvo || !valor || alvo->k != E_IDENT) {
        expr_free(alvo), expr_free(valor);
        return NULL;
    }
    No *n = no_novo(N_ASSIGN, alvo->linha, alvo->coluna);
    n->e1 = alvo, n->e2 = valor;
    return n;
}

static No *mk_if(const Token *t, Expr *cond, No *entao, No *senao) {
    if (!cond || !entao) {
        expr_free(cond), no_free(entao), no_free(senao);
        return NULL;
    }
    No *n = no_novo(N_IF, t->line, t->column);
    n->e1 = cond, n->c1 = entao, n->c2 = senao;
    return n;
}

static No *mk_while(const Token *t, Expr *cond, No *corpo) {
    if (!cond || !corpo) {
        expr_free(cond), no_free(corpo);
        return NULL;
    }
    No *n = no_novo(N_WHILE, t->line, t->column);
    n->e1 = cond, n->c1 = corpo;
    return n;
}

static No *mk_return(const Token *t, Expr *valor) { /* valor opcional */
    No *n = no_novo(N_RETURN, t->line, t->column);
    n->e1 = valor;
    return n;
}

static No *mk_expr_stmt(const Token *t, Expr *e) {
    if (!e) return NULL;
    No *n = no_novo(N_EXPR, t->line, t->column);
    n->e1 = e;
    return n;
}

/* -- parser com recuperação ------------------------------------------------ */

typedef enum { ST_OK, ST_SINTAXE, ST_INTERNO } Status;

typedef struct {
    char *nome;
    int linha, coluna;
} Simbolo;

typedef struct {
    Fluxo *fl;
    Diags *diags;
    Simbolo *sim;
    size_t n_sim, cap_sim;
    const Token *erro_tok; /* erro sintático pendente */
    const char *erro_esperado;
} Parser;

static Status falha(Parser *p, const Token *t, const char *esperado) {
    p->erro_tok = t;
    p->erro_esperado = esperado;
    return ST_SINTAXE;
}

static Status exige(Parser *p, TokenType tipo, const char *esperado, const Token **out) {
    if (!fluxo_verifica(p->fl, tipo)) return falha(p, fluxo_atual(p->fl), esperado);
    const Token *t = fluxo_proximo(p->fl);
    if (out) *out = t;
    return ST_OK;
}

static Status expressao(Parser *p, Expr **out) {
    ExprErro erro;
    *out = expr_parse(p->fl, &erro);
    return *out ? ST_OK : falha(p, erro.token, erro.esperado);
}

static const Simbolo *simbolo(const Parser *p, const char *nome) {
    for (size_t i = 0; i < p->n_sim; i++) {
        if (strcmp(p->sim[i].nome, nome) == 0) return &p->sim[i];
    }
    return NULL;
}

static void registra_sintatico(Parser *p) {
    const Token *t = p->erro_tok;
    diag_add(p->diags, CAT_SINTATICO, formata("esperado %s", p->erro_esperado),
             t->type == TOK_EOF ? NULL : t->lexeme, t->line, t->column);
}

static void sincroniza(Parser *p) {
    while (!fluxo_verifica(p->fl, TOK_EOF) && !fluxo_verifica(p->fl, TOK_RBRACE)) {
        if (fluxo_proximo(p->fl)->type == TOK_SEMICOLON) return;
    }
}

static void verifica_expr(Parser *p, const Expr *e) {
    if (!e) return;
    if (e->k == E_IDENT && !simbolo(p, e->texto)) {
        diag_add(p->diags, CAT_SEMANTICO, formata("identificador '%s' não declarado", e->texto), e->texto, e->linha,
                 e->coluna);
    }
    verifica_expr(p, e->a);
    verifica_expr(p, e->b);
}

/* Ação semântica sobre um comando completo (os itens de um bloco já foram
 * verificados um a um, ao serem lidos). */
static void verifica_nomes(Parser *p, const No *n) {
    if (!n || n->k == N_BLOCK || n->k == N_DECL) return;
    verifica_expr(p, n->e1);
    verifica_expr(p, n->e2);
    verifica_nomes(p, n->c1);
    verifica_nomes(p, n->c2);
}

static Status parse_comando(Parser *p, No **out);
static Status parse_itens(Parser *p, int dentro_de_bloco, No *destino);

static Status condicao(Parser *p, Expr **out) {
    Status s;
    if ((s = exige(p, TOK_LPAREN, "'('", NULL)) != ST_OK) return s;
    if ((s = expressao(p, out)) != ST_OK) return s;
    if ((s = exige(p, TOK_RPAREN, "operador ou ')'", NULL)) != ST_OK) {
        expr_free(*out);
        *out = NULL;
    }
    return s;
}

static Status interno(No *n, No **out) {
    *out = n;
    return n ? ST_OK : ST_INTERNO;
}

static Status parse_comando(Parser *p, No **out) {
    const Token *t = fluxo_atual(p->fl);
    Status s;
    *out = NULL;
    switch (t->type) {
    case TOK_LBRACE: {
        fluxo_proximo(p->fl);
        No *b = no_novo(N_BLOCK, t->line, t->column);
        if ((s = parse_itens(p, 1, b)) != ST_OK || (s = exige(p, TOK_RBRACE, "'}'", NULL)) != ST_OK) {
            no_free(b);
            return s;
        }
        *out = b;
        return ST_OK;
    }
    case TOK_KW_IF: {
        fluxo_proximo(p->fl);
        Expr *cond;
        No *entao, *senao = NULL;
        if ((s = condicao(p, &cond)) != ST_OK) return s;
        if ((s = parse_comando(p, &entao)) != ST_OK) {
            expr_free(cond);
            return s;
        }
        if (fluxo_aceita(p->fl, TOK_KW_ELSE) && (s = parse_comando(p, &senao)) != ST_OK) {
            expr_free(cond), no_free(entao);
            return s;
        }
        return interno(mk_if(t, cond, entao, senao), out);
    }
    case TOK_KW_WHILE: {
        fluxo_proximo(p->fl);
        Expr *cond;
        No *corpo;
        if ((s = condicao(p, &cond)) != ST_OK) return s;
        if ((s = parse_comando(p, &corpo)) != ST_OK) {
            expr_free(cond);
            return s;
        }
        return interno(mk_while(t, cond, corpo), out);
    }
    case TOK_KW_RETURN: {
        fluxo_proximo(p->fl);
        Expr *valor = NULL;
        if (!fluxo_verifica(p->fl, TOK_SEMICOLON) && (s = expressao(p, &valor)) != ST_OK) return s;
        if ((s = exige(p, TOK_SEMICOLON, "operador ou ';'", NULL)) != ST_OK) {
            expr_free(valor);
            return s;
        }
        return interno(mk_return(t, valor), out);
    }
    case TOK_RBRACE:
    case TOK_KW_ELSE:
    case TOK_EOF:
    case TOK_RPAREN:
    case TOK_SEMICOLON:
    case TOK_KW_INT:
    case TOK_KW_FLOAT:
    case TOK_KW_BOOL: return falha(p, t, "comando");
    default: break;
    }
    Expr *e;
    if ((s = expressao(p, &e)) != ST_OK) return s;
    if (fluxo_verifica(p->fl, TOK_ASSIGN)) {
        const Token *igual = fluxo_proximo(p->fl);
        if (e->k != E_IDENT) {
            expr_free(e);
            return falha(p, igual, "';' (o lado esquerdo de '=' deve ser um identificador)");
        }
        Expr *valor;
        if ((s = expressao(p, &valor)) != ST_OK) {
            expr_free(e);
            return s;
        }
        if ((s = exige(p, TOK_SEMICOLON, "operador ou ';'", NULL)) != ST_OK) {
            expr_free(e), expr_free(valor);
            return s;
        }
        return interno(mk_assign(e, valor), out);
    }
    if ((s = exige(p, TOK_SEMICOLON, "operador, '=' ou ';'", NULL)) != ST_OK) {
        expr_free(e);
        return s;
    }
    return interno(mk_expr_stmt(t, e), out);
}

static const char *tipo_decl(TokenType t) {
    return t == TOK_KW_INT ? "int" : t == TOK_KW_FLOAT ? "float" : t == TOK_KW_BOOL ? "bool" : NULL;
}

/* Um item: declaração (acrescenta VarDecls a destino) ou comando. */
static Status parse_item(Parser *p, No *destino) {
    const char *tipo = tipo_decl(fluxo_atual(p->fl)->type);
    Status s;
    if (tipo) {
        fluxo_proximo(p->fl);
        const Token **ids = NULL;
        size_t n = 0;
        do {
            const Token *id;
            if ((s = exige(p, TOK_IDENT, "identificador", &id)) != ST_OK) {
                free(ids);
                return s;
            }
            ids = realoca(ids, (n + 1) * sizeof(Token *));
            ids[n++] = id;
        } while (fluxo_aceita(p->fl, TOK_COMMA));
        if ((s = exige(p, TOK_SEMICOLON, "',' ou ';'", NULL)) != ST_OK) {
            free(ids);
            return s;
        }
        for (size_t i = 0; i < n; i++) {
            const Simbolo *ant = simbolo(p, ids[i]->lexeme);
            if (ant) {
                diag_add(p->diags, CAT_SEMANTICO,
                         formata("redeclaração de '%s' (declarado na linha %d, coluna %d)", ids[i]->lexeme,
                                 ant->linha, ant->coluna),
                         ids[i]->lexeme, ids[i]->line, ids[i]->column);
                continue;
            }
            if (p->n_sim == p->cap_sim) {
                p->cap_sim = p->cap_sim ? p->cap_sim * 2 : 8;
                p->sim = realoca(p->sim, p->cap_sim * sizeof(Simbolo));
            }
            p->sim[p->n_sim++] = (Simbolo){copia(ids[i]->lexeme), ids[i]->line, ids[i]->column};
            No *d = no_novo(N_DECL, ids[i]->line, ids[i]->column);
            d->nome = copia(ids[i]->lexeme);
            d->tipo = tipo;
            no_add(destino, d);
        }
        free(ids);
        return ST_OK;
    }
    No *cmd;
    if ((s = parse_comando(p, &cmd)) != ST_OK) return s;
    verifica_nomes(p, cmd);
    no_add(destino, cmd);
    return ST_OK;
}

static Status parse_itens(Parser *p, int dentro_de_bloco, No *destino) {
    while (!fluxo_verifica(p->fl, TOK_EOF) && !(dentro_de_bloco && fluxo_verifica(p->fl, TOK_RBRACE))) {
        if (!dentro_de_bloco && fluxo_verifica(p->fl, TOK_RBRACE)) {
            const Token *t = fluxo_proximo(p->fl);
            diag_add(p->diags, CAT_SINTATICO, copia("'}' sem '{' correspondente"), "}", t->line, t->column);
            continue;
        }
        Status s = parse_item(p, destino);
        if (s == ST_INTERNO) return s;
        if (s == ST_SINTAXE) {
            registra_sintatico(p);
            sincroniza(p);
        }
    }
    return ST_OK;
}

/* -- impressão ------------------------------------------------------------- */

static void linha_expr(int nivel, const char *rotulo, const Expr *e) {
    printf("%*s%s: ", 2 * nivel, "", rotulo);
    if (e) expr_sexpr(stdout, e, 0);
    else fputs("<ausente>", stdout);
    putchar('\n');
}

static void imprime(const No *n, int nivel, const char *rotulo) {
    printf("%*s", 2 * nivel, "");
    if (rotulo) printf("%s: ", rotulo);
    switch (n->k) {
    case N_DECL: printf("VarDecl %s : %s @%d:%d\n", n->nome, n->tipo, n->linha, n->coluna); break;
    case N_BLOCK:
        printf("Block @%d:%d%s\n", n->linha, n->coluna, n->n ? "" : " (vazio)");
        for (size_t i = 0; i < n->n; i++) imprime(n->itens[i], nivel + 1, NULL);
        break;
    case N_ASSIGN:
        printf("Assign @%d:%d\n", n->linha, n->coluna);
        linha_expr(nivel + 1, "alvo", n->e1);
        linha_expr(nivel + 1, "valor", n->e2);
        break;
    case N_IF:
        printf("If @%d:%d\n", n->linha, n->coluna);
        linha_expr(nivel + 1, "cond", n->e1);
        imprime(n->c1, nivel + 1, "entao");
        if (n->c2) imprime(n->c2, nivel + 1, "senao");
        else printf("%*ssenao: <ausente>\n", 2 * (nivel + 1), "");
        break;
    case N_WHILE:
        printf("While @%d:%d\n", n->linha, n->coluna);
        linha_expr(nivel + 1, "cond", n->e1);
        imprime(n->c1, nivel + 1, "corpo");
        break;
    case N_RETURN:
        printf("Return @%d:%d\n", n->linha, n->coluna);
        linha_expr(nivel + 1, "valor", n->e1);
        break;
    case N_EXPR:
        printf("ExprStmt @%d:%d\n", n->linha, n->coluna);
        linha_expr(nivel + 1, "expr", n->e1);
        break;
    }
}

/* Caractere (sequência UTF-8 inteira) na posição de um erro léxico. */
static char *lexema_na_posicao(const char *fonte, int linha, int coluna) {
    const char *p = fonte;
    for (int l = 1; l < linha && p; l++) {
        p = strchr(p, '\n');
        if (p) p++;
    }
    if (!p) return NULL;
    const char *fim = strchr(p, '\n');
    size_t tam = fim ? (size_t)(fim - p) : strlen(p);
    if (coluna < 1 || (size_t)coluna > tam) return NULL;
    const unsigned char *c = (const unsigned char *)p + coluna - 1;
    size_t n = 1;
    while (n < 4 && (c[n] & 0xC0) == 0x80 && *c >= 0xC0) n++;
    char *s = aloca(n + 1);
    memcpy(s, c, n);
    s[n] = '\0';
    return s;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "uso: %s <arquivo | ->\n", argv[0]);
        return ERRO_USO;
    }
    Fluxo fl;
    fl.fonte = ler_fonte(argv[1]);
    if (!fl.fonte) return ERRO_USO;
    lexer_init(&fl.lexer, fl.fonte);
    lexer_tokenize(&fl.lexer);
    fl.pos = 0;

    Diags diags = {0};
    for (size_t i = 0; i < fl.lexer.errors_len; i++) {
        const LexError *e = &fl.lexer.errors[i];
        char *lex = lexema_na_posicao(fl.fonte, e->line, e->column);
        diag_add(&diags, CAT_LEXICO, copia(e->message), lex, e->line, e->column);
        free(lex);
    }
    Parser p = {&fl, &diags, NULL, 0, 0, NULL, NULL};
    No *programa = no_novo(N_BLOCK, 0, 0);
    int codigo = OK;
    if (parse_itens(&p, 0, programa) == ST_INTERNO) {
        fprintf(stderr, "erro interno: pré-condição de ação violada\n");
        codigo = ERRO_USO;
    } else if (diags.n > 0) {
        qsort(diags.v, diags.n, sizeof(Diagnostic), compara);
        codigo = ERRO_SEMANTICO;
        for (size_t i = 0; i < diags.n; i++) {
            const Diagnostic *d = &diags.v[i];
            fprintf(stderr, "[%s] linha %d, coluna %d: %s; lexema: ", NOME_CAT[d->categoria], d->linha, d->coluna,
                    d->mensagem);
            if (d->lexema) fprintf(stderr, "'%s'\n", d->lexema);
            else fprintf(stderr, "fim da entrada\n");
            int c = d->categoria == CAT_LEXICO ? ERRO_LEXICO : d->categoria == CAT_SINTATICO ? ERRO_SINTAXE : ERRO_SEMANTICO;
            if (c < codigo) codigo = c;
        }
        printf("AST inválida: %zu diagnóstico(s); nenhuma árvore publicada.\n", diags.n);
    } else {
        printf("AST válida:\nPrograma%s\n", programa->n ? "" : " (vazio)");
        for (size_t i = 0; i < programa->n; i++) imprime(programa->itens[i], 1, NULL);
    }

    no_free(programa);
    for (size_t i = 0; i < diags.n; i++) free(diags.v[i].mensagem), free(diags.v[i].lexema);
    free(diags.v);
    for (size_t i = 0; i < p.n_sim; i++) free(p.sim[i].nome);
    free(p.sim);
    fluxo_fechar(&fl);
    return codigo;
}
