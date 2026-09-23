/* Exercício 02 — grafo de dependências e avaliador topológico.
 *
 * Mesma entrada, mesmas regras e mesma saída de grafo.py (ver a docstring de
 * lá). Representação em C:
 *
 * - cada atributo é um nó num vetor dinâmico (`Attr`), identificado pelo
 *   índice; os argumentos das ações são resolvidos de nome para índice depois
 *   de ler o arquivo inteiro (busca linear: O(n^2), suficiente para os grafos
 *   de uma SDD de exemplo);
 * - as arestas ficam em listas de adjacência (vetores dinâmicos de índices);
 * - o algoritmo de Kahn usa uma fila em vetor (cada nó entra no máximo uma
 *   vez, então n posições bastam).
 *
 * Não há limite fixo de atributos, de tamanho de nome ou de linha: tudo é
 * alocado dinamicamente. Separadores reconhecidos: espaço, tab e \r.
 *
 * Uso:  ./grafo <arquivo | ->
 */

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { OK = 0, ERRO_USO = 1, ERRO_SINTAXE = 3, ERRO_SEMANTICO = 4 };
typedef enum { A_CONST, A_COPIA, A_SOMA, A_SUB, A_MUL, A_DIV } Acao;
static const char *NOME_ACAO[] = {"const", "copia", "soma", "sub", "mul", "div"};

typedef struct {
    char *nome;
    int linha;
    Acao acao;
    long long cte;    /* só para const */
    char *arg_txt[2]; /* nomes dos argumentos, como escritos */
    int arg[2];       /* índices resolvidos */
    int nargs;
    int *suc; /* sucessores (quem depende deste atributo) */
    size_t n_suc, cap_suc;
    int grau; /* grau de entrada, para o Kahn */
    long long valor;
} Attr;

typedef struct {
    Attr *v;
    size_t n, cap;
} Grafo;

static void *aloca(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(ERRO_USO);
    }
    return p;
}

static void *realoca(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(ERRO_USO);
    }
    return q;
}

static char *copia_n(const char *s, size_t n) {
    char *c = aloca(n + 1);
    memcpy(c, s, n);
    c[n] = '\0';
    return c;
}

/* Erro com código: imprime em stderr e devolve o código, para `return erro(...)`. */
static int erro(int codigo, const char *fmt, ...) {
    va_list a;
    va_start(a, fmt);
    vfprintf(stderr, fmt, a);
    va_end(a);
    fputc('\n', stderr);
    return codigo;
}

static void grafo_free(Grafo *g) {
    for (size_t i = 0; i < g->n; i++) {
        free(g->v[i].nome);
        free(g->v[i].arg_txt[0]);
        free(g->v[i].arg_txt[1]);
        free(g->v[i].suc);
    }
    free(g->v);
}

static int procura(const Grafo *g, const char *nome) {
    for (size_t i = 0; i < g->n; i++) {
        if (strcmp(g->v[i].nome, nome) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static int e_sep(char c) { return c == ' ' || c == '\t' || c == '\r'; }

/* Quebra a linha (sem o comentário) em palavras; devolve quantas (até max). */
static size_t palavras(const char *ini, const char *fim, char **out, size_t max) {
    size_t n = 0;
    const char *p = ini;
    while (p < fim) {
        while (p < fim && e_sep(*p)) p++;
        if (p >= fim) break;
        const char *q = p;
        while (q < fim && !e_sep(*q)) q++;
        if (n < max) out[n] = copia_n(p, (size_t)(q - p));
        n++;
        p = q;
    }
    return n;
}

static int inteiro(const char *txt, int linha, long long *out) {
    const char *d = txt[0] == '-' ? txt + 1 : txt;
    if (*d == '\0') {
        return erro(ERRO_SINTAXE, "Erro de sintaxe na linha %d: 'const' espera um inteiro, encontrado '%s'.", linha, txt);
    }
    for (const char *p = d; *p; p++) {
        if (*p < '0' || *p > '9') {
            return erro(ERRO_SINTAXE, "Erro de sintaxe na linha %d: 'const' espera um inteiro, encontrado '%s'.", linha, txt);
        }
    }
    errno = 0;
    *out = strtoll(txt, NULL, 10);
    if (errno == ERANGE) {
        return erro(ERRO_SINTAXE, "Erro de sintaxe na linha %d: inteiro '%s' fora do intervalo de 64 bits.", linha, txt);
    }
    return OK;
}

static void libera_palavras(char **p, size_t n, size_t max) {
    for (size_t i = 0; i < n && i < max; i++) free(p[i]);
}

/* Lê a descrição; preenche g e o índice do resultado. */
static int ler_grafo(const char *texto, Grafo *g, int *resultado) {
    char *res_nome = NULL;
    int res_linha = 0;
    int linha = 0;
    const char *p = texto;
    int codigo = OK;
    while (*p && codigo == OK) {
        linha++;
        const char *fim = strchr(p, '\n');
        if (!fim) fim = p + strlen(p);
        const char *com = memchr(p, '#', (size_t)(fim - p));
        char *w[5] = {0};
        size_t n = palavras(p, com ? com : fim, w, 5);
        p = *fim ? fim + 1 : fim;
        if (n == 0) continue;

        if (strcmp(w[0], "resultado") == 0) {
            if (n != 2) {
                codigo = erro(ERRO_SINTAXE, "Erro de sintaxe na linha %d: 'resultado' espera um atributo.", linha);
            } else {
                free(res_nome);
                res_nome = w[1];
                w[1] = NULL;
                res_linha = linha;
            }
            libera_palavras(w, n, 5);
            continue;
        }
        if (n < 3 || strcmp(w[1], "=") != 0) {
            codigo = erro(ERRO_SINTAXE, "Erro de sintaxe na linha %d: esperado '<atributo> = <ação> <argumentos>'.", linha);
            libera_palavras(w, n, 5);
            break;
        }
        int acao = -1;
        for (int i = 0; i < 6; i++) {
            if (strcmp(w[2], NOME_ACAO[i]) == 0) acao = i;
        }
        if (acao < 0) {
            codigo = erro(ERRO_SINTAXE, "Erro de sintaxe na linha %d: ação desconhecida '%s'.", linha, w[2]);
            libera_palavras(w, n, 5);
            break;
        }
        size_t aridade = acao <= A_COPIA ? 1 : 2;
        if (n - 3 != aridade) {
            codigo = erro(ERRO_SINTAXE, "Erro de sintaxe na linha %d: '%s' espera %zu argumento(s).", linha, w[2], aridade);
            libera_palavras(w, n, 5);
            break;
        }
        Attr a = {0};
        a.linha = linha;
        a.acao = (Acao)acao;
        a.nargs = (int)aridade;
        if (acao == A_CONST) {
            codigo = inteiro(w[3], linha, &a.cte);
        } else {
            a.arg_txt[0] = w[3];
            w[3] = NULL;
            if (aridade == 2) {
                a.arg_txt[1] = w[4];
                w[4] = NULL;
            }
        }
        int anterior = procura(g, w[0]);
        if (codigo == OK && anterior >= 0) {
            codigo = erro(ERRO_SEMANTICO, "Erro semântico na linha %d: atributo '%s' já definido na linha %d.",
                          linha, w[0], g->v[anterior].linha);
        }
        if (codigo != OK) {
            free(a.arg_txt[0]);
            free(a.arg_txt[1]);
            libera_palavras(w, n, 5);
            break;
        }
        a.nome = w[0];
        w[0] = NULL;
        libera_palavras(w, n, 5);
        if (g->n == g->cap) {
            g->cap = g->cap ? g->cap * 2 : 8;
            g->v = realoca(g->v, g->cap * sizeof(Attr));
        }
        g->v[g->n++] = a;
    }

    if (codigo == OK && g->n == 0) {
        codigo = erro(ERRO_SINTAXE, "Erro de sintaxe: nenhum atributo definido.");
    }
    /* Arestas argumento -> atributo, na ordem de declaração. */
    for (size_t i = 0; codigo == OK && i < g->n; i++) {
        Attr *a = &g->v[i];
        if (a->acao == A_CONST) continue;
        for (int k = 0; k < a->nargs && codigo == OK; k++) {
            int j = procura(g, a->arg_txt[k]);
            if (j < 0) {
                codigo = erro(ERRO_SEMANTICO, "Erro semântico na linha %d: atributo '%s' usado por '%s' não foi definido.",
                              a->linha, a->arg_txt[k], a->nome);
                break;
            }
            a->arg[k] = j;
            Attr *orig = &g->v[j];
            int repetida = 0;
            for (size_t s = 0; s < orig->n_suc; s++) {
                if (orig->suc[s] == (int)i) repetida = 1;
            }
            if (!repetida) {
                if (orig->n_suc == orig->cap_suc) {
                    orig->cap_suc = orig->cap_suc ? orig->cap_suc * 2 : 4;
                    orig->suc = realoca(orig->suc, orig->cap_suc * sizeof(int));
                }
                orig->suc[orig->n_suc++] = (int)i;
            }
        }
    }
    if (codigo == OK) {
        if (res_nome == NULL) {
            *resultado = (int)g->n - 1;
        } else if ((*resultado = procura(g, res_nome)) < 0) {
            codigo = erro(ERRO_SEMANTICO, "Erro semântico na linha %d: atributo '%s' não foi definido.", res_linha, res_nome);
        }
    }
    free(res_nome);
    return codigo;
}

/* Kahn: preenche `ordem` (n posições) e devolve quantos nós entraram nela. */
static size_t ordem_topologica(Grafo *g, int *ordem) {
    for (size_t i = 0; i < g->n; i++) g->v[i].grau = 0;
    for (size_t i = 0; i < g->n; i++) {
        for (size_t s = 0; s < g->v[i].n_suc; s++) g->v[g->v[i].suc[s]].grau++;
    }
    size_t ini = 0, fim = 0; /* a fila é o próprio vetor `ordem` */
    for (size_t i = 0; i < g->n; i++) {
        if (g->v[i].grau == 0) ordem[fim++] = (int)i;
    }
    while (ini < fim) {
        Attr *a = &g->v[ordem[ini++]];
        for (size_t s = 0; s < a->n_suc; s++) {
            if (--g->v[a->suc[s]].grau == 0) ordem[fim++] = a->suc[s];
        }
    }
    return fim;
}

static int aplica(Acao acao, long long a, long long b, long long *r, const char **erro_txt) {
    int ok = 1;
    switch (acao) {
    case A_SOMA:
        ok = !((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b));
        if (ok) *r = a + b;
        break;
    case A_SUB:
        ok = !((b < 0 && a > LLONG_MAX + b) || (b > 0 && a < LLONG_MIN + b));
        if (ok) *r = a - b;
        break;
    case A_MUL:
        if (a > 0) ok = b > 0 ? a <= LLONG_MAX / b : b >= LLONG_MIN / a;
        else if (a < 0) ok = b > 0 ? a >= LLONG_MIN / b : b >= LLONG_MAX / a;
        if (ok) *r = a * b;
        break;
    default:
        if (b == 0) {
            *erro_txt = "divisão por zero";
            return 0;
        }
        ok = !(a == LLONG_MIN && b == -1);
        if (ok) *r = a / b; /* C já trunca em direção a zero */
        break;
    }
    if (!ok) *erro_txt = "estouro de inteiro de 64 bits";
    return ok;
}

static int evaluate(Grafo *g) {
    int *ordem = aloca(g->n * sizeof(int));
    size_t n = ordem_topologica(g, ordem);
    int codigo = OK;
    if (n < g->n) {
        fprintf(stderr, "Erro semântico: ciclo de dependências; atributos bloqueados: ");
        int primeiro = 1;
        for (size_t i = 0; i < g->n; i++) {
            if (g->v[i].grau > 0) {
                fprintf(stderr, "%s%s", primeiro ? "" : ", ", g->v[i].nome);
                primeiro = 0;
            }
        }
        fprintf(stderr, ".\n");
        free(ordem);
        return ERRO_SEMANTICO;
    }
    for (size_t i = 0; i < n && codigo == OK; i++) {
        Attr *a = &g->v[ordem[i]];
        if (a->acao == A_CONST) {
            a->valor = a->cte;
            printf("  %zu. %s := %lld\n", i + 1, a->nome, a->valor);
        } else if (a->acao == A_COPIA) {
            a->valor = g->v[a->arg[0]].valor;
            printf("  %zu. %s := %s = %lld\n", i + 1, a->nome, a->arg_txt[0], a->valor);
        } else {
            long long x = g->v[a->arg[0]].valor, y = g->v[a->arg[1]].valor;
            const char *erro_txt = NULL;
            if (!aplica(a->acao, x, y, &a->valor, &erro_txt)) {
                codigo = erro(ERRO_SEMANTICO, "Erro semântico na linha %d: %s ao avaliar '%s' (%s(%lld, %lld)).",
                              a->linha, erro_txt, a->nome, NOME_ACAO[a->acao], x, y);
                break;
            }
            const char *op = NOME_ACAO[a->acao];
            printf("  %zu. %s := %s(%s, %s) = %s(%lld, %lld) = %lld\n", i + 1, a->nome, op, a->arg_txt[0],
                   a->arg_txt[1], op, x, y, a->valor);
        }
    }
    free(ordem);
    return codigo;
}

static char *ler_arquivo(const char *caminho) {
    FILE *f = strcmp(caminho, "-") == 0 ? stdin : fopen(caminho, "rb");
    if (!f) {
        fprintf(stderr, "erro ao abrir '%s': %s\n", caminho, strerror(errno));
        return NULL;
    }
    size_t cap = 4096, len = 0, lidos;
    char *buf = aloca(cap);
    while ((lidos = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += lidos;
        if (len + 1 == cap) buf = realoca(buf, cap *= 2);
    }
    if (f != stdin) fclose(f);
    buf[len] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "uso: %s <arquivo | ->\n", argv[0]);
        return ERRO_USO;
    }
    char *texto = ler_arquivo(argv[1]);
    if (!texto) return ERRO_USO;

    Grafo g = {0};
    int resultado = -1;
    int codigo = ler_grafo(texto, &g, &resultado);
    if (codigo == OK) {
        size_t arestas = 0;
        for (size_t i = 0; i < g.n; i++) arestas += g.v[i].n_suc;
        printf("atributos: %zu, dependências: %zu\n", g.n, arestas);
        for (size_t i = 0; i < g.n; i++) {
            for (size_t s = 0; s < g.v[i].n_suc; s++) {
                printf("  %s -> %s\n", g.v[i].nome, g.v[g.v[i].suc[s]].nome);
            }
        }
        printf("ordem de avaliação:\n");
        fflush(stdout);
        codigo = evaluate(&g);
    }
    if (codigo == OK) {
        printf("resultado: %s = %lld\n", g.v[resultado].nome, g.v[resultado].valor);
    }
    grafo_free(&g);
    free(texto);
    return codigo;
}
