/* Ponto de entrada do analisador sintático do MINIC em C, como o enunciado
 * pede:
 *
 *     gcc -Wall -Wextra -std=c11 parser.c -o parser
 *     ./parser codigo.c
 *
 * O código de verdade mora em src/c/ (lexer.c da etapa 1, mais ast.c, parser.c
 * e parser_main.c da etapa 2). Este arquivo existe porque o script de teste do
 * professor compila **uma única unidade de tradução**:
 *
 *     gcc -Wall -Wextra -std=c11 $PARSER -o "${PARSER/.c/}"
 *
 * ou seja, ele passa um único .c para o compilador. Incluir os fontes aqui faz
 * com que essa linha de comando funcione sem adaptação nenhuma — o que importa,
 * porque é o professor quem vai rodá-la.
 *
 * Para desenvolver, a compilação separada continua valendo e é preferível (o
 * compilador confere cada arquivo isoladamente):
 *
 *     gcc -Wall -Wextra -std=c11 src/c/lexer.c src/c/ast.c src/c/parser.c \
 *         src/c/parser_main.c -o src/c/parser
 */

#include "src/c/lexer.c"
#include "src/c/ast.c"
#include "src/c/parser.c"
#include "src/c/parser_main.c"
