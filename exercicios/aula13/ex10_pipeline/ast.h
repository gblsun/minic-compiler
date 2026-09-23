/* Nós da AST do mini pipeline (exercício 10) — equivalente C de ast_nodes.py.
 *
 * Os nós já nascem com os atributos calculados pelas ações do parser: toda
 * expressão tem o `tipo` sintetizado; toda declaração, o tipo e o escopo
 * herdados. TY_ERRO marca uma subexpressão que já gerou diagnóstico.
 *
 * Posse: todos os nós de uma análise são registrados numa `Arena`, que é a
 * dona deles. `arena_free` libera tudo de uma vez — inclusive nós de uma
 * análise interrompida por erro de sintaxe, que nunca chegaram a ser ligados
 * à árvore. Os nós não devem ser liberados individualmente.
 */

#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdio.h>

typedef enum { TY_INT, TY_FLOAT, TY_BOOL, TY_ERRO } Tipo;
extern const char *const NOME_TIPO[];

typedef enum { EX_LITERAL, EX_ID, EX_UNARY, EX_BINARY } ExprKind;

typedef struct Expr {
    ExprKind k;
    char *texto; /* lexema, nome ou operador */
    Tipo tipo;   /* atributo sintetizado */
    int linha, coluna;
    struct Expr *a, *b; /* UNARY: a; BINARY: a, b */
} Expr;

typedef enum { ND_VARDECL, ND_BLOCK, ND_ASSIGN, ND_IF, ND_WHILE, ND_RETURN } NoKind;

typedef struct No {
    NoKind k;
    int linha, coluna;
    char *nome;   /* VARDECL */
    Tipo tipo;    /* VARDECL: herdado */
    char *escopo; /* VARDECL/BLOCK: herdado / criado */
    Expr *e1;     /* VARDECL: init; ASSIGN: alvo; IF/WHILE: cond; RETURN: valor */
    Expr *e2;     /* ASSIGN: valor */
    struct No *c1, *c2; /* IF: entao/senao; WHILE: corpo */
    struct No **itens;  /* BLOCK */
    size_t n, cap;
} No;

typedef struct {
    void **ptr;
    int *e_no; /* 1 = No, 0 = Expr */
    size_t n, cap;
} Arena;

typedef struct {
    No **itens;
    size_t n, cap;
    Arena arena;
} Programa;

/* Utilitários de memória (abortam com mensagem se faltar memória). */
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xformat(const char *fmt, ...);

Expr *expr_novo(Arena *ar, ExprKind k, const char *texto, Tipo tipo, int linha, int coluna, Expr *a, Expr *b);
No *no_novo(Arena *ar, NoKind k, int linha, int coluna);
void no_add(No *bloco, No *item);
void programa_add(Programa *p, No *item);
void arena_free(Arena *ar);
void programa_free(Programa *p);

void serializa(FILE *saida, const Programa *p);

#endif /* AST_H */
