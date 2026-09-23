/* Exercício 03 — construtor de AST com SDD S-atribuída.
 *
 * Mesma gramática, mesmas ações e mesma saída de ast_s.py (ver a docstring de
 * lá). Representação em C:
 *
 * - `No` é uma struct discriminada por `NoTipo` (enum) com uma union para os
 *   campos de cada tipo;
 * - **posse**: cada nó é dono dos seus filhos e do seu texto. `no_free` libera
 *   a subárvore inteira;
 * - **falha de alocação**: os construtores recebem os filhos já construídos e,
 *   se o malloc falhar, liberam esses filhos antes de devolver NULL. Assim quem
 *   chama nunca precisa limpar nada num caminho de erro: basta propagar NULL.
 *   O parser distingue os dois motivos de NULL (sintaxe ou memória) pelo campo
 *   `status`.
 *
 * Uso:  ./ast_s [--trace] <arquivo | ->
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comum/fluxo.h"

typedef enum { NO_NUMBER, NO_IDENTIFIER, NO_UNARY, NO_BINARY } NoTipo;

typedef struct No {
    NoTipo tipo;
    int linha, coluna;
    union {
        char *texto; /* NUMBER: o lexema; IDENTIFIER: o nome */
        struct {
            char op;
            struct No *operando;
        } un;
        struct {
            char op;
            struct No *esq, *dir;
        } bin;
    } u;
} No;

static void no_free(No *n) {
    if (!n) return;
    switch (n->tipo) {
    case NO_NUMBER:
    case NO_IDENTIFIER: free(n->u.texto); break;
    case NO_UNARY: no_free(n->u.un.operando); break;
    case NO_BINARY: no_free(n->u.bin.esq), no_free(n->u.bin.dir); break;
    }
    free(n);
}

/* -- construtores: devolvem NULL se faltar memória, sem vazar nada -------- */

static No *novo(NoTipo tipo, const Token *t) {
    No *n = malloc(sizeof(No));
    if (n) {
        n->tipo = tipo;
        n->linha = t->line;
        n->coluna = t->column;
    }
    return n;
}

static No *folha(NoTipo tipo, const Token *t) {
    No *n = novo(tipo, t);
    char *txt = malloc(strlen(t->lexeme) + 1);
    if (!n || !txt) {
        free(n), free(txt);
        return NULL;
    }
    strcpy(txt, t->lexeme);
    n->u.texto = txt;
    return n;
}

static No *mk_unary(const Token *op, No *operando) {
    No *n = operando ? novo(NO_UNARY, op) : NULL;
    if (!n) {
        no_free(operando); /* o construtor é dono do filho, mesmo falhando */
        return NULL;
    }
    n->u.un.op = op->lexeme[0];
    n->u.un.operando = operando;
    return n;
}

static No *mk_binary(const Token *op, No *esq, No *dir) {
    No *n = (esq && dir) ? novo(NO_BINARY, op) : NULL;
    if (!n) {
        no_free(esq), no_free(dir);
        return NULL;
    }
    n->u.bin.op = op->lexeme[0];
    n->u.bin.esq = esq;
    n->u.bin.dir = dir;
    return n;
}

/* -- parser com as ações semânticas ---------------------------------------- */

typedef enum { ST_OK, ST_SINTAXE, ST_MEMORIA } Status;

typedef struct {
    Fluxo *fl;
    int trace;
    Status status;
} Construtor;

static void regra(const Construtor *c, const char *prod, const char *acao) {
    if (c->trace) printf("%-14s%s\n", prod, acao);
}

/* Após um construtor devolver NULL sem erro de sintaxe, a causa é memória. */
static No *confere(Construtor *c, No *n) {
    if (!n && c->status == ST_OK) c->status = ST_MEMORIA;
    return n;
}

static No *parse_E(Construtor *c);

static No *parse_F(Construtor *c) {
    const Token *t = fluxo_atual(c->fl);
    if (fluxo_aceita(c->fl, TOK_MINUS)) {
        No *o = parse_F(c);
        if (!o) return NULL;
        regra(c, "F -> - F", "F.node = Unary('-', F1.node)");
        return confere(c, mk_unary(t, o));
    }
    if (fluxo_aceita(c->fl, TOK_LPAREN)) {
        No *n = parse_E(c);
        if (!n) return NULL;
        if (!fluxo_verifica(c->fl, TOK_RPAREN)) {
            erro_sintaxe(fluxo_atual(c->fl), "')'");
            c->status = ST_SINTAXE;
            no_free(n);
            return NULL;
        }
        fluxo_proximo(c->fl);
        regra(c, "F -> ( E )", "F.node = E.node");
        return n;
    }
    if (t->type == TOK_IDENT || t->type == TOK_INT) {
        fluxo_proximo(c->fl);
        char *acao = t->type == TOK_IDENT ? formata("F.node = Identifier(%s)", t->lexeme)
                                          : formata("F.node = Number(%s)", t->lexeme);
        regra(c, t->type == TOK_IDENT ? "F -> id" : "F -> num", acao);
        free(acao);
        return confere(c, folha(t->type == TOK_IDENT ? NO_IDENTIFIER : NO_NUMBER, t));
    }
    erro_sintaxe(t, "número, identificador, '-' ou '('");
    c->status = ST_SINTAXE;
    return NULL;
}

static No *parse_T(Construtor *c) {
    No *n = parse_F(c);
    if (!n) return NULL;
    regra(c, "T -> F", "T.node = F.node");
    while (fluxo_verifica(c->fl, TOK_STAR) || fluxo_verifica(c->fl, TOK_SLASH)) {
        const Token *op = fluxo_proximo(c->fl);
        No *dir = parse_F(c);
        if (!dir) {
            no_free(n);
            return NULL;
        }
        n = confere(c, mk_binary(op, n, dir));
        if (!n) return NULL;
        char *prod = formata("T -> T %s F", op->lexeme);
        char *acao = formata("T.node = Binary('%s', T1.node, F.node)", op->lexeme);
        regra(c, prod, acao);
        free(prod), free(acao);
    }
    return n;
}

static No *parse_E(Construtor *c) {
    No *n = parse_T(c);
    if (!n) return NULL;
    regra(c, "E -> T", "E.node = T.node");
    while (fluxo_verifica(c->fl, TOK_PLUS) || fluxo_verifica(c->fl, TOK_MINUS)) {
        const Token *op = fluxo_proximo(c->fl);
        No *dir = parse_T(c);
        if (!dir) {
            no_free(n);
            return NULL;
        }
        n = confere(c, mk_binary(op, n, dir));
        if (!n) return NULL;
        char *prod = formata("E -> E %s T", op->lexeme);
        char *acao = formata("E.node = Binary('%s', E1.node, T.node)", op->lexeme);
        regra(c, prod, acao);
        free(prod), free(acao);
    }
    return n;
}

/* -- travessias ------------------------------------------------------------- */

static void prefixa(const No *n) {
    switch (n->tipo) {
    case NO_NUMBER:
    case NO_IDENTIFIER: fputs(n->u.texto, stdout); break;
    case NO_UNARY:
        printf("(%c ", n->u.un.op);
        prefixa(n->u.un.operando);
        putchar(')');
        break;
    case NO_BINARY:
        printf("(%c ", n->u.bin.op);
        prefixa(n->u.bin.esq);
        putchar(' ');
        prefixa(n->u.bin.dir);
        putchar(')');
        break;
    }
}

typedef enum { AV_OK, AV_NAO_AVALIAVEL, AV_ERRO } AvStatus;

/* Em AV_NAO_AVALIAVEL, *motivo aponta o identificador; em AV_ERRO a mensagem
 * já foi impressa em stderr. */
static AvStatus avalia(const No *n, long long *out, const No **motivo) {
    long long a, b;
    AvStatus s;
    switch (n->tipo) {
    case NO_NUMBER:
        errno = 0;
        *out = strtoll(n->u.texto, NULL, 10);
        if (errno == ERANGE) {
            fprintf(stderr, "Erro semântico na linha %d, coluna %d: número %s fora do intervalo de 64 bits.\n",
                    n->linha, n->coluna, n->u.texto);
            return AV_ERRO;
        }
        return AV_OK;
    case NO_IDENTIFIER: *motivo = n; return AV_NAO_AVALIAVEL;
    case NO_UNARY:
        if ((s = avalia(n->u.un.operando, &a, motivo)) != AV_OK) return s;
        if (a == LLONG_MIN) {
            fprintf(stderr, "Erro semântico na linha %d, coluna %d: estouro de inteiro de 64 bits.\n", n->linha,
                    n->coluna);
            return AV_ERRO;
        }
        *out = -a;
        return AV_OK;
    case NO_BINARY: break;
    }
    if ((s = avalia(n->u.bin.esq, &a, motivo)) != AV_OK) return s;
    if ((s = avalia(n->u.bin.dir, &b, motivo)) != AV_OK) return s;
    char op = n->u.bin.op;
    int ok = 1;
    if (op == '/' && b == 0) {
        fprintf(stderr, "Erro semântico na linha %d, coluna %d: divisão por zero (%lld / %lld).\n", n->linha,
                n->coluna, a, b);
        return AV_ERRO;
    }
    switch (op) {
    case '+': ok = !((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b)); if (ok) *out = a + b; break;
    case '-': ok = !((b < 0 && a > LLONG_MAX + b) || (b > 0 && a < LLONG_MIN + b)); if (ok) *out = a - b; break;
    case '*':
        if (a > 0) ok = b > 0 ? a <= LLONG_MAX / b : b >= LLONG_MIN / a;
        else if (a < 0) ok = b > 0 ? a >= LLONG_MIN / b : b >= LLONG_MAX / a;
        if (ok) *out = a * b;
        break;
    default: ok = !(a == LLONG_MIN && b == -1); if (ok) *out = a / b; break;
    }
    if (!ok) {
        fprintf(stderr, "Erro semântico na linha %d, coluna %d: estouro de inteiro de 64 bits (%lld %c %lld).\n",
                n->linha, n->coluna, a, op, b);
        return AV_ERRO;
    }
    return AV_OK;
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

    Construtor c = {&fl, trace, ST_OK};
    No *arvore = parse_E(&c);
    if (arvore && !fluxo_verifica(&fl, TOK_EOF)) {
        erro_sintaxe(fluxo_atual(&fl), "operador ou fim da entrada");
        no_free(arvore);
        arvore = NULL;
        c.status = ST_SINTAXE;
    }
    if (!arvore) {
        if (c.status == ST_MEMORIA) fprintf(stderr, "erro: memória insuficiente ao construir a AST\n");
        fluxo_fechar(&fl);
        return c.status == ST_MEMORIA ? ERRO_USO : ERRO_SINTAXE;
    }

    fputs("prefixa: ", stdout);
    prefixa(arvore);
    putchar('\n');
    fflush(stdout);
    long long v = 0;
    const No *motivo = NULL;
    AvStatus s = avalia(arvore, &v, &motivo);
    if (s == AV_OK) {
        printf("valor: %lld\n", v);
    } else if (s == AV_NAO_AVALIAVEL) {
        printf("valor: não avaliável (identificador '%s' na linha %d, coluna %d)\n", motivo->u.texto, motivo->linha,
               motivo->coluna);
    } else {
        codigo = ERRO_SEMANTICO;
    }
    no_free(arvore);
    fluxo_fechar(&fl);
    return codigo;
}
