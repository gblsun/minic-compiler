/* Parser de expressões MINIC que constrói a AST durante o reconhecimento.
 *
 * Equivalente C de expr.py (ver lá a gramática, a precedência e as ações).
 * Módulo do exercício 06, reaproveitado pelos exercícios 07, 08 e 09.
 *
 * Consome tokens pela interface next_token/lookahead de comum/fluxo.h; os nós
 * são alocados dinamicamente e cada nó é dono dos seus filhos (expr_free
 * libera a subárvore).
 */

#ifndef EXPR_H
#define EXPR_H

#include <stdio.h>

#include "../comum/fluxo.h"

typedef enum { E_IDENT, E_INT, E_FLOAT, E_BOOL, E_UNARY, E_BINARY } ExprKind;

typedef struct Expr {
    ExprKind k;
    int linha, coluna; /* folha: posição do token; interno: do operador */
    char *texto;       /* folha: lexema; interno: o operador ("+", "&&", ...) */
    struct Expr *a;    /* UNARY: operando; BINARY: filho esquerdo */
    struct Expr *b;    /* BINARY: filho direito */
} Expr;

/* Erro de sintaxe encontrado pelo parser de expressões: o chamador decide se
 * imprime (erro_sintaxe) ou se transforma num diagnóstico. */
typedef struct {
    const Token *token;
    const char *esperado;
} ExprErro;

/* Analisa uma expressão a partir do token atual. Devolve NULL em erro de
 * sintaxe, com *erro preenchido e nenhum nó vazado. */
Expr *expr_parse(Fluxo *fl, ExprErro *erro);

/* Construtores com pré-condições: devolvem NULL se um filho for NULL ou o
 * operador não for reconhecido — e, nesse caso, liberam os filhos recebidos. */
Expr *expr_folha(ExprKind k, const Token *t);
Expr *expr_unaria(const Token *op, Expr *operando);
Expr *expr_binaria(const Token *op, Expr *esq, Expr *dir);

void expr_free(Expr *e);

/* Serialização prefixa totalmente parentizada; pos = 1 acrescenta @linha:coluna
 * às folhas. */
void expr_sexpr(FILE *saida, const Expr *e, int pos);

#endif /* EXPR_H */
