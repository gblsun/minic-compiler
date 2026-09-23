/* Ponto de entrada do analisador sintático da linguagem MINIC.
 *
 * Uso: ./parser <arquivo.c>
 *      ./parser --tree <arquivo.c>     (AST indentada)
 *      ./parser --tokens <arquivo.c>   (só os tokens, para depuração)
 *
 * Mesmo contrato de src/python/parser.py, para que as duas implementações
 * possam ser comparadas byte a byte:
 *   - stdout: a AST, quando o programa é sintaticamente válido;
 *   - stderr: os diagnósticos ("Erro de sintaxe na linha L, coluna C: ...");
 *   - código de saída: 0 = aceito, 1 = erro de uso, 2 = erro léxico,
 *     3 = erro sintático (Seção 11.1 da especificação).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "lexer.h"
#include "parser.h"

/* Lê o conteúdo inteiro de um arquivo para uma string terminada em '\0'.
 * Mesma função de main.c (o scanner) — as duas CLIs são compiladas em
 * executáveis separados, então cada uma carrega a sua. */
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

int main(int argc, char **argv) {
    const char *caminho = NULL;
    int modo_arvore = 0;
    int modo_tokens = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tree") == 0 || strcmp(argv[i], "-t") == 0) {
            modo_arvore = 1;
        } else if (strcmp(argv[i], "--tokens") == 0) {
            modo_tokens = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("uso: %s [--tree|--tokens] <arquivo.c>\n", argv[0]);
            return 0;
        } else if (caminho == NULL) {
            caminho = argv[i];
        } else {
            fprintf(stderr, "uso: %s [--tree|--tokens] <arquivo.c>\n", argv[0]);
            return 1;
        }
    }

    if (caminho == NULL) {
        fprintf(stderr, "uso: %s [--tree|--tokens] <arquivo.c>\n", argv[0]);
        return 1;
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

    /* Com erro léxico o parser não roda: o fluxo de tokens já está corrompido
     * e só produziria erros sintáticos em cascata, escondendo a causa real. */
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
        /* Entrada rejeitada: nenhuma AST é impressa. */
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
