/* Exercício 01 — calculadora de expressões com atributos sintetizados.
 *
 * Mesma gramática, mesma SDD e mesma saída de calculadora.py (ver a docstring
 * de lá para as regras semânticas). Diferenças de representação:
 *
 * - o atributo val é um `Valor` { definido, v }: o "None" do Python vira
 *   definido = 0, que se propaga sem gerar novos erros;
 * - o índice de leitura mora na struct Fluxo (comum/fluxo.h), passada por
 *   ponteiro para cada função do parser;
 * - o "throw" do erro de sintaxe vira um código de retorno: cada função devolve
 *   OK ou ERRO_SINTAXE, e o valor sai por parâmetro de saída.
 *
 * Uso:  ./calculadora [--trace] <arquivo | ->
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../comum/fluxo.h"

typedef struct {
    int definido;
    long long v;
} Valor;

typedef struct {
    Fluxo *fl;
    int trace;
    char **erros; /* erros semânticos, na ordem em que aparecem */
    size_t n_erros;
} Calc;

static void regra(const Calc *c, const char *producao, const char *texto) {
    if (c->trace) {
        printf("%-14s%s\n", producao, texto);
    }
}

static char *mostra(Valor v) { return v.definido ? formata("%lld", v.v) : copia("indefinido"); }

static void erro_sem(Calc *c, const Token *t, char *msg) {
    c->erros = realoca(c->erros, (c->n_erros + 1) * sizeof(char *));
    c->erros[c->n_erros++] =
        formata("Erro semântico na linha %d, coluna %d: %s.", t->line, t->column, msg);
    free(msg);
}

/* Operações com checagem de estouro (sem comportamento indefinido de C). */
static int soma_ok(long long a, long long b) {
    return !((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b));
}
static int sub_ok(long long a, long long b) {
    return !((b < 0 && a > LLONG_MAX + b) || (b > 0 && a < LLONG_MIN + b));
}
static int mul_ok(long long a, long long b) {
    if (a > 0) {
        return b > 0 ? a <= LLONG_MAX / b : b >= LLONG_MIN / a;
    }
    if (a == 0) {
        return 1;
    }
    return b > 0 ? a >= LLONG_MIN / b : b >= LLONG_MAX / a;
}

static Valor opera(Calc *c, const Token *op, Valor a, Valor b) {
    Valor r = {0, 0};
    if (!a.definido || !b.definido) {
        return r;
    }
    char o = op->lexeme[0];
    int ok = 1;
    if (o == '/' && b.v == 0) {
        erro_sem(c, op, formata("divisão por zero (%lld / %lld)", a.v, b.v));
        return r;
    }
    switch (o) {
    case '+': ok = soma_ok(a.v, b.v); if (ok) r.v = a.v + b.v; break;
    case '-': ok = sub_ok(a.v, b.v); if (ok) r.v = a.v - b.v; break;
    case '*': ok = mul_ok(a.v, b.v); if (ok) r.v = a.v * b.v; break;
    default: ok = !(a.v == LLONG_MIN && b.v == -1); if (ok) r.v = a.v / b.v; break;
    }
    if (!ok) {
        erro_sem(c, op, formata("estouro de inteiro de 64 bits (%lld %c %lld)", a.v, o, b.v));
        return r;
    }
    r.definido = 1;
    return r;
}

/* Registra no rastreamento uma redução binária (E -> E op T / T -> T op F). */
static void regra_binaria(const Calc *c, char nt, char filho, const Token *op, Valor a,
                          Valor b, Valor r) {
    char *sa = mostra(a), *sb = mostra(b), *sr = mostra(r);
    char *prod = formata("%c -> %c %s %c", nt, nt, op->lexeme, filho);
    char *txt = formata("%c.val = %c1.val %s %c.val = %s %s %s = %s", nt, nt, op->lexeme, filho,
                        sa, op->lexeme, sb, sr);
    regra(c, prod, txt);
    free(sa), free(sb), free(sr), free(prod), free(txt);
}

static void regra_copia(const Calc *c, const char *prod, const char *regra_txt, Valor v) {
    char *s = mostra(v);
    char *txt = formata("%s = %s", regra_txt, s);
    regra(c, prod, txt);
    free(s), free(txt);
}

static int parse_E(Calc *c, Valor *out);

static int parse_F(Calc *c, Valor *out) {
    Fluxo *fl = c->fl;
    if (fluxo_aceita(fl, TOK_LPAREN)) {
        Valor v;
        if (parse_E(c, &v) != OK) {
            return ERRO_SINTAXE;
        }
        if (!fluxo_verifica(fl, TOK_RPAREN)) {
            erro_sintaxe(fluxo_atual(fl), "')'");
            return ERRO_SINTAXE;
        }
        fluxo_proximo(fl);
        regra_copia(c, "F -> ( E )", "F.val = E.val", v);
        *out = v;
        return OK;
    }
    const Token *t = fluxo_atual(fl);
    if (t->type != TOK_INT) {
        erro_sintaxe(t, "número ou '('");
        return ERRO_SINTAXE;
    }
    fluxo_proximo(fl);
    errno = 0;
    long long n = strtoll(t->lexeme, NULL, 10);
    Valor v = {1, n};
    if (errno == ERANGE) {
        erro_sem(c, t, formata("número %s fora do intervalo de 64 bits", t->lexeme));
        v.definido = 0;
    }
    regra_copia(c, "F -> num", "F.val = num.lexval", v);
    *out = v;
    return OK;
}

static int parse_T(Calc *c, Valor *out) {
    Valor val;
    if (parse_F(c, &val) != OK) {
        return ERRO_SINTAXE;
    }
    regra_copia(c, "T -> F", "T.val = F.val", val);
    while (fluxo_verifica(c->fl, TOK_STAR) || fluxo_verifica(c->fl, TOK_SLASH)) {
        const Token *op = fluxo_proximo(c->fl);
        Valor f;
        if (parse_F(c, &f) != OK) {
            return ERRO_SINTAXE;
        }
        Valor novo = opera(c, op, val, f);
        regra_binaria(c, 'T', 'F', op, val, f, novo);
        val = novo;
    }
    *out = val;
    return OK;
}

static int parse_E(Calc *c, Valor *out) {
    Valor val;
    if (parse_T(c, &val) != OK) {
        return ERRO_SINTAXE;
    }
    regra_copia(c, "E -> T", "E.val = T.val", val);
    while (fluxo_verifica(c->fl, TOK_PLUS) || fluxo_verifica(c->fl, TOK_MINUS)) {
        const Token *op = fluxo_proximo(c->fl);
        Valor t;
        if (parse_T(c, &t) != OK) {
            return ERRO_SINTAXE;
        }
        Valor novo = opera(c, op, val, t);
        regra_binaria(c, 'E', 'T', op, val, t, novo);
        val = novo;
    }
    *out = val;
    return OK;
}

int main(int argc, char **argv) {
    int trace = 0;
    const char *arquivo = NULL;
    int n_args = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) {
            trace = 1;
        } else {
            arquivo = argv[i];
            n_args++;
        }
    }
    if (n_args != 1) {
        fprintf(stderr, "uso: %s [--trace] <arquivo | ->\n", argv[0]);
        return ERRO_USO;
    }

    Fluxo fl;
    int codigo = fluxo_abrir(&fl, arquivo);
    if (codigo != OK) {
        return codigo;
    }

    Calc c = {&fl, trace, NULL, 0};
    Valor val;
    codigo = parse_E(&c, &val);
    if (codigo == OK && !fluxo_verifica(&fl, TOK_EOF)) {
        erro_sintaxe(fluxo_atual(&fl), "operador ou fim da entrada");
        codigo = ERRO_SINTAXE;
    }

    if (codigo == OK && c.n_erros > 0) {
        for (size_t i = 0; i < c.n_erros; i++) {
            fprintf(stderr, "%s\n", c.erros[i]);
        }
        codigo = ERRO_SEMANTICO;
    } else if (codigo == OK) {
        printf("resultado = %lld\n", val.v);
    }

    for (size_t i = 0; i < c.n_erros; i++) {
        free(c.erros[i]);
    }
    free(c.erros);
    fluxo_fechar(&fl);
    return codigo;
}
