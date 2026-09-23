/* Implementação de scanner.h. */

#include "scanner.h"

void ts_init(TokenStream *ts, const char *source) {
    lexer_init(&ts->lexer, source);
    lexer_tokenize(&ts->lexer);
    ts->pos = 0;
}

void ts_free(TokenStream *ts) { lexer_free(&ts->lexer); }

const Token *ts_lookahead(const TokenStream *ts, size_t k) {
    size_t i = ts->pos + k;
    if (i >= ts->lexer.tokens_len) {
        return &ts->lexer.tokens[ts->lexer.tokens_len - 1]; /* EOF */
    }
    return &ts->lexer.tokens[i];
}

const Token *ts_next_token(TokenStream *ts) {
    const Token *t = ts_lookahead(ts, 0);
    if (t->type != TOK_EOF) ts->pos++;
    return t;
}

int ts_check(const TokenStream *ts, TokenType tipo) { return ts_lookahead(ts, 0)->type == tipo; }

const Token *ts_accept(TokenStream *ts, TokenType tipo) {
    return ts_check(ts, tipo) ? ts_next_token(ts) : NULL;
}
