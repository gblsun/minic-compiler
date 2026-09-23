/* Scanner do mini pipeline (exercício 10) — equivalente C de scanner.py.
 *
 * Não reimplementa nada: usa o lexer da etapa 1 (src/c/lexer.c) e expõe para
 * o parser um fluxo de tokens (tipo, lexema, posição) com next_token e
 * lookahead.
 */

#ifndef SCANNER_H
#define SCANNER_H

#include <stddef.h>

#include "../../../src/c/lexer.h"

typedef struct {
    Lexer lexer;
    size_t pos;
} TokenStream;

/* Tokeniza `source` inteiro (que precisa continuar válido enquanto o fluxo
 * for usado). Os erros léxicos ficam em ts->lexer.errors. */
void ts_init(TokenStream *ts, const char *source);
void ts_free(TokenStream *ts);

const Token *ts_lookahead(const TokenStream *ts, size_t k);
const Token *ts_next_token(TokenStream *ts);
int ts_check(const TokenStream *ts, TokenType tipo);
const Token *ts_accept(TokenStream *ts, TokenType tipo); /* NULL se não casar */

#endif /* SCANNER_H */
