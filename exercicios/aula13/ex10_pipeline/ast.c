/* Implementação de ast.h — mesma serialização de ast_nodes.py. */

#include "ast.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

const char *const NOME_TIPO[] = {"int", "float", "bool", "erro"};

void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    return p;
}

void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    return q;
}

char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    return memcpy(xmalloc(n), s, n);
}

char *xformat(const char *fmt, ...) {
    va_list a, b;
    va_start(a, fmt);
    va_copy(b, a);
    int n = vsnprintf(NULL, 0, fmt, b);
    va_end(b);
    char *buf = xmalloc((size_t)(n < 0 ? 0 : n) + 1);
    vsnprintf(buf, (size_t)n + 1, fmt, a);
    va_end(a);
    return buf;
}

static void arena_registra(Arena *ar, void *p, int e_no) {
    if (ar->n == ar->cap) {
        ar->cap = ar->cap ? ar->cap * 2 : 64;
        ar->ptr = xrealloc(ar->ptr, ar->cap * sizeof(void *));
        ar->e_no = xrealloc(ar->e_no, ar->cap * sizeof(int));
    }
    ar->ptr[ar->n] = p;
    ar->e_no[ar->n++] = e_no;
}

Expr *expr_novo(Arena *ar, ExprKind k, const char *texto, Tipo tipo, int linha, int coluna, Expr *a, Expr *b) {
    Expr *e = xmalloc(sizeof(Expr));
    *e = (Expr){k, xstrdup(texto), tipo, linha, coluna, a, b};
    arena_registra(ar, e, 0);
    return e;
}

No *no_novo(Arena *ar, NoKind k, int linha, int coluna) {
    No *n = xmalloc(sizeof(No));
    *n = (No){.k = k, .linha = linha, .coluna = coluna};
    arena_registra(ar, n, 1);
    return n;
}

void no_add(No *bloco, No *item) {
    if (bloco->n == bloco->cap) {
        bloco->cap = bloco->cap ? bloco->cap * 2 : 4;
        bloco->itens = xrealloc(bloco->itens, bloco->cap * sizeof(No *));
    }
    bloco->itens[bloco->n++] = item;
}

void programa_add(Programa *p, No *item) {
    if (p->n == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 8;
        p->itens = xrealloc(p->itens, p->cap * sizeof(No *));
    }
    p->itens[p->n++] = item;
}

void arena_free(Arena *ar) {
    for (size_t i = 0; i < ar->n; i++) {
        if (ar->e_no[i]) {
            No *n = ar->ptr[i];
            free(n->nome), free(n->escopo), free(n->itens);
        } else {
            free(((Expr *)ar->ptr[i])->texto);
        }
        free(ar->ptr[i]);
    }
    free(ar->ptr), free(ar->e_no);
    *ar = (Arena){0};
}

void programa_free(Programa *p) {
    arena_free(&p->arena);
    free(p->itens);
    free(p);
}

/* -- serialização ----------------------------------------------------------- */

static void expr_str(FILE *s, const Expr *e) {
    switch (e->k) {
    case EX_UNARY:
        fprintf(s, "(%s:%s ", e->texto, NOME_TIPO[e->tipo]);
        expr_str(s, e->a);
        fputc(')', s);
        break;
    case EX_BINARY:
        fprintf(s, "(%s:%s ", e->texto, NOME_TIPO[e->tipo]);
        expr_str(s, e->a);
        fputc(' ', s);
        expr_str(s, e->b);
        fputc(')', s);
        break;
    default: fprintf(s, "%s:%s", e->texto, NOME_TIPO[e->tipo]);
    }
}

static void campo(FILE *s, int nivel, const char *rotulo, const Expr *e) {
    fprintf(s, "%*s%s: ", 2 * nivel, "", rotulo);
    if (e) expr_str(s, e);
    else fputs("<ausente>", s);
    fputc('\n', s);
}

static void no_str(FILE *s, const No *n, int nivel, const char *rotulo) {
    fprintf(s, "%*s", 2 * nivel, "");
    if (rotulo) fprintf(s, "%s: ", rotulo);
    switch (n->k) {
    case ND_VARDECL:
        fprintf(s, "VarDecl %s : %s [%s] @%d:%d\n", n->nome, NOME_TIPO[n->tipo], n->escopo, n->linha, n->coluna);
        if (n->e1) campo(s, nivel + 1, "init", n->e1);
        break;
    case ND_BLOCK:
        fprintf(s, "Block [%s] @%d:%d%s\n", n->escopo, n->linha, n->coluna, n->n ? "" : " (vazio)");
        for (size_t i = 0; i < n->n; i++) no_str(s, n->itens[i], nivel + 1, NULL);
        break;
    case ND_ASSIGN:
        fprintf(s, "Assign @%d:%d\n", n->linha, n->coluna);
        campo(s, nivel + 1, "alvo", n->e1);
        campo(s, nivel + 1, "valor", n->e2);
        break;
    case ND_IF:
        fprintf(s, "If @%d:%d\n", n->linha, n->coluna);
        campo(s, nivel + 1, "cond", n->e1);
        no_str(s, n->c1, nivel + 1, "entao");
        if (n->c2) no_str(s, n->c2, nivel + 1, "senao");
        else fprintf(s, "%*ssenao: <ausente>\n", 2 * (nivel + 1), "");
        break;
    case ND_WHILE:
        fprintf(s, "While @%d:%d\n", n->linha, n->coluna);
        campo(s, nivel + 1, "cond", n->e1);
        no_str(s, n->c1, nivel + 1, "corpo");
        break;
    case ND_RETURN:
        fprintf(s, "Return @%d:%d\n", n->linha, n->coluna);
        campo(s, nivel + 1, "valor", n->e1);
        break;
    }
}

void serializa(FILE *saida, const Programa *p) {
    fprintf(saida, "Programa%s\n", p->n ? "" : " (vazio)");
    for (size_t i = 0; i < p->n; i++) no_str(saida, p->itens[i], 1, NULL);
}
