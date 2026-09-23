/* Implementação de expr.h — mesma gramática e mesmas ações de expr.py. */

#include "expr.h"

#include <stdlib.h>
#include <string.h>

/* Níveis binários, do mais fraco ao mais forte (igual a NIVEIS em expr.py). */
static const TokenType NIVEIS[][4] = {
    {TOK_OR},
    {TOK_AND},
    {TOK_EQ, TOK_NEQ},
    {TOK_LT, TOK_LE, TOK_GT, TOK_GE},
    {TOK_PLUS, TOK_MINUS},
    {TOK_STAR, TOK_SLASH, TOK_PERCENT},
};
static const int N_POR_NIVEL[] = {1, 1, 2, 4, 2, 3};
#define N_NIVEIS 6

static int operador_valido(const char *op) {
    static const char *const validos[] = {"||", "&&", "==", "!=", "<", "<=", ">",
                                          ">=", "+",  "-",  "*",  "/", "%",  "!"};
    for (size_t i = 0; i < sizeof validos / sizeof *validos; i++) {
        if (strcmp(op, validos[i]) == 0) return 1;
    }
    return 0;
}

static Expr *novo(ExprKind k, const Token *t) {
    Expr *e = aloca(sizeof(Expr));
    *e = (Expr){k, t->line, t->column, copia(t->lexeme), NULL, NULL};
    return e;
}

Expr *expr_folha(ExprKind k, const Token *t) { return novo(k, t); }

Expr *expr_unaria(const Token *op, Expr *operando) {
    if (!operando || !operador_valido(op->lexeme)) {
        expr_free(operando);
        return NULL;
    }
    Expr *e = novo(E_UNARY, op);
    e->a = operando;
    return e;
}

Expr *expr_binaria(const Token *op, Expr *esq, Expr *dir) {
    if (!esq || !dir || !operador_valido(op->lexeme)) {
        expr_free(esq), expr_free(dir);
        return NULL;
    }
    Expr *e = novo(E_BINARY, op);
    e->a = esq;
    e->b = dir;
    return e;
}

void expr_free(Expr *e) {
    if (!e) return;
    expr_free(e->a);
    expr_free(e->b);
    free(e->texto);
    free(e);
}

static int no_nivel(TokenType t, int nivel) {
    for (int i = 0; i < N_POR_NIVEL[nivel]; i++) {
        if (NIVEIS[nivel][i] == t) return 1;
    }
    return 0;
}

static Expr *falha(ExprErro *erro, const Token *t, const char *esperado) {
    erro->token = t;
    erro->esperado = esperado;
    return NULL;
}

static Expr *binario(Fluxo *fl, int nivel, ExprErro *erro);

static Expr *primaria(Fluxo *fl, ExprErro *erro) {
    const Token *t = fluxo_atual(fl);
    switch (t->type) {
    case TOK_IDENT: fluxo_proximo(fl); return expr_folha(E_IDENT, t);
    case TOK_INT: fluxo_proximo(fl); return expr_folha(E_INT, t);
    case TOK_FLOAT: fluxo_proximo(fl); return expr_folha(E_FLOAT, t);
    case TOK_KW_TRUE:
    case TOK_KW_FALSE: fluxo_proximo(fl); return expr_folha(E_BOOL, t);
    case TOK_LPAREN: {
        fluxo_proximo(fl);
        Expr *e = binario(fl, 0, erro);
        if (!e) return NULL;
        if (!fluxo_verifica(fl, TOK_RPAREN)) {
            expr_free(e);
            return falha(erro, fluxo_atual(fl), "')'");
        }
        fluxo_proximo(fl);
        return e;
    }
    default: return falha(erro, t, "expressão");
    }
}

static Expr *unaria(Fluxo *fl, ExprErro *erro) {
    if (fluxo_verifica(fl, TOK_MINUS) || fluxo_verifica(fl, TOK_NOT)) {
        const Token *op = fluxo_proximo(fl);
        Expr *o = unaria(fl, erro);
        if (!o) return NULL;
        Expr *e = expr_unaria(op, o);
        return e ? e : falha(erro, op, "operador reconhecido");
    }
    return primaria(fl, erro);
}

static Expr *binario(Fluxo *fl, int nivel, ExprErro *erro) {
    if (nivel == N_NIVEIS) return unaria(fl, erro);
    Expr *e = binario(fl, nivel + 1, erro);
    while (e && no_nivel(fluxo_atual(fl)->type, nivel)) {
        const Token *op = fluxo_proximo(fl);
        Expr *dir = binario(fl, nivel + 1, erro);
        if (!dir) {
            expr_free(e);
            return NULL;
        }
        e = expr_binaria(op, e, dir);
        if (!e) return falha(erro, op, "operador reconhecido");
    }
    return e;
}

Expr *expr_parse(Fluxo *fl, ExprErro *erro) { return binario(fl, 0, erro); }

void expr_sexpr(FILE *saida, const Expr *e, int pos) {
    switch (e->k) {
    case E_UNARY:
        fprintf(saida, "(%s ", e->texto);
        expr_sexpr(saida, e->a, pos);
        fputc(')', saida);
        return;
    case E_BINARY:
        fprintf(saida, "(%s ", e->texto);
        expr_sexpr(saida, e->a, pos);
        fputc(' ', saida);
        expr_sexpr(saida, e->b, pos);
        fputc(')', saida);
        return;
    default:
        fputs(e->texto, saida);
        if (pos) fprintf(saida, "@%d:%d", e->linha, e->coluna);
    }
}
