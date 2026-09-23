/* Exercício 08 — validação de tipos como atributos sintetizados.
 *
 * Mesmas regras, mesma política de promoção e mesma saída de tipos.py (ver a
 * docstring de lá). As expressões vêm de ex06_expressoes_minic/expr.c, sem
 * alteração na struct Expr:
 *
 * - check_expr é recursiva e devolve `enum Tipo`;
 * - o atributo de cada nó fica numa tabela à parte (pares nó -> tipo, busca
 *   linear), para não mexer na árvore sintática;
 * - os diagnósticos vão para um vetor de `Diagnostic` e só são impressos no
 *   fim: a verificação nunca encerra o processo.
 *
 * Uso:  ./tipos <arquivo | ->
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ex06_expressoes_minic/expr.h"

typedef enum { T_INT, T_FLOAT, T_BOOL, T_ERRO } Tipo;
static const char *NOME_TIPO[] = {"int", "float", "bool", "erro"};

typedef struct {
    int linha, coluna;
    char *mensagem;
} Diagnostic;

typedef struct {
    const Expr *no;
    Tipo tipo;
} Anotacao;

typedef struct {
    char *nome;
    Tipo tipo;
} Var;

typedef struct {
    Var *env;
    size_t n_env, cap_env;
    Anotacao *tipos;
    size_t n_tipos, cap_tipos;
    Diagnostic *diags;
    size_t n_diags, cap_diags;
} Verificador;

static void diag(Verificador *v, int linha, int coluna, char *mensagem) {
    if (v->n_diags == v->cap_diags) {
        v->cap_diags = v->cap_diags ? v->cap_diags * 2 : 8;
        v->diags = realoca(v->diags, v->cap_diags * sizeof(Diagnostic));
    }
    v->diags[v->n_diags++] = (Diagnostic){linha, coluna, mensagem};
}

static const Var *env_procura(const Verificador *v, const char *nome) {
    for (size_t i = 0; i < v->n_env; i++) {
        if (strcmp(v->env[i].nome, nome) == 0) return &v->env[i];
    }
    return NULL;
}

static void declara(Verificador *v, const Token *t, Tipo tipo) {
    if (env_procura(v, t->lexeme)) {
        diag(v, t->line, t->column, formata("redeclaração de '%s'", t->lexeme));
        return;
    }
    if (v->n_env == v->cap_env) {
        v->cap_env = v->cap_env ? v->cap_env * 2 : 8;
        v->env = realoca(v->env, v->cap_env * sizeof(Var));
    }
    v->env[v->n_env++] = (Var){copia(t->lexeme), tipo};
}

static Tipo anota(Verificador *v, const Expr *no, Tipo tipo) {
    if (v->n_tipos == v->cap_tipos) {
        v->cap_tipos = v->cap_tipos ? v->cap_tipos * 2 : 16;
        v->tipos = realoca(v->tipos, v->cap_tipos * sizeof(Anotacao));
    }
    v->tipos[v->n_tipos++] = (Anotacao){no, tipo};
    return tipo;
}

static Tipo tipo_de(const Verificador *v, const Expr *no) {
    for (size_t i = v->n_tipos; i-- > 0;) {
        if (v->tipos[i].no == no) return v->tipos[i].tipo;
    }
    return T_ERRO; /* inalcançável: todo nó é anotado antes de ser impresso */
}

static int numerico(Tipo t) { return t == T_INT || t == T_FLOAT; }

static Tipo binario(Verificador *v, const Expr *no, Tipo a, Tipo b) {
    const char *op = no->texto;
    const char *exigido;
    int nums = numerico(a) && numerico(b);
    if (strchr("+-*/", op[0]) && op[1] == '\0') {
        if (nums) return (a == T_FLOAT || b == T_FLOAT) ? T_FLOAT : T_INT;
        exigido = "numéricos";
    } else if (strcmp(op, "%") == 0) {
        if (a == T_INT && b == T_INT) return T_INT;
        exigido = "int";
    } else if (strcmp(op, "<") == 0 || strcmp(op, "<=") == 0 || strcmp(op, ">") == 0 || strcmp(op, ">=") == 0) {
        if (nums) return T_BOOL;
        exigido = "numéricos";
    } else if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
        if (nums || (a == T_BOOL && b == T_BOOL)) return T_BOOL;
        exigido = "do mesmo tipo (numéricos ou bool)";
    } else { /* && || */
        if (a == T_BOOL && b == T_BOOL) return T_BOOL;
        exigido = "bool";
    }
    diag(v, no->linha, no->coluna,
         formata("operador '%s' exige operandos %s; recebeu %s e %s", op, exigido, NOME_TIPO[a], NOME_TIPO[b]));
    return T_ERRO;
}

static Tipo check_expr(Verificador *v, const Expr *no) {
    Tipo t = T_ERRO;
    switch (no->k) {
    case E_INT: t = T_INT; break;
    case E_FLOAT: t = T_FLOAT; break;
    case E_BOOL: t = T_BOOL; break;
    case E_IDENT: {
        const Var *var = env_procura(v, no->texto);
        if (var) t = var->tipo;
        else diag(v, no->linha, no->coluna, formata("identificador '%s' não declarado", no->texto));
        break;
    }
    case E_UNARY: {
        Tipo o = check_expr(v, no->a);
        if (o == T_ERRO) break;
        if (no->texto[0] == '-' && numerico(o)) t = o;
        else if (no->texto[0] == '!' && o == T_BOOL) t = T_BOOL;
        else
            diag(v, no->linha, no->coluna,
                 formata("operador '%s' exige operando %s; recebeu %s", no->texto,
                         no->texto[0] == '-' ? "numérico" : "bool", NOME_TIPO[o]));
        break;
    }
    case E_BINARY: {
        Tipo a = check_expr(v, no->a);
        Tipo b = check_expr(v, no->b);
        if (a != T_ERRO && b != T_ERRO) t = binario(v, no, a, b);
        break;
    }
    }
    return anota(v, no, t);
}

static void anotada(const Verificador *v, const Expr *no) {
    const char *t = NOME_TIPO[tipo_de(v, no)];
    switch (no->k) {
    case E_UNARY:
        printf("(%s:%s ", no->texto, t);
        anotada(v, no->a);
        putchar(')');
        break;
    case E_BINARY:
        printf("(%s:%s ", no->texto, t);
        anotada(v, no->a);
        putchar(' ');
        anotada(v, no->b);
        putchar(')');
        break;
    default: printf("%s:%s", no->texto, t);
    }
}

/* Itens lidos na fase sintática: uma declaração (tipo + tokens) ou uma expressão. */
typedef struct {
    int e_decl;
    Tipo tipo;
    const Token **ids;
    size_t n_ids;
    Expr *expr;
} Item;

static Tipo tipo_decl(TokenType t) {
    return t == TOK_KW_INT ? T_INT : t == TOK_KW_FLOAT ? T_FLOAT : t == TOK_KW_BOOL ? T_BOOL : T_ERRO;
}

static const Token *exige(Fluxo *fl, TokenType tipo, const char *esperado) {
    if (fluxo_verifica(fl, tipo)) return fluxo_proximo(fl);
    erro_sintaxe(fluxo_atual(fl), esperado);
    return NULL;
}

/* Fase sintática: preenche *itens; devolve OK ou ERRO_SINTAXE. */
static int analisa(Fluxo *fl, Item **itens, size_t *n) {
    size_t cap = 0;
    while (!fluxo_verifica(fl, TOK_EOF)) {
        if (*n == cap) {
            cap = cap ? cap * 2 : 8;
            *itens = realoca(*itens, cap * sizeof(Item));
        }
        Item *it = &(*itens)[*n];
        *it = (Item){0};
        Tipo td = tipo_decl(fluxo_atual(fl)->type);
        if (td != T_ERRO) {
            fluxo_proximo(fl);
            it->e_decl = 1;
            it->tipo = td;
            (*n)++; /* já conta, para ser liberado mesmo se der erro */
            do {
                const Token *id = exige(fl, TOK_IDENT, "identificador");
                if (!id) return ERRO_SINTAXE;
                it->ids = realoca(it->ids, (it->n_ids + 1) * sizeof(Token *));
                it->ids[it->n_ids++] = id;
            } while (fluxo_aceita(fl, TOK_COMMA));
            if (!exige(fl, TOK_SEMICOLON, "',' ou ';'")) return ERRO_SINTAXE;
        } else {
            ExprErro erro;
            it->expr = expr_parse(fl, &erro);
            if (!it->expr) {
                erro_sintaxe(erro.token, erro.esperado);
                return ERRO_SINTAXE;
            }
            (*n)++;
            if (!exige(fl, TOK_SEMICOLON, "operador ou ';'")) return ERRO_SINTAXE;
        }
    }
    return OK;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "uso: %s <arquivo | ->\n", argv[0]);
        return ERRO_USO;
    }
    Fluxo fl;
    int codigo = fluxo_abrir(&fl, argv[1]);
    if (codigo != OK) return codigo;

    Item *itens = NULL;
    size_t n = 0;
    codigo = analisa(&fl, &itens, &n);

    Verificador v = {0};
    if (codigo == OK) {
        for (size_t i = 0; i < n; i++) {
            if (itens[i].e_decl) {
                for (size_t k = 0; k < itens[i].n_ids; k++) declara(&v, itens[i].ids[k], itens[i].tipo);
            } else {
                check_expr(&v, itens[i].expr);
                anotada(&v, itens[i].expr);
                putchar('\n');
            }
        }
        for (size_t i = 0; i < v.n_diags; i++) {
            fprintf(stderr, "Erro semântico na linha %d, coluna %d: %s.\n", v.diags[i].linha, v.diags[i].coluna,
                    v.diags[i].mensagem);
        }
        codigo = v.n_diags ? ERRO_SEMANTICO : OK;
    }

    for (size_t i = 0; i < n; i++) free(itens[i].ids), expr_free(itens[i].expr);
    free(itens);
    for (size_t i = 0; i < v.n_env; i++) free(v.env[i].nome);
    for (size_t i = 0; i < v.n_diags; i++) free(v.diags[i].mensagem);
    free(v.env), free(v.tipos), free(v.diags);
    fluxo_fechar(&fl);
    return codigo;
}
