/* Parser do mini pipeline (exercício 10) — equivalente C de parser.py.
 *
 * Gramática, atributos, regras semânticas e limitações: ver a docstring de
 * parser.py (as duas versões implementam exatamente o mesmo subconjunto e
 * produzem a mesma saída).
 */

#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "scanner.h"

enum { OK = 0, ERRO_USO = 1, ERRO_LEXICO = 2, ERRO_SINTAXE = 3, ERRO_SEMANTICO = 4 };

typedef struct {
    char **v; /* mensagens já formatadas, na ordem em que foram encontradas */
    size_t n, cap;
} Diagnosticos;

/* Analisa o fluxo inteiro. Devolve OK, ERRO_SINTAXE ou ERRO_SEMANTICO.
 * Em ERRO_SINTAXE, *out = NULL e diags tem a mensagem; senão *out recebe o
 * programa (liberar com programa_free) e diags os erros semânticos. */
int parse(TokenStream *ts, Programa **out, Diagnosticos *diags);

void diagnosticos_free(Diagnosticos *d);

#endif /* PARSER_H */
