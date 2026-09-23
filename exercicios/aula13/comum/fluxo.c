/* Implementação de fluxo.h — ver o cabeçalho para o contrato. */

#include "fluxo.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *aloca(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(ERRO_USO);
    }
    return p;
}

void *realoca(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(ERRO_USO);
    }
    return q;
}

char *copia(const char *s) {
    size_t n = strlen(s) + 1;
    char *c = aloca(n);
    memcpy(c, s, n);
    return c;
}

char *formata(const char *fmt, ...) {
    va_list a, b;
    va_start(a, fmt);
    va_copy(b, a);
    int n = vsnprintf(NULL, 0, fmt, b);
    va_end(b);
    char *buf = aloca((size_t)(n < 0 ? 0 : n) + 1);
    vsnprintf(buf, (size_t)n + 1, fmt, a);
    va_end(a);
    return buf;
}

char *ler_fonte(const char *caminho) {
    FILE *arq = strcmp(caminho, "-") == 0 ? stdin : fopen(caminho, "rb");
    if (!arq) {
        fprintf(stderr, "erro ao abrir '%s': %s\n", caminho, strerror(errno));
        return NULL;
    }
    /* Leitura em blocos, que funciona também para stdin (sem fseek). */
    size_t cap = 4096, len = 0;
    char *buf = aloca(cap);
    size_t lidos;
    while ((lidos = fread(buf + len, 1, cap - len - 1, arq)) > 0) {
        len += lidos;
        if (cap - len - 1 == 0) {
            cap *= 2;
            buf = realoca(buf, cap);
        }
    }
    if (arq != stdin) {
        fclose(arq);
    }
    buf[len] = '\0';
    return buf;
}

int fluxo_abrir(Fluxo *f, const char *caminho) {
    f->fonte = ler_fonte(caminho);
    if (!f->fonte) {
        return ERRO_USO;
    }
    lexer_init(&f->lexer, f->fonte);
    lexer_tokenize(&f->lexer);
    f->pos = 0;
    if (f->lexer.errors_len > 0) {
        for (size_t i = 0; i < f->lexer.errors_len; i++) {
            char *s = lexerror_to_string(&f->lexer.errors[i]);
            fprintf(stderr, "%s\n", s);
            free(s);
        }
        fluxo_fechar(f);
        return ERRO_LEXICO;
    }
    return OK;
}

void fluxo_fechar(Fluxo *f) {
    lexer_free(&f->lexer);
    free(f->fonte);
    f->fonte = NULL;
}

const Token *fluxo_espiar(const Fluxo *f, size_t k) {
    size_t i = f->pos + k;
    if (i >= f->lexer.tokens_len) {
        return &f->lexer.tokens[f->lexer.tokens_len - 1]; /* EOF */
    }
    return &f->lexer.tokens[i];
}

const Token *fluxo_atual(const Fluxo *f) { return fluxo_espiar(f, 0); }

const Token *fluxo_proximo(Fluxo *f) {
    const Token *t = fluxo_atual(f);
    if (t->type != TOK_EOF) {
        f->pos++;
    }
    return t;
}

int fluxo_verifica(const Fluxo *f, TokenType tipo) { return fluxo_atual(f)->type == tipo; }

int fluxo_aceita(Fluxo *f, TokenType tipo) {
    if (fluxo_verifica(f, tipo)) {
        fluxo_proximo(f);
        return 1;
    }
    return 0;
}

char *descrever(const Token *t) {
    if (t->type == TOK_EOF) {
        return copia("fim da entrada");
    }
    return formata("'%s'", t->lexeme);
}

void erro_sintaxe(const Token *t, const char *esperado) {
    char *d = descrever(t);
    fprintf(stderr, "Erro de sintaxe na linha %d, coluna %d: esperado %s; encontrado %s.\n",
            t->line, t->column, esperado, d);
    free(d);
}
