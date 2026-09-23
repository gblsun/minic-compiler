/* Implementação do parser do MINIC — ver parser.h para o contrato.
 *
 * A ordem das funções aqui é a mesma de src/python/parser.py: cursor, erros,
 * recuperação, programa, declarações, comandos e expressões (do nível de
 * precedência mais fraco para o mais forte).
 */

#include "parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Limite de aninhamento (parênteses, unários, atribuições encadeadas, comandos
 * dentro de comandos). Sem ele, uma entrada patológica estouraria a pilha da
 * descida recursiva. Mesmo valor de LIMITE_ANINHAMENTO em src/python/parser.py,
 * para as duas implementações rejeitarem as mesmas entradas. */
#define LIMITE_ANINHAMENTO 200

/* Os quatro tipos de dado (`void` só aparece como tipo de retorno). */
static const TokenType TIPOS[] = {TOK_KW_INT, TOK_KW_FLOAT, TOK_KW_BOOL, TOK_KW_CHAR};

/* ------------------------------------------------------------------ *
 * Utilitários de string
 * ------------------------------------------------------------------ */

/* Equivalente do f-string do Python: mede, aloca e formata (duas passadas).
 * Nome com prefixo `parser_` porque lexer.c tem uma funcao equivalente chamada
 * `format_string`, e as duas convivem na mesma unidade de traducao quando o
 * parser.c da raiz inclui os dois fontes. */
static char *parser_format(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list copia;
    va_copy(copia, args);
    int n = vsnprintf(NULL, 0, fmt, copia);
    va_end(copia);
    if (n < 0) {
        va_end(args);
        return NULL;
    }
    char *buf = malloc((size_t)n + 1);
    if (!buf) {
        fprintf(stderr, "erro: memória insuficiente ao formatar diagnóstico\n");
        exit(1);
    }
    vsnprintf(buf, (size_t)n + 1, fmt, args);
    va_end(args);
    return buf;
}

static char *duplicar(const char *texto) {
    size_t n = strlen(texto) + 1;
    char *copia = malloc(n);
    if (!copia) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    memcpy(copia, texto, n);
    return copia;
}

/* ------------------------------------------------------------------ *
 * Nomes "didáticos" dos tokens nas mensagens de erro.
 *
 * São os mesmos rótulos que o pacote de testes oficial usa nas pistas dos
 * casos inválidos (`pista: esperado PONTO_E_VIRGULA`), o que torna a
 * conferência direta. Em Python isso é o dicionário NOMES.
 * ------------------------------------------------------------------ */

static const char *nome_token(TokenType tipo) {
    switch (tipo) {
    case TOK_SEMICOLON: return "PONTO_E_VIRGULA";
    case TOK_COMMA: return "VIRGULA";
    case TOK_LPAREN: return "ABRE_PAREN";
    case TOK_RPAREN: return "FECHA_PAREN";
    case TOK_LBRACE: return "ABRE_CHAVE";
    case TOK_RBRACE: return "FECHA_CHAVE";
    case TOK_LBRACKET: return "ABRE_COLCHETE";
    case TOK_RBRACKET: return "FECHA_COLCHETE";
    case TOK_ASSIGN: return "ATRIBUICAO";
    case TOK_EOF: return "fim do arquivo";
    default: return token_type_name(tipo);
    }
}

/* Lexema de cada operador, para montar os nós Binary/Unary. */
static const char *operador_lexema(TokenType tipo) {
    switch (tipo) {
    case TOK_OR: return "||";
    case TOK_AND: return "&&";
    case TOK_EQ: return "==";
    case TOK_NEQ: return "!=";
    case TOK_LT: return "<";
    case TOK_GT: return ">";
    case TOK_LE: return "<=";
    case TOK_GE: return ">=";
    case TOK_PLUS: return "+";
    case TOK_MINUS: return "-";
    case TOK_STAR: return "*";
    case TOK_SLASH: return "/";
    case TOK_PERCENT: return "%";
    case TOK_NOT: return "!";
    default: return "?";
    }
}

static int e_tipo(TokenType tipo) {
    for (size_t i = 0; i < sizeof(TIPOS) / sizeof(TIPOS[0]); i++) {
        if (TIPOS[i] == tipo) {
            return 1;
        }
    }
    return 0;
}

/* Tokens que não podem, em nenhuma hipótese, iniciar um comando — o mesmo
 * conjunto NAO_INICIA_COMANDO do Python. */
static int nao_inicia_comando(TokenType tipo) {
    return tipo == TOK_RBRACE || tipo == TOK_RPAREN || tipo == TOK_RBRACKET ||
           tipo == TOK_COMMA || tipo == TOK_KW_ELSE || tipo == TOK_EOF;
}

/* ------------------------------------------------------------------ *
 * Ciclo de vida
 * ------------------------------------------------------------------ */

void parser_init(Parser *p, const Token *tokens, size_t tokens_len) {
    p->tokens = tokens;
    p->tokens_len = tokens_len;
    p->pos = 0;
    p->errors = NULL;
    p->errors_len = 0;
    p->errors_cap = 0;
    p->nodes = NULL;
    p->nodes_len = 0;
    p->nodes_cap = 0;
    p->pending = NULL;
    p->pending_line = 0;
    p->pending_column = 0;
    p->profundidade = 0;
}

void parser_free(Parser *p) {
    for (size_t i = 0; i < p->errors_len; i++) {
        free(p->errors[i].message);
    }
    free(p->errors);
    p->errors = NULL;
    p->errors_len = p->errors_cap = 0;

    for (size_t i = 0; i < p->nodes_len; i++) {
        ast_free_node(p->nodes[i]);
    }
    free(p->nodes);
    p->nodes = NULL;
    p->nodes_len = p->nodes_cap = 0;

    free(p->pending);
    p->pending = NULL;
}

char *parseerror_to_string(const ParseError *error) {
    return parser_format("Erro de sintaxe na linha %d, coluna %d: %s.", error->line,
                         error->column, error->message);
}

/* ------------------------------------------------------------------ *
 * Nós (com registro na lista do parser)
 * ------------------------------------------------------------------ */

static Ast *novo_no_em(Parser *p, AstKind kind, int line, int column) {
    Ast *node = ast_new(kind, line, column);
    if (p->nodes_len == p->nodes_cap) {
        size_t nova = p->nodes_cap == 0 ? 32 : p->nodes_cap * 2;
        Ast **novo = realloc(p->nodes, nova * sizeof(Ast *));
        if (!novo) {
            fprintf(stderr, "erro: memória insuficiente ao construir a AST\n");
            exit(1);
        }
        p->nodes = novo;
        p->nodes_cap = nova;
    }
    p->nodes[p->nodes_len++] = node;
    return node;
}

static Ast *novo_no(Parser *p, AstKind kind, const Token *token) {
    return novo_no_em(p, kind, token->line, token->column);
}

/* ------------------------------------------------------------------ *
 * Cursor sobre a lista de tokens
 * ------------------------------------------------------------------ */

static const Token *p_peek(Parser *p) {
    if (p->pos >= p->tokens_len) {
        return &p->tokens[p->tokens_len - 1]; /* o EOF, sempre o último */
    }
    return &p->tokens[p->pos];
}

static int p_check(Parser *p, TokenType tipo) { return p_peek(p)->type == tipo; }

static const Token *p_advance(Parser *p) {
    const Token *token = p_peek(p);
    if (token->type != TOK_EOF) {
        p->pos++;
    }
    return token;
}

static int p_match(Parser *p, TokenType tipo) {
    if (p_check(p, tipo)) {
        p_advance(p);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ *
 * Erros: guarda o erro em voo e salta para o ponto de recuperação
 * ------------------------------------------------------------------ */

static void p_erro_posicao(Parser *p, char *mensagem, int line, int column) {
    free(p->pending);
    p->pending = mensagem; /* o parser passa a ser dono da string */
    p->pending_line = line;
    p->pending_column = column;
    longjmp(p->recover, 1);
}

/* Acrescenta "; encontrado X" com o token atual, como faz `_erro` no Python. */
static void p_erro(Parser *p, const char *mensagem) {
    const Token *token = p_peek(p);
    char *completa =
        parser_format("%s; encontrado %s", mensagem, nome_token(token->type));
    p_erro_posicao(p, completa, token->line, token->column);
}

static const Token *p_expect(Parser *p, TokenType tipo, const char *esperado) {
    if (p_check(p, tipo)) {
        return p_advance(p);
    }
    const Token *token = p_peek(p);
    char *completa = parser_format("esperado %s; encontrado %s",
                                   esperado ? esperado : nome_token(tipo),
                                   nome_token(token->type));
    p_erro_posicao(p, completa, token->line, token->column);
    return NULL; /* inalcançável: p_erro_posicao nunca retorna */
}

static void registrar_pendente(Parser *p) {
    if (p->errors_len == p->errors_cap) {
        size_t nova = p->errors_cap == 0 ? 8 : p->errors_cap * 2;
        ParseError *novo = realloc(p->errors, nova * sizeof(ParseError));
        if (!novo) {
            fprintf(stderr, "erro: memória insuficiente ao registrar diagnóstico\n");
            exit(1);
        }
        p->errors = novo;
        p->errors_cap = nova;
    }
    p->errors[p->errors_len].message = p->pending ? p->pending : duplicar("erro sintático");
    p->errors[p->errors_len].line = p->pending_line;
    p->errors[p->errors_len].column = p->pending_column;
    p->errors_len++;
    p->pending = NULL; /* a posse da string passou para o ParseError */
}

/* Chama `funcao` contando um nível de aninhamento. Passando do limite, o erro
 * não vai para o ponto de recuperação mais próximo (`recover`), e sim para
 * `abortar`, que encerra a análise: retomar dentro de 200 blocos abertos só
 * geraria uma cascata de erros — com `{` repetido, o mesmo erro para sempre. */
static Ast *aninhado(Parser *p, Ast *(*funcao)(Parser *)) {
    if (p->profundidade >= LIMITE_ANINHAMENTO) {
        const Token *token = p_peek(p);
        free(p->pending);
        p->pending = parser_format("aninhamento acima do limite de %d níveis; encontrado %s",
                                   LIMITE_ANINHAMENTO, nome_token(token->type));
        p->pending_line = token->line;
        p->pending_column = token->column;
        longjmp(p->abortar, 1);
    }
    p->profundidade++;
    Ast *resultado = funcao(p);
    p->profundidade--;
    return resultado;
}

/* Modo pânico: descarta tokens até um ponto seguro para retomar. Mesma lista
 * de sincronizadores do Python. */
static void sincronizar(Parser *p) {
    while (!p_check(p, TOK_EOF)) {
        TokenType tipo = p_peek(p)->type;
        if (tipo == TOK_SEMICOLON || tipo == TOK_RBRACE) {
            p_advance(p);
            return;
        }
        if (e_tipo(tipo) || tipo == TOK_KW_VOID || tipo == TOK_KW_IF ||
            tipo == TOK_KW_WHILE || tipo == TOK_KW_FOR || tipo == TOK_KW_RETURN ||
            tipo == TOK_KW_BREAK || tipo == TOK_KW_CONTINUE || tipo == TOK_KW_PRINT ||
            tipo == TOK_KW_READ || tipo == TOK_LBRACE) {
            return;
        }
        p_advance(p);
    }
}

/* ------------------------------------------------------------------ *
 * Declarações e comandos (protótipos, porque são mutuamente recursivos)
 * ------------------------------------------------------------------ */

static void parse_item_de_topo(Parser *p, Ast *destino);
static void parse_declaracao(Parser *p, Ast *destino);
static void parse_var_decls(Parser *p, const Token *tipo, const Token *nome, Ast *destino);
static Ast *parse_declarador(Parser *p, const Token *tipo, const Token *nome);
static Ast *parse_funcao(Parser *p, const Token *tipo, const Token *nome);
static Ast *parse_parametro(Parser *p);
static Ast *parse_bloco(Parser *p);
static void parse_item_de_bloco(Parser *p, Ast *bloco);
static Ast *parse_comando(Parser *p);
static Ast *parse_expressao(Parser *p);

/* ------------------------------------------------------------------ *
 * Programa
 * ------------------------------------------------------------------ */

Ast *parser_parse(Parser *p) {
    const Token *primeiro = p_peek(p);
    Ast *programa = novo_no(p, AST_PROGRAM, primeiro);

    if (setjmp(p->abortar) != 0) {
        registrar_pendente(p);
        return programa;
    }

    while (!p_check(p, TOK_EOF)) {
        /* O longjmp pula os `profundidade--` de `aninhado`; quem retoma
         * restaura o valor que havia antes da tentativa (em Python, o `finally`
         * de `_aninhado` faz isso sozinho). */
        int profundidade = p->profundidade;
        if (setjmp(p->recover) == 0) {
            parse_item_de_topo(p, programa);
        } else {
            p->profundidade = profundidade;
            registrar_pendente(p);
            sincronizar(p);
        }
    }
    return programa;
}

static void parse_item_de_topo(Parser *p, Ast *destino) {
    if (p_check(p, TOK_KW_VOID) || e_tipo(p_peek(p)->type)) {
        parse_declaracao(p, destino);
        return;
    }
    if (nao_inicia_comando(p_peek(p)->type)) {
        p_erro(p, "token inesperado no nível do programa");
    }
    ast_push(destino, parse_comando(p));
}

/* ------------------------------------------------------------------ *
 * Declarações
 * ------------------------------------------------------------------ */

static void parse_declaracao(Parser *p, Ast *destino) {
    const Token *tipo = p_advance(p);
    const Token *nome = p_expect(p, TOK_IDENT, NULL);
    if (p_check(p, TOK_LPAREN)) {
        ast_push(destino, parse_funcao(p, tipo, nome));
        return;
    }
    if (tipo->type == TOK_KW_VOID) {
        p_erro(p, "void só pode ser tipo de retorno de função; esperado ABRE_PAREN");
    }
    parse_var_decls(p, tipo, nome, destino);
}

/* Lista de declaradores (`int a, b = 2, v[3];`).
 *
 * Diferença de forma em relação ao Python: lá a função devolve um nó ou uma
 * lista de nós, e quem chama achata; aqui ela recebe o nó de destino e empurra
 * cada VarDecl direto nele — sem tipos-união nem retorno de lista, que em C
 * custariam bem mais que o ganho. */
static void parse_var_decls(Parser *p, const Token *tipo, const Token *nome, Ast *destino) {
    ast_push(destino, parse_declarador(p, tipo, nome));
    while (p_match(p, TOK_COMMA)) {
        const Token *outro = p_expect(p, TOK_IDENT, NULL);
        ast_push(destino, parse_declarador(p, tipo, outro));
    }
    p_expect(p, TOK_SEMICOLON, NULL);
}

static Ast *parse_declarador(Parser *p, const Token *tipo, const Token *nome) {
    Ast *no = novo_no(p, AST_VAR_DECL, nome);
    no->type_name = duplicar(tipo->lexeme);
    no->text = duplicar(nome->lexeme);
    if (p_match(p, TOK_LBRACKET)) {
        no->b = parse_expressao(p); /* tamanho */
        p_expect(p, TOK_RBRACKET, NULL);
        return no;
    }
    if (p_match(p, TOK_ASSIGN)) {
        no->a = parse_expressao(p); /* inicializador */
    }
    return no;
}

static Ast *parse_funcao(Parser *p, const Token *tipo, const Token *nome) {
    Ast *no = novo_no(p, AST_FUNCTION, tipo);
    no->type_name = duplicar(tipo->lexeme);
    no->text = duplicar(nome->lexeme);

    p_expect(p, TOK_LPAREN, NULL);
    if (!p_check(p, TOK_RPAREN)) {
        ast_push(no, parse_parametro(p));
        while (p_match(p, TOK_COMMA)) {
            ast_push(no, parse_parametro(p));
        }
    }
    p_expect(p, TOK_RPAREN, "FECHA_PAREN");
    if (!p_check(p, TOK_LBRACE)) {
        /* Função sem corpo (`int f();`) é erro: a MINIC não tem protótipo. */
        p_erro(p, "esperado ABRE_CHAVE");
    }
    no->a = parse_bloco(p);
    return no;
}

static Ast *parse_parametro(Parser *p) {
    if (!e_tipo(p_peek(p)->type)) {
        p_erro(p, "esperado KW_<tipo> ou FECHA_PAREN");
    }
    const Token *tipo = p_advance(p);
    const Token *nome = p_expect(p, TOK_IDENT, NULL);
    Ast *no = novo_no(p, AST_PARAM, tipo);
    no->type_name = duplicar(tipo->lexeme);
    no->text = duplicar(nome->lexeme);
    if (p_match(p, TOK_LBRACKET)) {
        p_expect(p, TOK_RBRACKET, NULL);
        no->is_array = 1;
    }
    return no;
}

/* ------------------------------------------------------------------ *
 * Comandos
 * ------------------------------------------------------------------ */

static Ast *parse_bloco(Parser *p) {
    const Token *abre = p_expect(p, TOK_LBRACE, NULL);
    Ast *bloco = novo_no(p, AST_BLOCK, abre);

    /* Este laço instala seu próprio ponto de recuperação (para reportar mais de
     * um erro por bloco), então o ponto anterior precisa ser salvo e reposto ao
     * sair — senão um erro depois do bloco saltaria para um quadro de pilha que
     * já não existe. Em Python isso é automático: cada `try` se empilha e se
     * desempilha sozinho. */
    jmp_buf anterior;
    memcpy(anterior, p->recover, sizeof(jmp_buf));

    while (!p_check(p, TOK_RBRACE) && !p_check(p, TOK_EOF)) {
        int profundidade = p->profundidade;
        if (setjmp(p->recover) == 0) {
            parse_item_de_bloco(p, bloco);
        } else {
            p->profundidade = profundidade;
            registrar_pendente(p);
            sincronizar(p);
        }
    }

    memcpy(p->recover, anterior, sizeof(jmp_buf));
    p_expect(p, TOK_RBRACE, "FECHA_CHAVE");
    return bloco;
}

static void parse_item_de_bloco(Parser *p, Ast *bloco) {
    if (e_tipo(p_peek(p)->type)) {
        const Token *tipo = p_advance(p);
        const Token *nome = p_expect(p, TOK_IDENT, NULL);
        parse_var_decls(p, tipo, nome, bloco);
        return;
    }
    if (nao_inicia_comando(p_peek(p)->type)) {
        p_erro(p, "token inesperado no início de statement");
    }
    ast_push(bloco, parse_comando(p));
}

static Ast *parse_comando_if(Parser *p) {
    const Token *token = p_advance(p);
    Ast *no = novo_no(p, AST_IF, token);
    p_expect(p, TOK_LPAREN, NULL);
    no->a = parse_expressao(p);
    p_expect(p, TOK_RPAREN, "FECHA_PAREN");
    no->b = parse_comando(p);
    if (p_match(p, TOK_KW_ELSE)) {
        /* Consumir o `else` assim que ele aparece resolve o "else pendente"
         * ligando-o ao `if` mais próximo, que é a convenção adotada. */
        no->c = parse_comando(p);
    }
    return no;
}

static Ast *parse_comando_while(Parser *p) {
    const Token *token = p_advance(p);
    Ast *no = novo_no(p, AST_WHILE, token);
    p_expect(p, TOK_LPAREN, NULL);
    no->a = parse_expressao(p);
    p_expect(p, TOK_RPAREN, "FECHA_PAREN");
    no->b = parse_comando(p);
    return no;
}

static Ast *parse_comando_for(Parser *p) {
    const Token *token = p_advance(p);
    Ast *no = novo_no(p, AST_FOR, token);
    p_expect(p, TOK_LPAREN, NULL);
    if (!p_check(p, TOK_SEMICOLON)) {
        no->a = parse_expressao(p);
    }
    p_expect(p, TOK_SEMICOLON, NULL);
    if (!p_check(p, TOK_SEMICOLON)) {
        no->b = parse_expressao(p);
    }
    p_expect(p, TOK_SEMICOLON, NULL);
    if (!p_check(p, TOK_RPAREN)) {
        no->c = parse_expressao(p);
    }
    p_expect(p, TOK_RPAREN, "FECHA_PAREN");
    no->d = parse_comando(p);
    return no;
}

static Ast *parse_comando_return(Parser *p) {
    const Token *token = p_advance(p);
    Ast *no = novo_no(p, AST_RETURN, token);
    if (!p_check(p, TOK_SEMICOLON)) {
        no->a = parse_expressao(p);
    }
    p_expect(p, TOK_SEMICOLON, NULL);
    return no;
}

static Ast *parse_comando_print(Parser *p) {
    const Token *token = p_advance(p);
    Ast *no = novo_no(p, AST_PRINT, token);
    p_expect(p, TOK_LPAREN, NULL);
    no->a = parse_expressao(p);
    p_expect(p, TOK_RPAREN, "FECHA_PAREN");
    p_expect(p, TOK_SEMICOLON, NULL);
    return no;
}

static Ast *parse_comando_read(Parser *p) {
    const Token *token = p_advance(p);
    Ast *no = novo_no(p, AST_READ, token);
    p_expect(p, TOK_LPAREN, NULL);
    no->a = parse_expressao(p);
    if (no->a->kind != AST_ID && no->a->kind != AST_INDEX) {
        p_erro(p, "read espera uma variável ou elemento de vetor");
    }
    p_expect(p, TOK_RPAREN, "FECHA_PAREN");
    p_expect(p, TOK_SEMICOLON, NULL);
    return no;
}

static Ast *parse_comando_sem_limite(Parser *p) {
    const Token *token = p_peek(p);
    TokenType tipo = token->type;

    if (nao_inicia_comando(tipo)) {
        /* Corpo de construção ausente, como em `while (x < 2) }`. */
        p_erro(p, "esperado início de statement");
    }

    switch (tipo) {
    case TOK_LBRACE:
        return parse_bloco(p);
    case TOK_KW_IF:
        return parse_comando_if(p);
    case TOK_KW_WHILE:
        return parse_comando_while(p);
    case TOK_KW_FOR:
        return parse_comando_for(p);
    case TOK_KW_RETURN:
        return parse_comando_return(p);
    case TOK_KW_PRINT:
        return parse_comando_print(p);
    case TOK_KW_READ:
        return parse_comando_read(p);
    case TOK_KW_BREAK: {
        p_advance(p);
        p_expect(p, TOK_SEMICOLON, NULL);
        return novo_no(p, AST_BREAK, token);
    }
    case TOK_KW_CONTINUE: {
        p_advance(p);
        p_expect(p, TOK_SEMICOLON, NULL);
        return novo_no(p, AST_CONTINUE, token);
    }
    case TOK_SEMICOLON: {
        /* `comando_expressao ::= expressao? ";"` — o comando vazio. */
        p_advance(p);
        return novo_no(p, AST_EXPR_STMT, token);
    }
    default:
        break;
    }

    if (e_tipo(tipo)) {
        /* Declaração onde só cabe comando (ex.: `if (x) int y;`). */
        p_erro(p, "declaração não é permitida aqui; esperado início de statement");
    }

    Ast *no = novo_no(p, AST_EXPR_STMT, token);
    no->a = parse_expressao(p);
    p_expect(p, TOK_SEMICOLON, NULL);
    return no;
}

static Ast *parse_comando(Parser *p) { return aninhado(p, parse_comando_sem_limite); }

/* ------------------------------------------------------------------ *
 * Expressões — um nível por função, do mais fraco ao mais forte
 * ------------------------------------------------------------------ */

static Ast *parse_posfixa(Parser *p);
static Ast *parse_unaria(Parser *p);

/* Laço genérico de um nível binário associativo à esquerda (o equivalente do
 * `_binario_esquerda` do Python): a gramática da especificação é recursiva à
 * esquerda, o que um parser descendente não pode executar literalmente, e o
 * laço produz exatamente a mesma árvore. */
static Ast *binario_esquerda(Parser *p, Ast *(*proximo)(Parser *),
                             const TokenType *ops, size_t n_ops) {
    Ast *no = proximo(p);
    for (;;) {
        TokenType atual = p_peek(p)->type;
        size_t i = 0;
        while (i < n_ops && ops[i] != atual) {
            i++;
        }
        if (i == n_ops) {
            return no;
        }
        const Token *op = p_advance(p);
        Ast *direita = proximo(p);
        Ast *pai = novo_no(p, AST_BINARY, op);
        pai->text = duplicar(operador_lexema(op->type));
        pai->a = no;
        pai->b = direita;
        no = pai;
    }
}

static Ast *parse_multiplicativa(Parser *p) {
    static const TokenType ops[] = {TOK_STAR, TOK_SLASH, TOK_PERCENT};
    return binario_esquerda(p, parse_unaria, ops, 3);
}

static Ast *parse_aditiva(Parser *p) {
    static const TokenType ops[] = {TOK_PLUS, TOK_MINUS};
    return binario_esquerda(p, parse_multiplicativa, ops, 2);
}

static Ast *parse_relacional(Parser *p) {
    static const TokenType ops[] = {TOK_LT, TOK_GT, TOK_LE, TOK_GE};
    return binario_esquerda(p, parse_aditiva, ops, 4);
}

static Ast *parse_igualdade(Parser *p) {
    static const TokenType ops[] = {TOK_EQ, TOK_NEQ};
    return binario_esquerda(p, parse_relacional, ops, 2);
}

static Ast *parse_logico_e(Parser *p) {
    static const TokenType ops[] = {TOK_AND};
    return binario_esquerda(p, parse_igualdade, ops, 1);
}

static Ast *parse_logico_ou(Parser *p) {
    static const TokenType ops[] = {TOK_OR};
    return binario_esquerda(p, parse_logico_e, ops, 1);
}

static Ast *parse_atribuicao(Parser *p);

static Ast *parse_atribuicao_sem_limite(Parser *p) {
    Ast *esquerda = parse_logico_ou(p);
    if (!p_check(p, TOK_ASSIGN)) {
        return esquerda;
    }
    const Token *igual = p_advance(p);
    if (esquerda->kind != AST_ID && esquerda->kind != AST_INDEX) {
        p_erro_posicao(p,
                       duplicar("lado esquerdo da atribuição não é atribuível; "
                                "esperado identificador ou elemento de vetor"),
                       igual->line, igual->column);
    }
    /* O nó herda a posição do alvo (como no Python), e não a do `=`. */
    Ast *no = novo_no_em(p, AST_ASSIGN, esquerda->line, esquerda->column);
    no->a = esquerda;
    /* Recursão (e não laço) porque `=` é associativo à direita. */
    no->b = parse_atribuicao(p);
    return no;
}

static Ast *parse_atribuicao(Parser *p) { return aninhado(p, parse_atribuicao_sem_limite); }

static Ast *parse_expressao(Parser *p) { return parse_atribuicao(p); }

static Ast *parse_unaria(Parser *p) {
    if (p_check(p, TOK_MINUS) || p_check(p, TOK_NOT)) {
        const Token *op = p_advance(p);
        Ast *no = novo_no(p, AST_UNARY, op);
        no->text = duplicar(operador_lexema(op->type));
        no->a = aninhado(p, parse_unaria); /* associativo à direita */
        return no;
    }
    return parse_posfixa(p);
}

static Ast *parse_primario(Parser *p) {
    const Token *token = p_peek(p);

    switch (token->type) {
    case TOK_IDENT: {
        p_advance(p);
        Ast *no = novo_no(p, AST_ID, token);
        no->text = duplicar(token->lexeme);
        return no;
    }
    case TOK_INT:
    case TOK_FLOAT:
    case TOK_KW_TRUE:
    case TOK_KW_FALSE: {
        p_advance(p);
        Ast *no = novo_no(p, AST_LIT, token);
        no->type_name = duplicar(token->type == TOK_INT      ? "int"
                                 : token->type == TOK_FLOAT  ? "real"
                                                             : "bool");
        no->text = duplicar(token->lexeme);
        return no;
    }
    case TOK_CHAR:
    case TOK_STRING: {
        p_advance(p);
        Ast *no = novo_no(p, AST_LIT, token);
        int e_char = token->type == TOK_CHAR;
        no->type_name = duplicar(e_char ? "char" : "string");
        /* O lexer entrega o valor já decodificado; aqui o literal é reescrito
         * na forma de origem, com aspas e escapes. */
        char *escapado = ast_escape(token->lexeme);
        no->text = parser_format(e_char ? "'%s'" : "\"%s\"", escapado);
        free(escapado);
        return no;
    }
    case TOK_LPAREN: {
        p_advance(p);
        Ast *interna = parse_expressao(p);
        p_expect(p, TOK_RPAREN, "FECHA_PAREN");
        /* Os parênteses não geram nó: a hierarquia da árvore já registra o
         * agrupamento que eles pediram. */
        return interna;
    }
    default:
        p_erro(p, "esperado expressão (identificador, literal ou ABRE_PAREN)");
        return NULL; /* inalcançável */
    }
}

static Ast *parse_posfixa(Parser *p) {
    Ast *no = parse_primario(p);
    for (;;) {
        if (p_check(p, TOK_LBRACKET)) {
            p_advance(p);
            Ast *indice = parse_expressao(p);
            p_expect(p, TOK_RBRACKET, NULL);
            /* O nó posfixo herda a posição da base, como no Python. */
            Ast *pai = novo_no_em(p, AST_INDEX, no->line, no->column);
            pai->a = no;
            pai->b = indice;
            no = pai;
            continue;
        }
        if (p_check(p, TOK_LPAREN)) {
            p_advance(p);
            Ast *pai = novo_no_em(p, AST_CALL, no->line, no->column);
            pai->a = no;
            if (!p_check(p, TOK_RPAREN)) {
                ast_push(pai, parse_expressao(p));
                while (p_match(p, TOK_COMMA)) {
                    ast_push(pai, parse_expressao(p));
                }
            }
            p_expect(p, TOK_RPAREN, "FECHA_PAREN");
            no = pai;
            continue;
        }
        return no;
    }
}
