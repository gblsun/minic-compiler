/* Exercício 07 — ações semânticas para comandos e fluxo de controle.
 *
 * Mesma gramática, mesmos nós, mesmas invariantes e mesma saída de
 * comandos.py (ver a docstring de lá). As expressões vêm de
 * ex06_expressoes_minic/expr.c.
 *
 * Em C, `Cmd` é uma struct discriminada por `CmdKind`, com um construtor por
 * comando. Posse: cada Cmd é dono das suas expressões e dos seus comandos
 * filhos; cmd_free destrói a subárvore recursivamente. Em erro de sintaxe, cada
 * função de parse libera o que já construiu antes de devolver NULL.
 *
 * Uso:  ./comandos <arquivo | ->
 */

#include <stdio.h>
#include <stdlib.h>

#include "../ex06_expressoes_minic/expr.h"

typedef enum { C_BLOCK, C_ASSIGN, C_IF, C_WHILE, C_RETURN, C_EXPR } CmdKind;

typedef struct Cmd {
    CmdKind k;
    int linha, coluna;
    Expr *e1;          /* ASSIGN: alvo; IF/WHILE: cond; RETURN: valor (ou NULL); EXPR: expr */
    Expr *e2;          /* ASSIGN: valor */
    struct Cmd *c1;    /* IF: entao; WHILE: corpo */
    struct Cmd *c2;    /* IF: senao (NULL = sem else) */
    struct Cmd **lista; /* BLOCK: comandos, na ordem do texto */
    size_t n, cap;
} Cmd;

static void cmd_free(Cmd *c) {
    if (!c) return;
    expr_free(c->e1);
    expr_free(c->e2);
    cmd_free(c->c1);
    cmd_free(c->c2);
    for (size_t i = 0; i < c->n; i++) cmd_free(c->lista[i]);
    free(c->lista);
    free(c);
}

/* -- construtores --------------------------------------------------------- */

static Cmd *cmd_novo(CmdKind k, int linha, int coluna) {
    Cmd *c = aloca(sizeof(Cmd));
    *c = (Cmd){.k = k, .linha = linha, .coluna = coluna};
    return c;
}

static void cmd_block_add(Cmd *bloco, Cmd *c) {
    if (bloco->n == bloco->cap) {
        bloco->cap = bloco->cap ? bloco->cap * 2 : 4;
        bloco->lista = realoca(bloco->lista, bloco->cap * sizeof(Cmd *));
    }
    bloco->lista[bloco->n++] = c;
}

static Cmd *cmd_assign(Expr *alvo, Expr *valor) {
    Cmd *c = cmd_novo(C_ASSIGN, alvo->linha, alvo->coluna);
    c->e1 = alvo, c->e2 = valor;
    return c;
}

static Cmd *cmd_if(const Token *t, Expr *cond, Cmd *entao, Cmd *senao) {
    Cmd *c = cmd_novo(C_IF, t->line, t->column);
    c->e1 = cond, c->c1 = entao, c->c2 = senao;
    return c;
}

static Cmd *cmd_while(const Token *t, Expr *cond, Cmd *corpo) {
    Cmd *c = cmd_novo(C_WHILE, t->line, t->column);
    c->e1 = cond, c->c1 = corpo;
    return c;
}

static Cmd *cmd_return(const Token *t, Expr *valor) {
    Cmd *c = cmd_novo(C_RETURN, t->line, t->column);
    c->e1 = valor;
    return c;
}

static Cmd *cmd_expr(const Token *t, Expr *e) {
    Cmd *c = cmd_novo(C_EXPR, t->line, t->column);
    c->e1 = e;
    return c;
}

/* -- parser ---------------------------------------------------------------- */

static int exige(Fluxo *fl, TokenType tipo, const char *esperado) {
    if (fluxo_aceita(fl, tipo)) return 1;
    erro_sintaxe(fluxo_atual(fl), esperado);
    return 0;
}

static Expr *expressao(Fluxo *fl) {
    ExprErro erro;
    Expr *e = expr_parse(fl, &erro);
    if (!e) erro_sintaxe(erro.token, erro.esperado);
    return e;
}

static Expr *condicao(Fluxo *fl) {
    if (!exige(fl, TOK_LPAREN, "'('")) return NULL;
    Expr *e = expressao(fl);
    if (e && !exige(fl, TOK_RPAREN, "operador ou ')'")) {
        expr_free(e);
        return NULL;
    }
    return e;
}

static Cmd *parse_comando(Fluxo *fl);

static Cmd *parse_bloco(Fluxo *fl) {
    const Token *abre = fluxo_proximo(fl); /* '{' */
    Cmd *b = cmd_novo(C_BLOCK, abre->line, abre->column);
    while (!fluxo_verifica(fl, TOK_RBRACE)) {
        if (fluxo_verifica(fl, TOK_EOF)) {
            erro_sintaxe(fluxo_atual(fl), "'}'");
            cmd_free(b);
            return NULL;
        }
        Cmd *c = parse_comando(fl);
        if (!c) {
            cmd_free(b);
            return NULL;
        }
        cmd_block_add(b, c);
    }
    fluxo_proximo(fl);
    return b;
}

static Cmd *parse_comando(Fluxo *fl) {
    const Token *t = fluxo_atual(fl);
    switch (t->type) {
    case TOK_LBRACE: return parse_bloco(fl);
    case TOK_KW_IF: {
        fluxo_proximo(fl);
        Expr *cond = condicao(fl);
        if (!cond) return NULL;
        Cmd *entao = parse_comando(fl);
        if (!entao) {
            expr_free(cond);
            return NULL;
        }
        Cmd *senao = NULL;
        if (fluxo_aceita(fl, TOK_KW_ELSE) && !(senao = parse_comando(fl))) {
            expr_free(cond), cmd_free(entao);
            return NULL;
        }
        return cmd_if(t, cond, entao, senao);
    }
    case TOK_KW_WHILE: {
        fluxo_proximo(fl);
        Expr *cond = condicao(fl);
        if (!cond) return NULL;
        Cmd *corpo = parse_comando(fl);
        if (!corpo) {
            expr_free(cond);
            return NULL;
        }
        return cmd_while(t, cond, corpo);
    }
    case TOK_KW_RETURN: {
        fluxo_proximo(fl);
        Expr *valor = NULL;
        if (!fluxo_verifica(fl, TOK_SEMICOLON) && !(valor = expressao(fl))) return NULL;
        if (!exige(fl, TOK_SEMICOLON, "operador ou ';'")) {
            expr_free(valor);
            return NULL;
        }
        return cmd_return(t, valor);
    }
    case TOK_RBRACE:
    case TOK_KW_ELSE:
    case TOK_EOF:
    case TOK_RPAREN:
    case TOK_SEMICOLON: erro_sintaxe(t, "comando"); return NULL;
    default: break;
    }
    Expr *e = expressao(fl);
    if (!e) return NULL;
    if (fluxo_verifica(fl, TOK_ASSIGN)) {
        const Token *igual = fluxo_proximo(fl);
        if (e->k != E_IDENT) {
            erro_sintaxe(igual, "';' (o lado esquerdo de '=' deve ser um identificador)");
            expr_free(e);
            return NULL;
        }
        Expr *valor = expressao(fl);
        if (!valor) {
            expr_free(e);
            return NULL;
        }
        if (!exige(fl, TOK_SEMICOLON, "operador ou ';'")) {
            expr_free(e), expr_free(valor);
            return NULL;
        }
        return cmd_assign(e, valor);
    }
    if (!exige(fl, TOK_SEMICOLON, "operador, '=' ou ';'")) {
        expr_free(e);
        return NULL;
    }
    return cmd_expr(t, e);
}

/* -- impressão ------------------------------------------------------------- */

static void linha_expr(int nivel, const char *rotulo, const Expr *e) {
    printf("%*s%s: ", 2 * nivel, "", rotulo);
    if (e) expr_sexpr(stdout, e, 0);
    else fputs("<ausente>", stdout);
    putchar('\n');
}

static void imprime(const Cmd *c, int nivel, const char *rotulo) {
    printf("%*s", 2 * nivel, "");
    if (rotulo) printf("%s: ", rotulo);
    switch (c->k) {
    case C_BLOCK:
        printf("Block @%d:%d%s\n", c->linha, c->coluna, c->n ? "" : " (vazio)");
        for (size_t i = 0; i < c->n; i++) imprime(c->lista[i], nivel + 1, NULL);
        break;
    case C_ASSIGN:
        printf("Assign @%d:%d\n", c->linha, c->coluna);
        linha_expr(nivel + 1, "alvo", c->e1);
        linha_expr(nivel + 1, "valor", c->e2);
        break;
    case C_IF:
        printf("If @%d:%d\n", c->linha, c->coluna);
        linha_expr(nivel + 1, "cond", c->e1);
        imprime(c->c1, nivel + 1, "entao");
        if (c->c2) imprime(c->c2, nivel + 1, "senao");
        else printf("%*ssenao: <ausente>\n", 2 * (nivel + 1), "");
        break;
    case C_WHILE:
        printf("While @%d:%d\n", c->linha, c->coluna);
        linha_expr(nivel + 1, "cond", c->e1);
        imprime(c->c1, nivel + 1, "corpo");
        break;
    case C_RETURN:
        printf("Return @%d:%d\n", c->linha, c->coluna);
        linha_expr(nivel + 1, "valor", c->e1);
        break;
    case C_EXPR:
        printf("ExprStmt @%d:%d\n", c->linha, c->coluna);
        linha_expr(nivel + 1, "expr", c->e1);
        break;
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "uso: %s <arquivo | ->\n", argv[0]);
        return ERRO_USO;
    }
    Fluxo fl;
    int codigo = fluxo_abrir(&fl, argv[1]);
    if (codigo != OK) return codigo;

    Cmd *programa = cmd_novo(C_BLOCK, 0, 0); /* só um contêiner para a lista */
    while (codigo == OK && !fluxo_verifica(&fl, TOK_EOF)) {
        Cmd *c = parse_comando(&fl);
        if (c) cmd_block_add(programa, c);
        else codigo = ERRO_SINTAXE;
    }
    if (codigo == OK) {
        printf("Programa%s\n", programa->n ? "" : " (vazio)");
        for (size_t i = 0; i < programa->n; i++) imprime(programa->lista[i], 1, NULL);
    }
    cmd_free(programa);
    fluxo_fechar(&fl);
    return codigo;
}
