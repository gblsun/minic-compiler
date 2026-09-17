/* Implementação da AST do MINIC — construção, impressão e liberação.
 *
 * Ver ast.h para a tabela de "qual campo significa o quê em cada tipo de nó".
 * A impressão segue exatamente as mesmas regras de src/python/minic_ast.py:
 * itens de lista (Program/Block) separados por ", ", todo o resto compacto.
 */

#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Estilo da forma compacta — os mesmos valores de minic_ast.py. */
static const char *const SEP_LISTA = ", ";
static const char *const SEP = ",";
static const char *const EQ = "=";

/* ------------------------------------------------------------------ *
 * Buffer de string que cresce sozinho.
 *
 * Em Python a impressão é feita com f-strings e `"".join(...)`, que criam
 * strings novas a cada passo; em C isso viraria uma cascata de malloc/strcat
 * com custo quadrático. Este buffer é o equivalente prático: um bloco que
 * dobra de tamanho conforme necessário e no qual as partes vão sendo
 * emendadas.
 * ------------------------------------------------------------------ */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buf;

static void buf_init(Buf *b) {
    b->cap = 128;
    b->len = 0;
    b->data = malloc(b->cap);
    if (!b->data) {
        fprintf(stderr, "erro: memória insuficiente ao imprimir a AST\n");
        exit(1);
    }
    b->data[0] = '\0';
}

static void buf_reserve(Buf *b, size_t extra) {
    if (b->len + extra + 1 <= b->cap) {
        return;
    }
    while (b->len + extra + 1 > b->cap) {
        b->cap *= 2;
    }
    char *novo = realloc(b->data, b->cap);
    if (!novo) {
        fprintf(stderr, "erro: memória insuficiente ao imprimir a AST\n");
        exit(1);
    }
    b->data = novo;
}

static void buf_add(Buf *b, const char *texto) {
    size_t n = strlen(texto);
    buf_reserve(b, n);
    memcpy(b->data + b->len, texto, n);
    b->len += n;
    b->data[b->len] = '\0';
}

static void buf_add_char(Buf *b, char ch) {
    buf_reserve(b, 1);
    b->data[b->len++] = ch;
    b->data[b->len] = '\0';
}

/* ------------------------------------------------------------------ *
 * Construção
 * ------------------------------------------------------------------ */

Ast *ast_new(AstKind kind, int line, int column) {
    Ast *node = calloc(1, sizeof(Ast));
    if (!node) {
        fprintf(stderr, "erro: memória insuficiente ao construir a AST\n");
        exit(1);
    }
    node->kind = kind;
    node->line = line;
    node->column = column;
    return node;
}

void ast_push(Ast *parent, Ast *child) {
    if (parent->items_len == parent->items_cap) {
        size_t nova = parent->items_cap == 0 ? 4 : parent->items_cap * 2;
        Ast **novo = realloc(parent->items, nova * sizeof(Ast *));
        if (!novo) {
            fprintf(stderr, "erro: memória insuficiente ao construir a AST\n");
            exit(1);
        }
        parent->items = novo;
        parent->items_cap = nova;
    }
    parent->items[parent->items_len++] = child;
}

void ast_free_node(Ast *node) {
    if (!node) {
        return;
    }
    free(node->text);
    free(node->type_name);
    free(node->items);
    free(node);
}

char *ast_escape(const char *texto) {
    Buf b;
    buf_init(&b);
    for (const char *p = texto; *p; p++) {
        switch (*p) {
        case '\\':
            buf_add(&b, "\\\\");
            break;
        case '\n':
            buf_add(&b, "\\n");
            break;
        case '\t':
            buf_add(&b, "\\t");
            break;
        case '\r':
            buf_add(&b, "\\r");
            break;
        case '\'':
            buf_add(&b, "\\'");
            break;
        case '"':
            buf_add(&b, "\\\"");
            break;
        default:
            buf_add_char(&b, *p);
            break;
        }
    }
    return b.data;
}

void ast_free(Ast *node) {
    if (!node) {
        return;
    }
    free(node->text);
    free(node->type_name);
    ast_free(node->a);
    ast_free(node->b);
    ast_free(node->c);
    ast_free(node->d);
    for (size_t i = 0; i < node->items_len; i++) {
        ast_free(node->items[i]);
    }
    free(node->items);
    free(node);
}

/* ------------------------------------------------------------------ *
 * Impressão compacta (a notação dos testes oficiais)
 * ------------------------------------------------------------------ */

static void sexpr(const Ast *node, Buf *out);

/* Filho que pode não existir: `NULL` explícito, como no Python. */
static void sexpr_opt(const Ast *node, Buf *out) {
    if (!node) {
        buf_add(out, "NULL");
        return;
    }
    sexpr(node, out);
}

static void sexpr_lista(Ast *const *itens, size_t n, const char *sep, Buf *out) {
    for (size_t i = 0; i < n; i++) {
        if (i > 0) {
            buf_add(out, sep);
        }
        sexpr(itens[i], out);
    }
}

static void sexpr(const Ast *node, Buf *out) {
    switch (node->kind) {
    case AST_PROGRAM:
        buf_add(out, "Program(");
        sexpr_lista(node->items, node->items_len, SEP_LISTA, out);
        buf_add_char(out, ')');
        break;

    case AST_VAR_DECL:
        buf_add(out, "VarDecl(");
        buf_add(out, node->type_name);
        buf_add_char(out, ' ');
        buf_add(out, node->text);
        if (node->b) { /* vetor: `size=` */
            buf_add(out, " size=");
            sexpr(node->b, out);
        } else if (node->a) { /* inicializador */
            buf_add(out, EQ);
            sexpr(node->a, out);
        }
        buf_add_char(out, ')');
        break;

    case AST_PARAM:
        buf_add(out, node->type_name);
        buf_add_char(out, ' ');
        buf_add(out, node->text);
        if (node->is_array) {
            buf_add(out, "[]");
        }
        break;

    case AST_FUNCTION:
        buf_add(out, "Function(");
        buf_add(out, node->type_name);
        buf_add_char(out, ' ');
        buf_add(out, node->text);
        buf_add_char(out, '(');
        sexpr_lista(node->items, node->items_len, ",", out);
        buf_add(out, ") ");
        sexpr_opt(node->a, out);
        buf_add_char(out, ')');
        break;

    case AST_BLOCK:
        buf_add(out, "Block(");
        sexpr_lista(node->items, node->items_len, SEP_LISTA, out);
        buf_add_char(out, ')');
        break;

    case AST_EXPR_STMT:
        buf_add(out, "ExprStmt(");
        sexpr_opt(node->a, out);
        buf_add_char(out, ')');
        break;

    case AST_IF:
        buf_add(out, "If(");
        sexpr_opt(node->a, out);
        buf_add(out, SEP);
        sexpr_opt(node->b, out);
        buf_add(out, SEP);
        sexpr_opt(node->c, out);
        buf_add_char(out, ')');
        break;

    case AST_WHILE:
        buf_add(out, "While(");
        sexpr_opt(node->a, out);
        buf_add(out, SEP);
        sexpr_opt(node->b, out);
        buf_add_char(out, ')');
        break;

    case AST_FOR:
        buf_add(out, "For(");
        sexpr_opt(node->a, out);
        buf_add(out, SEP);
        sexpr_opt(node->b, out);
        buf_add(out, SEP);
        sexpr_opt(node->c, out);
        buf_add(out, SEP);
        sexpr_opt(node->d, out);
        buf_add_char(out, ')');
        break;

    case AST_RETURN:
        buf_add(out, "Return(");
        sexpr_opt(node->a, out);
        buf_add_char(out, ')');
        break;

    case AST_BREAK:
        buf_add(out, "Break()");
        break;

    case AST_CONTINUE:
        buf_add(out, "Continue()");
        break;

    case AST_PRINT:
        buf_add(out, "Print(");
        sexpr_opt(node->a, out);
        buf_add_char(out, ')');
        break;

    case AST_READ:
        buf_add(out, "Read(");
        sexpr_opt(node->a, out);
        buf_add_char(out, ')');
        break;

    case AST_ASSIGN:
        buf_add(out, "Assign(");
        sexpr_opt(node->a, out);
        buf_add(out, SEP);
        sexpr_opt(node->b, out);
        buf_add_char(out, ')');
        break;

    case AST_BINARY:
        buf_add(out, "Binary(");
        buf_add(out, node->text);
        buf_add(out, SEP);
        sexpr_opt(node->a, out);
        buf_add(out, SEP);
        sexpr_opt(node->b, out);
        buf_add_char(out, ')');
        break;

    case AST_UNARY:
        buf_add(out, "Unary(");
        buf_add(out, node->text);
        buf_add(out, SEP);
        sexpr_opt(node->a, out);
        buf_add_char(out, ')');
        break;

    case AST_CALL:
        buf_add(out, "Call(");
        sexpr_opt(node->a, out);
        for (size_t i = 0; i < node->items_len; i++) {
            buf_add(out, SEP);
            sexpr(node->items[i], out);
        }
        buf_add_char(out, ')');
        break;

    case AST_INDEX:
        buf_add(out, "Index(");
        sexpr_opt(node->a, out);
        buf_add(out, SEP);
        sexpr_opt(node->b, out);
        buf_add_char(out, ')');
        break;

    case AST_ID:
        buf_add(out, "Id(");
        buf_add(out, node->text);
        buf_add_char(out, ')');
        break;

    case AST_LIT:
        /* Único nó em que o separador é sempre "," em todos os arquivos
         * oficiais — por isso não usa SEP. */
        buf_add(out, "Lit(");
        buf_add(out, node->type_name);
        buf_add_char(out, ',');
        buf_add(out, node->text);
        buf_add_char(out, ')');
        break;
    }
}

char *ast_to_sexpr(const Ast *node) {
    Buf b;
    buf_init(&b);
    if (!node) {
        buf_add(&b, "NULL");
    } else {
        sexpr(node, &b);
    }
    return b.data;
}

/* ------------------------------------------------------------------ *
 * Impressão indentada (para leitura humana)
 * ------------------------------------------------------------------ */

static void tree(const Ast *node, int nivel, Buf *out);

static void recuo(int nivel, Buf *out) {
    for (int i = 0; i < nivel; i++) {
        buf_add(out, "  ");
    }
}

static void tree_opt(const Ast *node, int nivel, Buf *out) {
    if (!node) {
        recuo(nivel, out);
        buf_add(out, "NULL\n");
        return;
    }
    tree(node, nivel, out);
}

static void tree(const Ast *node, int nivel, Buf *out) {
    recuo(nivel, out);
    switch (node->kind) {
    case AST_PROGRAM:
        buf_add(out, "Program\n");
        for (size_t i = 0; i < node->items_len; i++) {
            tree(node->items[i], nivel + 1, out);
        }
        break;

    case AST_VAR_DECL:
        buf_add(out, "VarDecl type=");
        buf_add(out, node->type_name);
        buf_add(out, " name=");
        buf_add(out, node->text);
        buf_add_char(out, '\n');
        if (node->b) {
            recuo(nivel + 1, out);
            buf_add(out, "size\n");
            tree(node->b, nivel + 2, out);
        }
        if (node->a) {
            tree(node->a, nivel + 1, out);
        }
        break;

    case AST_PARAM:
        buf_add(out, "Param type=");
        buf_add(out, node->type_name);
        buf_add(out, " name=");
        buf_add(out, node->text);
        if (node->is_array) {
            buf_add(out, "[]");
        }
        buf_add_char(out, '\n');
        break;

    case AST_FUNCTION:
        buf_add(out, "Function returnType=");
        buf_add(out, node->type_name);
        buf_add(out, " name=");
        buf_add(out, node->text);
        buf_add_char(out, '\n');
        for (size_t i = 0; i < node->items_len; i++) {
            tree(node->items[i], nivel + 1, out);
        }
        tree_opt(node->a, nivel + 1, out);
        break;

    case AST_BLOCK:
        buf_add(out, "Block\n");
        for (size_t i = 0; i < node->items_len; i++) {
            tree(node->items[i], nivel + 1, out);
        }
        break;

    case AST_EXPR_STMT:
        buf_add(out, "ExprStmt\n");
        tree_opt(node->a, nivel + 1, out);
        break;

    case AST_IF:
        buf_add(out, "If\n");
        tree_opt(node->a, nivel + 1, out);
        tree_opt(node->b, nivel + 1, out);
        tree_opt(node->c, nivel + 1, out);
        break;

    case AST_WHILE:
        buf_add(out, "While\n");
        tree_opt(node->a, nivel + 1, out);
        tree_opt(node->b, nivel + 1, out);
        break;

    case AST_FOR:
        buf_add(out, "For\n");
        tree_opt(node->a, nivel + 1, out);
        tree_opt(node->b, nivel + 1, out);
        tree_opt(node->c, nivel + 1, out);
        tree_opt(node->d, nivel + 1, out);
        break;

    case AST_RETURN:
        buf_add(out, "Return\n");
        tree_opt(node->a, nivel + 1, out);
        break;

    case AST_BREAK:
        buf_add(out, "Break\n");
        break;

    case AST_CONTINUE:
        buf_add(out, "Continue\n");
        break;

    case AST_PRINT:
        buf_add(out, "Print\n");
        tree_opt(node->a, nivel + 1, out);
        break;

    case AST_READ:
        buf_add(out, "Read\n");
        tree_opt(node->a, nivel + 1, out);
        break;

    case AST_ASSIGN:
        buf_add(out, "Assign\n");
        tree_opt(node->a, nivel + 1, out);
        tree_opt(node->b, nivel + 1, out);
        break;

    case AST_BINARY:
        buf_add(out, "Binary op=");
        buf_add(out, node->text);
        buf_add_char(out, '\n');
        tree_opt(node->a, nivel + 1, out);
        tree_opt(node->b, nivel + 1, out);
        break;

    case AST_UNARY:
        buf_add(out, "Unary op=");
        buf_add(out, node->text);
        buf_add_char(out, '\n');
        tree_opt(node->a, nivel + 1, out);
        break;

    case AST_CALL:
        buf_add(out, "Call\n");
        tree_opt(node->a, nivel + 1, out);
        for (size_t i = 0; i < node->items_len; i++) {
            tree(node->items[i], nivel + 1, out);
        }
        break;

    case AST_INDEX:
        buf_add(out, "Index\n");
        tree_opt(node->a, nivel + 1, out);
        tree_opt(node->b, nivel + 1, out);
        break;

    case AST_ID:
        buf_add(out, "Id name=");
        buf_add(out, node->text);
        buf_add_char(out, '\n');
        break;

    case AST_LIT:
        buf_add(out, "Lit type=");
        buf_add(out, node->type_name);
        buf_add(out, " value=");
        buf_add(out, node->text);
        buf_add_char(out, '\n');
        break;
    }
}

char *ast_to_tree(const Ast *node) {
    Buf b;
    buf_init(&b);
    if (!node) {
        buf_add(&b, "NULL");
        return b.data;
    }
    tree(node, 0, &b);
    /* A versão Python devolve as linhas unidas por "\n", sem quebra final;
     * aqui cada linha já sai com "\n", então a última é removida para que as
     * duas saídas sejam idênticas byte a byte. */
    if (b.len > 0 && b.data[b.len - 1] == '\n') {
        b.data[--b.len] = '\0';
    }
    return b.data;
}
