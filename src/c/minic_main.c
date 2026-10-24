/* Compilador MINIC até a representação intermediária — a CLI do pipeline.
 *
 * Uso: ./minic [--ir] [-O0|-O1] <arquivo.c>
 *
 * Mesmo contrato de src/python/minic.py, para as duas implementações serem
 * comparadas byte a byte: lexer -> parser -> analisador semântico -> IR ->
 * otimizador (só com -O1), cada fase só rodando se a anterior terminou sem
 * erro.
 *   - stdout, com --ir e programa válido: a IR de três endereços (linhas com
 *     LF);
 *   - stdout, nos outros casos: o relatório da análise semântica
 *     (diagnósticos + resumo, linhas separadas por CRLF e sem quebra no fim,
 *     formato da AP 2);
 *   - stderr: erros léxicos ou sintáticos, quando houver;
 *   - código de saída: 0 = aceito, 1 = uso inválido, 2 = erro léxico,
 *     3 = erro sintático, 4 = erro semântico (Seção 11.1 da especificação).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "ast.h"
#include "ir.h"
#include "lexer.h"
#include "otimizador.h"
#include "parser.h"
#include "semantico.h"

/* Lê o arquivo inteiro para uma string terminada em '\0' (mesma função de
 * parser_main.c; cada CLI é um executável à parte e carrega a sua). */
static char *minic_ler_arquivo(const char *path) {
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
    size_t lidos = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[lidos] = '\0';
    return buf;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    /* Sem tradução de "\n" para "\r\n": o relatório já traz os CRLF do
     * gabarito, e as mensagens de erro saem com LF, como no Linux. */
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        printf("uso: %s [--ir] [-O0|-O1] <arquivo.c>\n", argv[0]);
        return 0;
    }
    int modo_ir = 0;
    int otimizar = 0;
    const char *arquivo = NULL;
    int arquivos = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ir") == 0) {
            modo_ir = 1;
        } else if (strcmp(argv[i], "-O0") == 0 || strcmp(argv[i], "-O1") == 0) {
            otimizar = strcmp(argv[i], "-O1") == 0;
        } else {
            arquivo = argv[i];
            arquivos++;
        }
    }
    if (arquivos != 1 || arquivo[0] == '-') {
        fprintf(stderr, "uso: %s [--ir] [-O0|-O1] <arquivo.c>\n", argv[0]);
        return 1;
    }

    char *source = minic_ler_arquivo(arquivo);

    Lexer lex;
    lexer_init(&lex, source);
    lexer_tokenize(&lex);
    if (lex.errors_len > 0) {
        for (size_t i = 0; i < lex.errors_len; i++) {
            char *s = lexerror_to_string(&lex.errors[i]);
            fprintf(stderr, "%s\n", s);
            free(s);
        }
        lexer_free(&lex);
        free(source);
        return 2;
    }

    Parser parser;
    parser_init(&parser, lex.tokens, lex.tokens_len);
    parser.destino_generico = 1; /* `3 = n;` passa pelo parser e vira SEM013 */
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
        Diagnosticos diagnosticos = {NULL, 0, 0};
        semantico_analisar(programa, &diagnosticos);
        if (modo_ir && diagnosticos.len == 0) {
            IrPrograma codigo_ir;
            memset(&codigo_ir, 0, sizeof codigo_ir);
            ir_gerar(programa, &codigo_ir);
            if (otimizar) {
                otimizador_otimizar(&codigo_ir);
            }
            char *texto = ir_formatar(&codigo_ir);
            fputs(texto, stdout);
            free(texto);
            ir_programa_free(&codigo_ir);
            codigo = 0;
        } else {
            char *relatorio = semantico_relatorio(&diagnosticos);
            fputs(relatorio, stdout);
            free(relatorio);
            codigo = diagnosticos.len > 0 ? 4 : 0;
        }
        diagnosticos_free(&diagnosticos);
    }

    parser_free(&parser);
    lexer_free(&lex);
    free(source);
    return codigo;
}
