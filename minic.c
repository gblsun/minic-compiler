/* Ponto de entrada do compilador MINIC em C (até a análise semântica), como a
 * AP 2 pede:
 *
 *     gcc -Wall -Wextra minic.c -o minic
 *     ./minic codigo.c
 *
 * O código de verdade mora em src/c/, um arquivo por componente: lexer.c
 * (etapa 1), ast.c e parser.c (etapa 2), tabela_simbolos.c e semantico.c
 * (etapa 3), e minic_main.c, a CLI que encadeia as fases. Este arquivo existe
 * porque o script de teste do professor compila **uma única unidade de
 * tradução** (`gcc -Wall -Wextra "$arquivo_fonte" -o "$executavel"`), e é o
 * mesmo arranjo do parser.c da raiz.
 *
 * Para desenvolver, a compilação separada continua valendo:
 *
 *     gcc -Wall -Wextra -std=c11 src/c/lexer.c src/c/ast.c src/c/parser.c \
 *         src/c/tabela_simbolos.c src/c/semantico.c src/c/minic_main.c -o minic
 */

#include "src/c/lexer.c"
#include "src/c/ast.c"
#include "src/c/parser.c"
#include "src/c/tabela_simbolos.c"
#include "src/c/semantico.c"
#include "src/c/minic_main.c"
