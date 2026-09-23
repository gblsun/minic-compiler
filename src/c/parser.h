/* Analisador sintático (parser) da linguagem MINIC.
 *
 * Transcrição para C de src/python/parser.py: descida recursiva, uma função
 * por não terminal da gramática de docs/gramatica.md, mesmas decisões de
 * desambiguação e mesmas mensagens de erro, para que as duas implementações
 * aceitem/rejeitem exatamente os mesmos programas com a mesma saída.
 *
 * Duas diferenças de mecânica em relação ao Python, ambas por falta de
 * equivalente direto em C:
 *
 * 1. **Abortar a análise.** Em Python cada função "desiste" levantando
 *    ParseError, e o laço de itens captura com try/except. Aqui o papel do
 *    try/except é feito com setjmp/longjmp: o erro em voo fica guardado no
 *    Parser e o salto leva o controle de volta ao ponto de recuperação.
 * 2. **Quem é dono dos nós.** O Parser guarda todos os nós que criou e os
 *    libera em parser_free(), porque uma análise abortada deixa subárvores
 *    órfãs que em Python o coletor de lixo recolheria. Consequência prática,
 *    igual à do lexer: quem usa a AST **depois** de parser_free() está lendo
 *    memória já liberada.
 */

#ifndef PARSER_H
#define PARSER_H

#include <setjmp.h>
#include <stddef.h>

#include "ast.h"
#include "lexer.h"

/* Um erro sintático, no formato exigido pela Seção 12 da especificação. */
typedef struct {
    char *message; /* alocado dinamicamente; o ParseError é dono da string */
    int line;
    int column;
} ParseError;

typedef struct {
    const Token *tokens;
    size_t tokens_len;
    size_t pos; /* índice do próximo token a consumir */

    ParseError *errors;
    size_t errors_len;
    size_t errors_cap;

    /* Todos os nós criados durante a análise (ver comentário do cabeçalho). */
    Ast **nodes;
    size_t nodes_len;
    size_t nodes_cap;

    /* Ponto de retomada e o erro que está "em voo" até chegar nele. */
    jmp_buf recover;
    /* Saída de emergência para aninhamento acima de LIMITE_ANINHAMENTO: pula
     * todos os pontos de recuperação e encerra a análise (ver parser.c). */
    jmp_buf abortar;
    int profundidade; /* níveis de aninhamento abertos agora */
    char *pending;
    int pending_line;
    int pending_column;
} Parser;

/* Prepara o parser sobre um array de tokens (que deve permanecer válido
 * durante o uso; a struct não copia nada). */
void parser_init(Parser *parser, const Token *tokens, size_t tokens_len);

/* Libera os erros e **todos os nós da AST** criados pelo parser. */
void parser_free(Parser *parser);

/* Analisa o arquivo inteiro e devolve o nó Program.
 *
 * Nunca devolve NULL: mesmo com erros, a árvore parcial é devolvida (útil para
 * depuração). Quem chama decide o que fazer olhando parser->errors_len — e,
 * havendo erro, não deve imprimir a AST (o pacote de testes oficial é
 * explícito: entrada rejeitada não produz árvore). */
Ast *parser_parse(Parser *parser);

/* Formata a mensagem de erro sintático no padrão da Seção 12 da especificação.
 * O chamador é dono da string devolvida (usar free()). */
char *parseerror_to_string(const ParseError *error);

#endif /* PARSER_H */
