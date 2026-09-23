/* CLI do mini pipeline (exercício 10): scanner -> parser -> atributos -> AST.
 *
 * Mesma interface e mesma saída da CLI de parser.py: a AST (com os atributos)
 * em stdout e os diagnósticos em stderr.
 *
 * Uso:  ./pipeline [arquivo | -]      (sem argumento: lê a entrada padrão)
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

static char *ler(const char *caminho) {
    FILE *f = strcmp(caminho, "-") == 0 ? stdin : fopen(caminho, "rb");
    if (!f) {
        fprintf(stderr, "erro ao abrir '%s': %s\n", caminho, strerror(errno));
        return NULL;
    }
    size_t cap = 4096, len = 0, lidos;
    char *buf = xmalloc(cap);
    while ((lidos = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += lidos;
        if (len + 1 == cap) buf = xrealloc(buf, cap *= 2);
    }
    if (f != stdin) fclose(f);
    buf[len] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "uso: %s [arquivo | -]\n", argv[0]);
        return ERRO_USO;
    }
    char *fonte = ler(argc == 2 ? argv[1] : "-");
    if (!fonte) return ERRO_USO;

    TokenStream ts;
    ts_init(&ts, fonte);
    int codigo;
    if (ts.lexer.errors_len > 0) {
        for (size_t i = 0; i < ts.lexer.errors_len; i++) {
            char *s = lexerror_to_string(&ts.lexer.errors[i]);
            fprintf(stderr, "%s\n", s);
            free(s);
        }
        codigo = ERRO_LEXICO;
    } else {
        Programa *prog = NULL;
        Diagnosticos diags = {0};
        codigo = parse(&ts, &prog, &diags);
        if (prog) {
            serializa(stdout, prog);
            programa_free(prog);
        }
        fflush(stdout);
        for (size_t i = 0; i < diags.n; i++) fprintf(stderr, "%s\n", diags.v[i]);
        diagnosticos_free(&diags);
    }
    ts_free(&ts);
    free(fonte);
    return codigo;
}
