/* Analisador léxico da linguagem MINIC.
 *
 * Transcrição para C do lexer originalmente escrito em Python, mantendo a
 * mesma lógica: percorremos o código-fonte caractere a caractere, com um
 * "cursor" que sabe em que posição (linha e coluna) estamos, e a cada passo
 * decidimos que tipo de token está começando ali só olhando para o
 * caractere atual (e, quando precisa, para o próximo). Não usamos
 * expressões regulares de propósito — o objetivo é entender como um lexer
 * funciona por dentro, então o reconhecimento é feito token a token na mão.
 */

#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

/* Tipos de token reconhecidos pelo lexer.
 *
 * Usamos o prefixo TOK_ (em vez dos nomes "crus" como em Python) para não
 * colidir com macros padrão do C, como EOF de <stdio.h>.
 */
typedef enum {
    TOK_IDENT,
    TOK_INT,
    TOK_FLOAT,
    TOK_CHAR,
    TOK_STRING,

    /* Palavras reservadas */
    TOK_KW_INT,
    TOK_KW_FLOAT,
    TOK_KW_BOOL,
    TOK_KW_CHAR,
    TOK_KW_VOID,
    TOK_KW_IF,
    TOK_KW_ELSE,
    TOK_KW_WHILE,
    TOK_KW_FOR,
    TOK_KW_RETURN,
    TOK_KW_BREAK,
    TOK_KW_CONTINUE,
    TOK_KW_TRUE,
    TOK_KW_FALSE,
    TOK_KW_PRINT,
    TOK_KW_READ,

    /* Operadores de dois caracteres */
    TOK_EQ,
    TOK_NEQ,
    TOK_LE,
    TOK_GE,
    TOK_AND,
    TOK_OR,

    /* Símbolos de um único caractere */
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_LT,
    TOK_GT,
    TOK_NOT,
    TOK_ASSIGN,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_SEMICOLON,
    TOK_COMMA,

    TOK_EOF
} TokenType;

/* Valor já convertido do token (equivalente ao campo `value` da versão
 * Python: o int de "42", não a string "42"). Só é relevante para os tokens
 * "com atributo"; hoje só é realmente usado internamente, quem imprime o
 * token usa `lexeme` via token_to_string. Para IDENT/CHAR/STRING, o ponteiro
 * `str_value` aponta para a MESMA string de `lexeme` (não é dono dela) —
 * assim como em Python, onde `value` e `lexeme` são o mesmo objeto.
 */
typedef union {
    long long int_value;
    double float_value;
    char *str_value; /* alias de Token.lexeme; não liberar separadamente */
} TokenValue;

/* Um token reconhecido pelo lexer.
 *
 * `line`/`column` guardam onde o token COMEÇA no código-fonte (1-indexado,
 * como um editor de texto mostraria), o que é essencial para as mensagens
 * de erro do parser mais pra frente.
 */
typedef struct {
    TokenType type;
    char *lexeme; /* alocado dinamicamente; o Token é dono desta string */
    int line;
    int column;
    TokenValue value;
} Token;

/* Um erro léxico, no formato exigido pela Seção 12 da especificação. */
typedef struct {
    char *message; /* alocado dinamicamente; o LexError é dono desta string */
    int line;
    int column;
} LexError;

/* Varre o código-fonte inteiro e produz a lista de tokens e de erros.
 *
 * O lexer NÃO para no primeiro erro: ele registra o erro e continua
 * tentando reconhecer o resto do arquivo, para que quem chama consiga
 * reportar todos os problemas léxicos de uma vez (em vez de obrigar o
 * usuário a corrigir um erro por vez e rodar de novo).
 */
typedef struct {
    const char *source;
    size_t length;
    size_t pos; /* índice absoluto do próximo caractere a ler em `source` */
    int line;
    int column;

    Token *tokens;
    size_t tokens_len;
    size_t tokens_cap;

    LexError *errors;
    size_t errors_len;
    size_t errors_cap;
} Lexer;

/* Nome "cru" do tipo de token (equivalente ao nome usado em Python, ex.
 * "IDENT", "KW_INT", "SEMICOLON"). String estática, não precisa de free. */
const char *token_type_name(TokenType type);

/* Formata um token no padrão pedido pela especificação: tokens "com
 * atributo" (IDENT/INT/FLOAT/CHAR/STRING) aparecem como "<TOKEN, lexema>";
 * os demais aparecem só com o nome do token. O chamador é dono da string
 * devolvida (usar free()). */
char *token_to_string(const Token *token);

/* Formata a mensagem de erro léxico no padrão da Seção 12 da especificação.
 * O chamador é dono da string devolvida (usar free()). */
char *lexerror_to_string(const LexError *error);

/* Inicializa um Lexer sobre `source` (que deve permanecer válido durante o
 * uso do lexer; a struct não faz cópia do código-fonte). */
void lexer_init(Lexer *lexer, const char *source);

/* Libera os tokens e erros acumulados no lexer (mas não `source`, que
 * continua sendo responsabilidade de quem chamou lexer_init). */
void lexer_free(Lexer *lexer);

/* Ponto de entrada: consome o código inteiro, preenchendo lexer->tokens e
 * lexer->errors. Um token TOK_EOF explícito é sempre adicionado ao final,
 * para que etapas futuras do compilador (o parser, por exemplo) saibam onde
 * o fluxo de tokens acaba sem precisar checar o tamanho do array toda hora.
 */
void lexer_tokenize(Lexer *lexer);

#endif /* LEXER_H */