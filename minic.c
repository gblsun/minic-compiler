/* Ponto de entrada do compilador MINIC em C (até a representação
 * intermediária), como a AP 2 pede:
 *
 *     gcc -Wall -Wextra minic.c -o minic
 *     ./minic codigo.c              # análise semântica (relatório da AP 2)
 *     ./minic --ir -O1 codigo.c     # IR de três endereços, otimizada
 *     ./minic --run codigo.c        # gera o código da máquina de pilha e executa
 *
 * O código de verdade mora em src/c/, um arquivo por componente: lexer.c
 * (etapa 1), ast.c e parser.c (etapa 2), tabela_simbolos.c e semantico.c
 * (etapa 3), ir.c e otimizador.c (IR e -O1), geracao.c e vm.c (back-end:
 * código da máquina de pilha e sua execução), e minic_main.c, a CLI que
 * encadeia as fases. Este arquivo existe porque o script de teste do professor
 * compila **uma única unidade de tradução** (`gcc -Wall -Wextra
 * "$arquivo_fonte" -o "$executavel"`), e é o mesmo arranjo do parser.c da raiz.
 *
 * Para desenvolver, a compilação separada continua valendo:
 *
 *     gcc -Wall -Wextra -std=c11 src/c/lexer.c src/c/ast.c src/c/parser.c \
 *         src/c/tabela_simbolos.c src/c/semantico.c src/c/ir.c \
 *         src/c/otimizador.c src/c/geracao.c src/c/vm.c src/c/minic_main.c \
 *         -o minic
 */

#include "src/c/lexer.c"
#include "src/c/ast.c"
#include "src/c/parser.c"
#include "src/c/tabela_simbolos.c"
#include "src/c/semantico.c"
#include "src/c/ir.c"
#include "src/c/otimizador.c"
#include "src/c/geracao.c"
#include "src/c/vm.c"
#include "src/c/minic_main.c"
