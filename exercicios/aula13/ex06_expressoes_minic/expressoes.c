/* Exercício 06 — AST de expressões MINIC respeitando precedência.
 *
 * CLI equivalente a expressoes.py: lê expressões separadas por ';' e imprime a
 * AST de cada uma em notação prefixa. O parser está em expr.c.
 *
 * Uso:  ./expressoes [--pos] <arquivo | ->
 */

#include <stdio.h>
#include <string.h>

#include "expr.h"

int main(int argc, char **argv) {
    int pos = 0, n_args = 0;
    const char *arquivo = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pos") == 0) pos = 1;
        else arquivo = argv[i], n_args++;
    }
    if (n_args != 1) {
        fprintf(stderr, "uso: %s [--pos] <arquivo | ->\n", argv[0]);
        return ERRO_USO;
    }
    Fluxo fl;
    int codigo = fluxo_abrir(&fl, arquivo);
    if (codigo != OK) return codigo;

    while (codigo == OK && !fluxo_verifica(&fl, TOK_EOF)) {
        ExprErro erro;
        Expr *e = expr_parse(&fl, &erro);
        if (!e) {
            erro_sintaxe(erro.token, erro.esperado);
            codigo = ERRO_SINTAXE;
        } else if (!fluxo_aceita(&fl, TOK_SEMICOLON)) {
            erro_sintaxe(fluxo_atual(&fl), "operador ou ';'");
            codigo = ERRO_SINTAXE;
        } else {
            expr_sexpr(stdout, e, pos);
            putchar('\n');
        }
        expr_free(e);
    }
    fluxo_fechar(&fl);
    return codigo;
}
