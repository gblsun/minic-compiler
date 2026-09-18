// main.c
// Unity build: compila todos os fontes em uma única unidade de tradução.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/c/ast.c"
#include "src/c/lexer.c"
#include "src/c/parser.c"

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

static void imprimir_erros_lexicos(const Lexer *lex) {
    for (size_t i = 0; i < lex->errors_len; i++) {
        char *s = lexerror_to_string(&lex->errors[i]);
        fprintf(stderr, "%s\n", s);
        free(s);
    }
}

static void print_usage(const char *program) {
    fprintf(stderr,
            "uso: %s [--scanner|--parser] [--tree|--tokens] <arquivo>\n"
            "     %s <arquivo.mc>     => scanner\n"
            "     %s <arquivo.c>      => parser\n",
            program, program, program);
}

int main(int argc, char **argv) {
    const char *caminho = NULL;
    int force_parser = 0;
    int force_scanner = 0;
    int modo_arvore = 0;
    int modo_tokens = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--scanner") == 0) {
            force_scanner = 1;
        } else if (strcmp(argv[i], "--parser") == 0) {
            force_parser = 1;
        } else if (strcmp(argv[i], "--tree") == 0 || strcmp(argv[i], "-t") == 0) {
            modo_arvore = 1;
        } else if (strcmp(argv[i], "--tokens") == 0) {
            modo_tokens = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (caminho == NULL) {
            caminho = argv[i];
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (caminho == NULL) {
        print_usage(argv[0]);
        return 1;
    }

    int parser_mode = force_parser || (!force_scanner && strstr(caminho, ".c") != NULL);

    if (!parser_mode) {
        char *source = read_file(caminho);

        Lexer lex;
        lexer_init(&lex, source);
        lexer_tokenize(&lex);

        for (size_t i = 0; i < lex.tokens_len; i++) {
            if (lex.tokens[i].type == TOK_EOF) {
                continue;
            }
            char *s = token_to_string(&lex.tokens[i]);
            printf("%s\n", s);
            free(s);
        }

        for (size_t i = 0; i < lex.errors_len; i++) {
            char *s = lexerror_to_string(&lex.errors[i]);
            fprintf(stderr, "%s\n", s);
            free(s);
        }

        int had_errors = lex.errors_len > 0;
        lexer_free(&lex);
        free(source);
        return had_errors ? 2 : 0;
    }

    char *source = read_file(caminho);

    Lexer lex;
    lexer_init(&lex, source);
    lexer_tokenize(&lex);

    if (modo_tokens) {
        for (size_t i = 0; i < lex.tokens_len; i++) {
            if (lex.tokens[i].type == TOK_EOF) {
                continue;
            }
            char *s = token_to_string(&lex.tokens[i]);
            printf("%s\n", s);
            free(s);
        }
        imprimir_erros_lexicos(&lex);
        int houve = lex.errors_len > 0;
        lexer_free(&lex);
        free(source);
        return houve ? 2 : 0;
    }

    if (lex.errors_len > 0) {
        imprimir_erros_lexicos(&lex);
        lexer_free(&lex);
        free(source);
        return 2;
    }

    Parser parser;
    parser_init(&parser, lex.tokens, lex.tokens_len);
    Ast *programa = parser_parse(&parser);

    int codigo;
    if (parser.errors_len > 0) {
        for (size_t i = 0; i < parser.errors_len; i++) {
            char *s = parseerror_to_string(&parser.errors[i]);
            fprintf(stderr, "%s\n", s);
            free(s);
        }
        codigo = 3;
    } else {
        char *saida = modo_arvore ? ast_to_tree(programa) : ast_to_sexpr(programa);
        printf("%s\n", saida);
        free(saida);
        codigo = 0;
    }

    parser_free(&parser);
    lexer_free(&lex);
    free(source);
    return codigo;
}