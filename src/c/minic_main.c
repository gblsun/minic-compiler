/* Compilador MINIC — a CLI do pipeline completo (Seção 11 da especificação).
 *
 * Uso: ./minic [--check|--tokens|--ast|--symbols|--ir|--target=asm|--run]
 *              [-O0|-O1] [--stats] <arquivo.c>
 *
 * Mesmo contrato de src/python/minic.py, para as duas implementações serem
 * comparadas byte a byte: lexer -> parser -> analisador semântico -> IR ->
 * otimizador (só com -O1) -> gerador de código (MVM) -> máquina virtual, cada
 * fase só rodando se a anterior terminou sem erro. Sem opção de modo, a saída
 * é o relatório da análise semântica (o contrato da AP 2).
 *
 * Códigos de saída (Seção 11.1): 0 sucesso, 1 uso inválido, 2 erro léxico,
 * 3 erro sintático, 4 erro semântico (relatório em stdout), 5 erro na geração
 * de código, 6 erro durante a execução.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "ast.h"
#include "geracao.h"
#include "ir.h"
#include "lexer.h"
#include "otimizador.h"
#include "parser.h"
#include "semantico.h"
#include "vm.h"

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

#define MINIC_USO                                                                         \
    "uso: %s [--check|--tokens|--ast|--symbols|--ir|--target=asm|--run] [-O0|-O1] [--stats] " \
    "<arquivo.c>\n"

static const char *MINIC_MODOS[] = {"--check", "--tokens", "--ast", "--symbols",
                                    "--ir", "--target=asm", "--run"};

static int minic_eh_modo(const char *arg) {
    for (size_t i = 0; i < sizeof MINIC_MODOS / sizeof MINIC_MODOS[0]; i++) {
        if (strcmp(arg, MINIC_MODOS[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Depois da análise semântica sem erros: IR, símbolos, código ou execução. */
static int minic_back_end(const Ast *programa, const char *modo, int otimizar, int estatisticas) {
    IrPrograma codigo_ir;
    memset(&codigo_ir, 0, sizeof codigo_ir);
    ir_gerar(programa, &codigo_ir);
    if (otimizar) {
        otimizador_otimizar(&codigo_ir);
    }
    if (strcmp(modo, "--ir") == 0 || strcmp(modo, "--symbols") == 0) {
        char *texto = strcmp(modo, "--ir") == 0 ? ir_formatar(&codigo_ir)
                                                : geracao_simbolos(&codigo_ir);
        fputs(texto, stdout);
        free(texto);
        ir_programa_free(&codigo_ir);
        return 0;
    }

    VmPrograma codigo_vm;
    memset(&codigo_vm, 0, sizeof codigo_vm);
    char *erro = NULL;
    int codigo = geracao_gerar(&codigo_ir, &codigo_vm, &erro);
    if (codigo) {
        fprintf(stderr, "Erro na geração de código: %s\n", erro);
        free(erro);
    } else if (strcmp(modo, "--target=asm") == 0) {
        char *texto = geracao_formatar(&codigo_vm);
        fputs(texto, stdout);
        free(texto);
    } else {
        long long executadas = 0;
        codigo = vm_executar(&codigo_vm, &erro, &executadas);
        if (codigo) {
            fprintf(stderr, "%s\n", erro);
            free(erro);
        }
        if (estatisticas) {
            /* Dígito a dígito: o printf do Windows antigo não conhece %lld. */
            char digitos[24];
            size_t n = 0;
            do {
                digitos[n++] = (char)('0' + executadas % 10);
                executadas /= 10;
            } while (executadas > 0);
            fputs("instruções executadas: ", stderr);
            while (n > 0) {
                fputc(digitos[--n], stderr);
            }
            fputc('\n', stderr);
        }
    }
    geracao_free(&codigo_vm);
    ir_programa_free(&codigo_ir);
    return codigo;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    /* Sem tradução de "\n" para "\r\n" (nem na entrada do read): o relatório
     * já traz os CRLF do gabarito, e o resto sai com LF, como no Linux. */
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        printf(MINIC_USO, argv[0]);
        return 0;
    }
    const char *modo = "--check";
    int modos = 0, otimizar = 0, estatisticas = 0, arquivos = 0;
    const char *arquivo = NULL;
    for (int i = 1; i < argc; i++) {
        if (minic_eh_modo(argv[i])) {
            modo = argv[i];
            modos++;
        } else if (strcmp(argv[i], "-O0") == 0 || strcmp(argv[i], "-O1") == 0) {
            otimizar = strcmp(argv[i], "-O1") == 0;
        } else if (strcmp(argv[i], "--stats") == 0) {
            estatisticas = 1;
        } else {
            arquivo = argv[i];
            arquivos++;
        }
    }
    if (modos > 1 || arquivos != 1 || arquivo[0] == '-') {
        fprintf(stderr, MINIC_USO, argv[0]);
        return 1;
    }

    char *source = minic_ler_arquivo(arquivo);

    Lexer lex;
    lexer_init(&lex, source);
    lexer_tokenize(&lex);
    if (strcmp(modo, "--tokens") == 0) {
        for (size_t i = 0; i < lex.tokens_len; i++) {
            if (lex.tokens[i].type != TOK_EOF) {
                char *s = token_to_string(&lex.tokens[i]);
                printf("%s\n", s);
                free(s);
            }
        }
        fflush(stdout);
    }
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
    if (strcmp(modo, "--tokens") == 0) {
        lexer_free(&lex);
        free(source);
        return 0;
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
    } else if (strcmp(modo, "--ast") == 0) {
        char *s = ast_to_sexpr(programa);
        printf("%s\n", s);
        free(s);
        codigo = 0;
    } else {
        Diagnosticos diagnosticos = {NULL, 0, 0};
        semantico_analisar(programa, &diagnosticos);
        if (strcmp(modo, "--check") == 0 || diagnosticos.len > 0) {
            char *relatorio = semantico_relatorio(&diagnosticos);
            fputs(relatorio, stdout);
            free(relatorio);
            codigo = diagnosticos.len > 0 ? 4 : 0;
        } else {
            codigo = minic_back_end(programa, modo, otimizar, estatisticas);
        }
        diagnosticos_free(&diagnosticos);
    }

    parser_free(&parser);
    lexer_free(&lex);
    free(source);
    return codigo;
}
