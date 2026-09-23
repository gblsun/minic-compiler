/* Exercício 04 — propagação de tipo herdado em listas de identificadores.
 *
 * Mesma gramática, mesma SDD L-atribuída e mesma saída de declaracoes.py (ver
 * a docstring de lá para as regras e a justificativa). Em C:
 *
 * - o atributo herdado L.inh é o parâmetro `inh` de parse_L;
 * - a tabela de símbolos é um vetor dinâmico em ordem de declaração, com busca
 *   linear de duplicatas (O(n) por inserção);
 * - a tabela é dona das strings de nome; os tipos são literais estáticos.
 *
 * Uso:  ./declaracoes [--trace] <arquivo | ->
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comum/fluxo.h"

typedef struct {
    char *nome;
    const char *tipo;
    int linha, coluna;
} Simbolo;

typedef struct {
    Simbolo *v;
    size_t n, cap;
} Tabela;

static const Simbolo *tabela_procura(const Tabela *t, const char *nome) {
    for (size_t i = 0; i < t->n; i++) {
        if (strcmp(t->v[i].nome, nome) == 0) return &t->v[i];
    }
    return NULL;
}

static void tabela_insere(Tabela *t, const Token *tok, const char *tipo) {
    if (t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 8;
        t->v = realoca(t->v, t->cap * sizeof(Simbolo));
    }
    t->v[t->n++] = (Simbolo){copia(tok->lexeme), tipo, tok->line, tok->column};
}

static void tabela_free(Tabela *t) {
    for (size_t i = 0; i < t->n; i++) free(t->v[i].nome);
    free(t->v);
}

typedef struct {
    Fluxo *fl;
    int trace;
    Tabela tabela;
    char **erros;
    size_t n_erros;
} Declaracoes;

static void regra(const Declaracoes *d, const char *prod, const char *acao) {
    if (d->trace) printf("%-14s%s\n", prod, acao);
}

/* Ação semântica addtype(id, L.inh). */
static void addtype(Declaracoes *d, const Token *tok, const char *tipo, const char *prod) {
    const Simbolo *anterior = tabela_procura(&d->tabela, tok->lexeme);
    if (anterior) {
        char *acao = formata("addtype(%s, L.inh) -> redeclaração", tok->lexeme);
        regra(d, prod, acao);
        free(acao);
        d->erros = realoca(d->erros, (d->n_erros + 1) * sizeof(char *));
        d->erros[d->n_erros++] = formata(
            "Erro semântico na linha %d, coluna %d: redeclaração de '%s' (declarado na linha %d, coluna %d).",
            tok->line, tok->column, tok->lexeme, anterior->linha, anterior->coluna);
        return;
    }
    char *acao = formata("addtype(%s, L.inh) -> %s : %s", tok->lexeme, tok->lexeme, tipo);
    regra(d, prod, acao);
    free(acao);
    tabela_insere(&d->tabela, tok, tipo);
}

/* Consome um token do tipo pedido; senão imprime o erro e devolve NULL. */
static const Token *exige(Fluxo *fl, TokenType tipo, const char *esperado) {
    if (fluxo_verifica(fl, tipo)) return fluxo_proximo(fl);
    erro_sintaxe(fluxo_atual(fl), esperado);
    return NULL;
}

/* T -> int | float | char; devolve T.type (NULL em erro de sintaxe). */
static const char *parse_T(Declaracoes *d) {
    const Token *t = fluxo_atual(d->fl);
    const char *tipo = t->type == TOK_KW_INT     ? "int"
                       : t->type == TOK_KW_FLOAT ? "float"
                       : t->type == TOK_KW_CHAR  ? "char"
                                                 : NULL;
    if (!tipo) {
        erro_sintaxe(t, "tipo (int, float ou char)");
        return NULL;
    }
    fluxo_proximo(d->fl);
    char *prod = formata("T -> %s", tipo), *acao = formata("T.type = %s", tipo);
    regra(d, prod, acao);
    free(prod), free(acao);
    return tipo;
}

/* L -> id { , id }; `inh` é o atributo herdado L.inh. */
static int parse_L(Declaracoes *d, const char *inh) {
    const Token *id = exige(d->fl, TOK_IDENT, "identificador");
    if (!id) return ERRO_SINTAXE;
    addtype(d, id, inh, "L -> id");
    while (fluxo_aceita(d->fl, TOK_COMMA)) {
        if (!(id = exige(d->fl, TOK_IDENT, "identificador"))) return ERRO_SINTAXE;
        addtype(d, id, inh, "L -> L , id");
    }
    return OK;
}

static int parse_D(Declaracoes *d) {
    const char *tipo = parse_T(d);
    if (!tipo) return ERRO_SINTAXE;
    char *acao = formata("L.inh = T.type = %s", tipo);
    regra(d, "D -> T L ;", acao);
    free(acao);
    if (parse_L(d, tipo) != OK) return ERRO_SINTAXE;
    return exige(d->fl, TOK_SEMICOLON, "',' ou ';'") ? OK : ERRO_SINTAXE;
}

int main(int argc, char **argv) {
    int trace = 0, n_args = 0;
    const char *arquivo = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) trace = 1;
        else arquivo = argv[i], n_args++;
    }
    if (n_args != 1) {
        fprintf(stderr, "uso: %s [--trace] <arquivo | ->\n", argv[0]);
        return ERRO_USO;
    }
    Fluxo fl;
    int codigo = fluxo_abrir(&fl, arquivo);
    if (codigo != OK) return codigo;

    Declaracoes d = {&fl, trace, {0}, NULL, 0};
    while (codigo == OK && !fluxo_verifica(&fl, TOK_EOF)) {
        codigo = parse_D(&d);
    }
    if (codigo == OK) {
        printf("tabela de símbolos:\n");
        for (size_t i = 0; i < d.tabela.n; i++) {
            const Simbolo *s = &d.tabela.v[i];
            printf("  %s : %s (linha %d, coluna %d)\n", s->nome, s->tipo, s->linha, s->coluna);
        }
        for (size_t i = 0; i < d.n_erros; i++) fprintf(stderr, "%s\n", d.erros[i]);
        codigo = d.n_erros ? ERRO_SEMANTICO : OK;
    }
    for (size_t i = 0; i < d.n_erros; i++) free(d.erros[i]);
    free(d.erros);
    tabela_free(&d.tabela);
    fluxo_fechar(&fl);
    return codigo;
}
