/* Ponto de entrada do analisador léxico da linguagem MINIC.
 *
 * Uso: ./scanner <arquivo.mc>
 *
 * Lê o arquivo-fonte inteiro, roda o lexer sobre ele e imprime:
 *   - os tokens reconhecidos (na saída padrão), no formato pedido pela
 *     especificação (<TOKEN, lexema> para tokens "com atributo", ou só o
 *     nome do token para os demais);
 *   - os erros léxicos (na saída de erro), no formato da Seção 12 da
 *     especificação.
 *
 * O lexer não para no primeiro erro: ele tenta reconhecer o arquivo
 * inteiro e reporta todos os problemas de uma vez só.
 */

#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"

/* Lê o conteúdo inteiro de um arquivo para uma string terminada em '\0',
 * alocada dinamicamente. Quem chama é responsável por dar free() nela. */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "erro: não foi possível abrir \"%s\": ", path);
        perror("");
        exit(1);
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "erro: falha ao ler \"%s\"\n", path);
        fclose(f);
        exit(1);
    }
    long size = ftell(f);
    if (size < 0) {
        fprintf(stderr, "erro: falha ao ler \"%s\"\n", path);
        fclose(f);
        exit(1);
    }
    rewind(f);

    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fprintf(stderr, "erro: memória insuficiente para ler \"%s\"\n", path);
        fclose(f);
        exit(1);
    }

    size_t read = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[read] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "uso: %s <arquivo.mc>\n", argv[0]);
        return 1;
    }

    char *source = read_file(argv[1]);

    Lexer lex;
    lexer_init(&lex, source);
    lexer_tokenize(&lex);

    /* Imprime os tokens, filtrando o EOF final: ele existe para uso
     * interno do compilador (ex.: o parser), mas não faz parte da saída
     * visível ao usuário. */
    for (size_t i = 0; i < lex.tokens_len; i++) {
        if (lex.tokens[i].type == TOK_EOF) {
            continue;
        }
        char *s = token_to_string(&lex.tokens[i]);
        printf("%s\n", s);
        free(s);
    }

    /* Erros léxicos vão para stderr, separados dos tokens em stdout. */
    for (size_t i = 0; i < lex.errors_len; i++) {
        char *s = lexerror_to_string(&lex.errors[i]);
        fprintf(stderr, "%s\n", s);
        free(s);
    }

    int had_errors = lex.errors_len > 0;

    lexer_free(&lex);
    free(source);

    /* Seção 11.1 da especificação: 0 quando não há erro léxico, 2 quando há
     * (1 fica reservado para erros de uso, como acima). Mesma convenção do
     * main.py, para que os exit codes das duas implementações batam. */
    return had_errors ? 2 : 0;
}