#include "lexer.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------- */
/* Utilidades genéricas: string formatada, buffer de string crescente e   */
/* arrays crescentes de Token / LexError. Em Python essas coisas vêm de   */
/* graça (str % args, listas dinâmicas); em C precisamos escrevê-las.     */
/* ---------------------------------------------------------------------- */

static char *format_string(const char *fmt, ...) {
    va_list args, args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    char *buf = malloc((size_t)needed + 1);
    vsnprintf(buf, (size_t)needed + 1, fmt, args);
    va_end(args);
    return buf;
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static void strbuf_init(StrBuf *sb) {
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void strbuf_push(StrBuf *sb, char c) {
    if (sb->len + 1 >= sb->cap) {
        sb->cap = sb->cap ? sb->cap * 2 : 16;
        sb->data = realloc(sb->data, sb->cap);
    }
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

/* Entrega a posse do buffer para quem chamou (equivalente a "".join(chars)). */
static char *strbuf_finish(StrBuf *sb) {
    if (!sb->data) {
        sb->data = malloc(1);
        sb->data[0] = '\0';
    }
    return sb->data;
}

static void tokens_push(Lexer *lexer, Token token) {
    if (lexer->tokens_len + 1 > lexer->tokens_cap) {
        lexer->tokens_cap = lexer->tokens_cap ? lexer->tokens_cap * 2 : 32;
        lexer->tokens = realloc(lexer->tokens, lexer->tokens_cap * sizeof(Token));
    }
    lexer->tokens[lexer->tokens_len++] = token;
}

static void errors_push(Lexer *lexer, LexError error) {
    if (lexer->errors_len + 1 > lexer->errors_cap) {
        lexer->errors_cap = lexer->errors_cap ? lexer->errors_cap * 2 : 8;
        lexer->errors = realloc(lexer->errors, lexer->errors_cap * sizeof(LexError));
    }
    lexer->errors[lexer->errors_len++] = error;
}

/* ---------------------------------------------------------------------- */
/* Tabelas estáticas: equivalentes a KEYWORDS, ESCAPES, TWO_CHAR_SYMBOLS e */
/* ONE_CHAR_SYMBOLS do arquivo Python.                                    */
/* ---------------------------------------------------------------------- */

/* Palavras reservadas da linguagem: se um identificador escaneado bater com
 * uma entrada desta tabela, ele deixa de ser IDENT e vira a palavra-chave
 * correspondente. É por isso que identificadores e palavras-chave são
 * reconhecidos pela mesma função (scan_identifier_or_keyword) — elas têm
 * exatamente a mesma forma sintática, a diferença só aparece depois de ler
 * a palavra inteira. */
typedef struct {
    const char *word;
    TokenType type;
} KeywordEntry;

static const KeywordEntry KEYWORDS[] = {
    {"int", TOK_KW_INT},         {"float", TOK_KW_FLOAT},
    {"bool", TOK_KW_BOOL},       {"char", TOK_KW_CHAR},
    {"void", TOK_KW_VOID},       {"if", TOK_KW_IF},
    {"else", TOK_KW_ELSE},       {"while", TOK_KW_WHILE},
    {"for", TOK_KW_FOR},         {"return", TOK_KW_RETURN},
    {"break", TOK_KW_BREAK},     {"continue", TOK_KW_CONTINUE},
    {"true", TOK_KW_TRUE},       {"false", TOK_KW_FALSE},
    {"print", TOK_KW_PRINT},     {"read", TOK_KW_READ},
};
#define KEYWORDS_COUNT (sizeof(KEYWORDS) / sizeof(KEYWORDS[0]))

/* Operadores de dois caracteres. Precisam ser conferidos ANTES dos de um
 * caractere só, senão "==" seria lido como dois ASSIGN separados em vez de
 * um único EQ. Essa é a técnica clássica de "maximal munch": sempre tentar
 * consumir o token mais longo possível primeiro. */
typedef struct {
    const char pair[3]; /* dois caracteres + '\0' */
    TokenType type;
} TwoCharEntry;

static const TwoCharEntry TWO_CHAR_SYMBOLS[] = {
    {"==", TOK_EQ}, {"!=", TOK_NEQ}, {"<=", TOK_LE},
    {">=", TOK_GE}, {"&&", TOK_AND}, {"||", TOK_OR},
};
#define TWO_CHAR_SYMBOLS_COUNT (sizeof(TWO_CHAR_SYMBOLS) / sizeof(TWO_CHAR_SYMBOLS[0]))

/* Símbolos de um único caractere (operadores, delimitadores, pontuação). */
typedef struct {
    char ch;
    TokenType type;
} OneCharEntry;

static const OneCharEntry ONE_CHAR_SYMBOLS[] = {
    {'+', TOK_PLUS},     {'-', TOK_MINUS},   {'*', TOK_STAR},
    {'/', TOK_SLASH},    {'%', TOK_PERCENT}, {'<', TOK_LT},
    {'>', TOK_GT},       {'!', TOK_NOT},     {'=', TOK_ASSIGN},
    {'(', TOK_LPAREN},   {')', TOK_RPAREN},  {'[', TOK_LBRACKET},
    {']', TOK_RBRACKET}, {'{', TOK_LBRACE},  {'}', TOK_RBRACE},
    {';', TOK_SEMICOLON}, {',', TOK_COMMA},
};
#define ONE_CHAR_SYMBOLS_COUNT (sizeof(ONE_CHAR_SYMBOLS) / sizeof(ONE_CHAR_SYMBOLS[0]))

/* Sequências de escape válidas dentro de char/string (Seção 3.5 da spec).
 * Qualquer coisa fora daqui (ex.: "\q") é erro léxico — ver read_escape. */
static int escape_lookup(char esc, char *out) {
    switch (esc) {
        case 'n': *out = '\n'; return 1;
        case 't': *out = '\t'; return 1;
        case '\\': *out = '\\'; return 1;
        case '\'': *out = '\''; return 1;
        case '"': *out = '"'; return 1;
        default: return 0;
    }
}

/* Só esses tipos de token carregam um "atributo" (o lexema em si) que muda
 * de ocorrência para ocorrência. Os demais (palavras reservadas, símbolos,
 * etc.) são sempre o mesmo texto fixo, então não faz sentido repetir o
 * lexema na saída — o próprio nome do token já diz tudo. */
static int is_token_com_atributo(TokenType type) {
    return type == TOK_IDENT || type == TOK_INT || type == TOK_FLOAT ||
           type == TOK_CHAR || type == TOK_STRING;
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOK_IDENT: return "IDENT";
        case TOK_INT: return "INT";
        case TOK_FLOAT: return "FLOAT";
        case TOK_CHAR: return "CHAR";
        case TOK_STRING: return "STRING";

        case TOK_KW_INT: return "KW_INT";
        case TOK_KW_FLOAT: return "KW_FLOAT";
        case TOK_KW_BOOL: return "KW_BOOL";
        case TOK_KW_CHAR: return "KW_CHAR";
        case TOK_KW_VOID: return "KW_VOID";
        case TOK_KW_IF: return "KW_IF";
        case TOK_KW_ELSE: return "KW_ELSE";
        case TOK_KW_WHILE: return "KW_WHILE";
        case TOK_KW_FOR: return "KW_FOR";
        case TOK_KW_RETURN: return "KW_RETURN";
        case TOK_KW_BREAK: return "KW_BREAK";
        case TOK_KW_CONTINUE: return "KW_CONTINUE";
        case TOK_KW_TRUE: return "KW_TRUE";
        case TOK_KW_FALSE: return "KW_FALSE";
        case TOK_KW_PRINT: return "KW_PRINT";
        case TOK_KW_READ: return "KW_READ";

        case TOK_EQ: return "EQ";
        case TOK_NEQ: return "NEQ";
        case TOK_LE: return "LE";
        case TOK_GE: return "GE";
        case TOK_AND: return "AND";
        case TOK_OR: return "OR";

        case TOK_PLUS: return "PLUS";
        case TOK_MINUS: return "MINUS";
        case TOK_STAR: return "STAR";
        case TOK_SLASH: return "SLASH";
        case TOK_PERCENT: return "PERCENT";
        case TOK_LT: return "LT";
        case TOK_GT: return "GT";
        case TOK_NOT: return "NOT";
        case TOK_ASSIGN: return "ASSIGN";
        case TOK_LPAREN: return "LPAREN";
        case TOK_RPAREN: return "RPAREN";
        case TOK_LBRACKET: return "LBRACKET";
        case TOK_RBRACKET: return "RBRACKET";
        case TOK_LBRACE: return "LBRACE";
        case TOK_RBRACE: return "RBRACE";
        case TOK_SEMICOLON: return "SEMICOLON";
        case TOK_COMMA: return "COMMA";

        case TOK_EOF: return "EOF";
    }
    return "?";
}

char *token_to_string(const Token *token) {
    /* Formato pedido pela especificação: tokens "com atributo" aparecem
     * como <TOKEN, lexema>; os demais aparecem só com o nome do token, já
     * que o lexema seria sempre igual ao próprio tipo (ex.: SEMICOLON é
     * sempre ";"). */
    if (is_token_com_atributo(token->type)) {
        return format_string("<%s, %s>", token_type_name(token->type), token->lexeme);
    }
    return format_string("%s", token_type_name(token->type));
}

char *lexerror_to_string(const LexError *error) {
    return format_string("Erro léxico na linha %d, coluna %d: %s.", error->line,
                          error->column, error->message);
}

/* ---------------------------------------------------------------------- */
/* Cursor: camada de baixo nível do lexer. Ler caracteres do código-fonte  */
/* um a um, sempre mantendo line/column em dia. Todo o resto do arquivo é */
/* construído em cima só destas quatro operações.                        */
/* ---------------------------------------------------------------------- */

static int lexer_at_end(const Lexer *lexer) {
    return lexer->pos >= lexer->length;
}

/* Olha um caractere à frente sem consumi-lo (lookahead). Devolve '\0'
 * quando a posição pedida está fora do arquivo, assim quem chama pode
 * comparar com um caractere normal sem precisar checar limites o tempo
 * todo (ex.: lexer_peek(lexer, 0) == '\'' funciona mesmo perto do fim do
 * arquivo). */
static char lexer_peek(const Lexer *lexer, int offset) {
    size_t idx = lexer->pos + (size_t)offset;
    if (idx >= lexer->length) {
        return '\0';
    }
    return lexer->source[idx];
}

/* Consome e devolve o caractere atual, avançando o cursor. É aqui que
 * linha/coluna são atualizadas: uma quebra de linha reinicia a coluna e
 * incrementa a linha; qualquer outro caractere só anda uma coluna para a
 * direita. */
static char lexer_advance(Lexer *lexer) {
    char ch = lexer->source[lexer->pos];
    lexer->pos += 1;
    if (ch == '\n') {
        lexer->line += 1;
        lexer->column = 1;
    } else {
        lexer->column += 1;
    }
    return ch;
}

static void lexer_add_token(Lexer *lexer, TokenType type, char *lexeme, int line,
                             int column, TokenValue value) {
    Token token;
    token.type = type;
    token.lexeme = lexeme;
    token.line = line;
    token.column = column;
    token.value = value;
    tokens_push(lexer, token);
}

static void lexer_add_error(Lexer *lexer, char *message, int line, int column) {
    LexError error;
    error.message = message;
    error.line = line;
    error.column = column;
    errors_push(lexer, error);
}

/* ---------------------------------------------------------------------- */
/* Espaços e comentários                                                  */
/* ---------------------------------------------------------------------- */

static void lexer_skip_block_comment(Lexer *lexer) {
    /* Consome um comentário de bloco (barra-asterisco ... asterisco-barra),
     * sem suporte a aninhamento, como manda a spec. */
    int start_line = lexer->line, start_col = lexer->column;
    lexer_advance(lexer); /* "/" */
    lexer_advance(lexer); /* "*" */
    while (!lexer_at_end(lexer)) {
        if (lexer_peek(lexer, 0) == '*' && lexer_peek(lexer, 1) == '/') {
            lexer_advance(lexer);
            lexer_advance(lexer);
            return;
        }
        lexer_advance(lexer);
    }
    /* Chegamos ao fim do arquivo sem achar o fechamento do comentário. */
    lexer_add_error(lexer, format_string("comentário de bloco não terminado"),
                     start_line, start_col);
}

/* Pula espaços, tabs, quebras de linha e comentários antes do próximo
 * token. Isso roda em loop porque pode haver várias dessas coisas em
 * sequência (ex.: um comentário seguido de espaços seguido de outro
 * comentário); só paramos quando encontramos algo que de fato inicia um
 * token. */
static void lexer_skip_whitespace_and_comments(Lexer *lexer) {
    while (!lexer_at_end(lexer)) {
        char ch = lexer_peek(lexer, 0);
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            lexer_advance(lexer);
            continue;
        }
        if (ch == '/' && lexer_peek(lexer, 1) == '/') {
            /* Comentário de linha: consome tudo até a próxima quebra de
             * linha (sem consumir a própria quebra, que o loop de cima
             * trata na próxima iteração). */
            while (!lexer_at_end(lexer) && lexer_peek(lexer, 0) != '\n') {
                lexer_advance(lexer);
            }
            continue;
        }
        if (ch == '/' && lexer_peek(lexer, 1) == '*') {
            lexer_skip_block_comment(lexer);
            continue;
        }
        break;
    }
}

/* ---------------------------------------------------------------------- */
/* Escaneadores de cada categoria léxica                                  */
/* ---------------------------------------------------------------------- */

/* Interpreta o caractere logo após uma '\' (já consumida por quem chamou).
 * Devolve o valor "traduzido" do escape (ex.: \n vira uma quebra de linha
 * de verdade). Se a sequência não existir na tabela de escapes, registra
 * um erro léxico mas ainda assim consome o caractere inválido — assim o
 * lexer não trava num loop infinito tentando reler o mesmo caractere, e
 * consegue seguir escaneando o resto do arquivo. */
static char lexer_read_escape(Lexer *lexer) {
    char esc = lexer_peek(lexer, 0);
    char translated;
    if (escape_lookup(esc, &translated)) {
        lexer_advance(lexer);
        return translated;
    }
    int line = lexer->line, column = lexer->column;
    lexer_add_error(lexer, format_string("sequência de escape inválida \"\\%c\"", esc),
                     line, column);
    if (!lexer_at_end(lexer) && esc != '\0') {
        lexer_advance(lexer);
    }
    return esc;
}

/* Lê um identificador inteiro (maximal munch) e só depois decide o tipo.
 * Consumimos letras, dígitos e '_' enquanto der, formando o maior lexema
 * possível, e só então conferimos se ele é uma palavra reservada. Fazer
 * essa checagem cedo demais (ex.: parar em "in" achando que é "int")
 * quebraria identificadores como "inteiro" ou "index". */
static void lexer_scan_identifier_or_keyword(Lexer *lexer, char first, int line,
                                              int column) {
    StrBuf sb;
    strbuf_init(&sb);
    strbuf_push(&sb, first);
    while (!lexer_at_end(lexer) &&
           (isalnum((unsigned char)lexer_peek(lexer, 0)) || lexer_peek(lexer, 0) == '_')) {
        strbuf_push(&sb, lexer_advance(lexer));
    }
    char *lexeme = strbuf_finish(&sb);

    TokenType type = TOK_IDENT;
    for (size_t i = 0; i < KEYWORDS_COUNT; i++) {
        if (strcmp(KEYWORDS[i].word, lexeme) == 0) {
            type = KEYWORDS[i].type;
            break;
        }
    }

    TokenValue value;
    value.str_value = lexeme; /* alias, não é dono separado */
    lexer_add_token(lexer, type, lexeme, line, column, value);
}

/* Lê um número inteiro ou de ponto flutuante. A parte fracionária só é
 * consumida se houver um dígito logo depois do ponto. Isso evita, por
 * exemplo, tratar "3." como início de um float incompleto: nesse caso o
 * "3" vira um INT normal e o "." sobra para ser tratado (e provavelmente
 * rejeitado, já que "." sozinho não é um símbolo reconhecido) na próxima
 * chamada. */
static void lexer_scan_number(Lexer *lexer, char first, int line, int column) {
    StrBuf sb;
    strbuf_init(&sb);
    strbuf_push(&sb, first);
    while (!lexer_at_end(lexer) && isdigit((unsigned char)lexer_peek(lexer, 0))) {
        strbuf_push(&sb, lexer_advance(lexer));
    }

    int is_float = 0;
    if (lexer_peek(lexer, 0) == '.' && isdigit((unsigned char)lexer_peek(lexer, 1))) {
        is_float = 1;
        strbuf_push(&sb, lexer_advance(lexer));
        while (!lexer_at_end(lexer) && isdigit((unsigned char)lexer_peek(lexer, 0))) {
            strbuf_push(&sb, lexer_advance(lexer));
        }
    }

    char *lexeme = strbuf_finish(&sb);
    TokenValue value;
    if (is_float) {
        value.float_value = strtod(lexeme, NULL);
        lexer_add_token(lexer, TOK_FLOAT, lexeme, line, column, value);
    } else {
        value.int_value = strtoll(lexeme, NULL, 10);
        lexer_add_token(lexer, TOK_INT, lexeme, line, column, value);
    }
}

/* Lê uma cadeia entre aspas duplas, processando escapes pelo caminho.
 * Strings em MINIC não atravessam quebra de linha: se a linha acabar
 * antes de fecharmos as aspas, é erro ("cadeia não terminada"), mesmo que
 * o arquivo continue depois. Isso evita que um '"' esquecido engula o
 * resto do arquivo inteiro como se fosse uma única string gigante. */
static void lexer_scan_string(Lexer *lexer, int line, int column) {
    StrBuf sb;
    strbuf_init(&sb);
    int terminated = 0;

    while (!lexer_at_end(lexer)) {
        char ch = lexer_peek(lexer, 0);
        if (ch == '"') {
            lexer_advance(lexer);
            terminated = 1;
            break;
        }
        if (ch == '\n') {
            break;
        }
        if (ch == '\\') {
            lexer_advance(lexer);
            strbuf_push(&sb, lexer_read_escape(lexer));
            continue;
        }
        strbuf_push(&sb, lexer_advance(lexer));
    }

    if (!terminated) {
        free(sb.data);
        lexer_add_error(lexer, format_string("cadeia não terminada"), line, column);
        return;
    }

    char *value_str = strbuf_finish(&sb);
    TokenValue value;
    value.str_value = value_str;
    lexer_add_token(lexer, TOK_STRING, value_str, line, column, value);
}

/* Lê um literal de caractere 'x'.
 *
 * Char é o caso mais delicado do lexer porque a spec exige exatamente UM
 * caractere entre aspas simples — nem zero, nem dois ou mais — então
 * precisamos diferenciar três situações de erro além do caso feliz:
 *
 * 1. '' (vazio) ou aspas nunca fechadas na mesma linha/arquivo.
 * 2. Exatamente um caractere (ou um escape) seguido de ' -> sucesso.
 * 3. Mais de um caractere antes do ' de fechamento -> "tamanho inválido"
 *    (achamos o fechamento, só que tarde demais).
 */
static void lexer_scan_char(Lexer *lexer, int line, int column) {
    if (lexer_at_end(lexer) || lexer_peek(lexer, 0) == '\n' || lexer_peek(lexer, 0) == '\'') {
        if (lexer_peek(lexer, 0) == '\'') {
            /* '': as aspas fecham imediatamente, ou seja, zero caracteres
             * dentro — tamanho inválido, não "vazio". */
            lexer_advance(lexer);
            lexer_add_error(lexer, format_string("caractere com tamanho inválido"), line,
                             column);
        } else {
            lexer_add_error(lexer, format_string("literal de caractere não terminado"),
                             line, column);
        }
        return;
    }

    char ch_value;
    if (lexer_peek(lexer, 0) == '\\') {
        lexer_advance(lexer);
        ch_value = lexer_read_escape(lexer);
    } else {
        ch_value = lexer_advance(lexer);
    }

    if (!lexer_at_end(lexer) && lexer_peek(lexer, 0) == '\'') {
        /* Caso feliz: um caractere (ou um escape, que conta como um só),
         * fechado corretamente. */
        lexer_advance(lexer);
        char *lexeme = malloc(2);
        lexeme[0] = ch_value;
        lexeme[1] = '\0';
        TokenValue value;
        value.str_value = lexeme;
        lexer_add_token(lexer, TOK_CHAR, lexeme, line, column, value);
        return;
    }

    /* Sobrou mais coisa antes do fechamento (ex.: 'ab') — ou o arquivo
     * acabou/quebrou linha sem fechar. Drenamos o resto para descobrir
     * qual dos dois casos é, e só então decidimos a mensagem de erro. */
    while (!lexer_at_end(lexer) && lexer_peek(lexer, 0) != '\'' && lexer_peek(lexer, 0) != '\n') {
        lexer_advance(lexer);
    }
    if (!lexer_at_end(lexer) && lexer_peek(lexer, 0) == '\'') {
        lexer_advance(lexer);
        lexer_add_error(lexer, format_string("caractere com tamanho inválido"), line, column);
    } else {
        lexer_add_error(lexer, format_string("literal de caractere não terminado"), line,
                         column);
    }
}

/* Reconhece operadores e delimitadores, tentando sempre o mais longo
 * primeiro. Primeiro testamos o par ch + próximo caractere contra os
 * símbolos de dois caracteres (maximal munch, mesma ideia do EQ vs.
 * ASSIGN+ASSIGN citada lá em cima); só se isso falhar caímos para os
 * símbolos de um caractere. Se nem isso bater, o caractere não pertence ao
 * alfabeto da linguagem e viramos um erro léxico. */
static void lexer_scan_symbol(Lexer *lexer, char ch, int line, int column) {
    char pair[3] = {ch, lexer_peek(lexer, 0), '\0'};
    for (size_t i = 0; i < TWO_CHAR_SYMBOLS_COUNT; i++) {
        if (strcmp(TWO_CHAR_SYMBOLS[i].pair, pair) == 0) {
            lexer_advance(lexer);
            TokenValue value = {0};
            lexer_add_token(lexer, TWO_CHAR_SYMBOLS[i].type, format_string("%s", pair), line,
                             column, value);
            return;
        }
    }

    for (size_t i = 0; i < ONE_CHAR_SYMBOLS_COUNT; i++) {
        if (ONE_CHAR_SYMBOLS[i].ch == ch) {
            char *lexeme = malloc(2);
            lexeme[0] = ch;
            lexeme[1] = '\0';
            TokenValue value = {0};
            lexer_add_token(lexer, ONE_CHAR_SYMBOLS[i].type, lexeme, line, column, value);
            return;
        }
    }

    lexer_add_error(lexer, format_string("símbolo \"%c\" não reconhecido", ch), line, column);
}

/* Decide, a partir do primeiro caractere, qual tipo de token começa aqui.
 * Esse é o "roteador" principal do lexer: cada categoria léxica tem uma
 * característica no primeiro caractere que já entrega a que ela pertence
 * (letra/'_' -> identificador ou palavra-chave; dígito -> número; aspas ->
 * string/char; qualquer outra coisa -> símbolo/operador). */
static void lexer_scan_token(Lexer *lexer) {
    int line = lexer->line, column = lexer->column;
    char ch = lexer_advance(lexer);

    if (isalpha((unsigned char)ch) || ch == '_') {
        lexer_scan_identifier_or_keyword(lexer, ch, line, column);
    } else if (isdigit((unsigned char)ch)) {
        lexer_scan_number(lexer, ch, line, column);
    } else if (ch == '"') {
        lexer_scan_string(lexer, line, column);
    } else if (ch == '\'') {
        lexer_scan_char(lexer, line, column);
    } else {
        lexer_scan_symbol(lexer, ch, line, column);
    }
}

/* ---------------------------------------------------------------------- */
/* API pública                                                            */
/* ---------------------------------------------------------------------- */

void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->length = strlen(source);
    lexer->pos = 0;
    lexer->line = 1;
    lexer->column = 1;

    lexer->tokens = NULL;
    lexer->tokens_len = 0;
    lexer->tokens_cap = 0;

    lexer->errors = NULL;
    lexer->errors_len = 0;
    lexer->errors_cap = 0;
}

void lexer_free(Lexer *lexer) {
    for (size_t i = 0; i < lexer->tokens_len; i++) {
        free(lexer->tokens[i].lexeme);
    }
    free(lexer->tokens);
    lexer->tokens = NULL;
    lexer->tokens_len = lexer->tokens_cap = 0;

    for (size_t i = 0; i < lexer->errors_len; i++) {
        free(lexer->errors[i].message);
    }
    free(lexer->errors);
    lexer->errors = NULL;
    lexer->errors_len = lexer->errors_cap = 0;
}

/* Ponto de entrada: consome o código inteiro e devolve (via lexer->tokens
 * e lexer->errors) o resultado. */
void lexer_tokenize(Lexer *lexer) {
    while (1) {
        lexer_skip_whitespace_and_comments(lexer);
        if (lexer_at_end(lexer)) {
            break;
        }
        lexer_scan_token(lexer);
    }
    /* Um token EOF explícito no final ajuda etapas futuras do compilador
     * (o parser, por exemplo) a saber onde o fluxo de tokens acaba sem
     * precisar checar o tamanho do array toda hora. Quem for imprimir os
     * tokens deve filtrar esse token antes, já que ele não faz parte da
     * saída visível. */
    TokenValue empty_value = {0};
    lexer_add_token(lexer, TOK_EOF, format_string("%s", ""), lexer->line, lexer->column,
                     empty_value);
}