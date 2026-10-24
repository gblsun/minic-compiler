/* Otimizador da IR do MINIC — ver otimizador.h e src/python/otimizador.py.
 *
 * Como todas as strings da IR estão internadas no pool do programa (ir.h),
 * "mesmo nome" é "mesmo ponteiro": conjuntos de nomes e fatos são vetores de
 * ponteiros, comparados com ==. Os programas são pequenos, então busca linear
 * basta.
 */

#include "otimizador.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *ot_realloc(void *ptr, size_t tamanho) {
    void *novo = realloc(ptr, tamanho);
    if (!novo) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    return novo;
}

#define OT_PUSH(arr, len, cap, item)                                     \
    do {                                                                 \
        if ((len) == (cap)) {                                            \
            (cap) = (cap) ? (cap) * 2 : 8;                               \
            (arr) = ot_realloc((arr), (cap) * sizeof *(arr));            \
        }                                                                \
        (arr)[(len)++] = (item);                                         \
    } while (0)

#define OT_INT_MIN (-2147483647LL - 1)
#define OT_INT_MAX 2147483647LL

/* Teto de rodadas: cada rodada só encolhe ou simplifica a IR, então o ponto
 * fixo chega bem antes disso; o limite só garante que o laço termina. */
#define OT_MAX_RODADAS 50

typedef struct {
    IrPrograma *p;
    const char **globais;
    size_t nglobais;
} Otimizador;

static int ot_eh(const char *s, const char *texto) { return s && strcmp(s, texto) == 0; }

static int ot_global(const Otimizador *o, const char *nome) {
    for (size_t i = 0; i < o->nglobais; i++) {
        if (o->globais[i] == nome) {
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ *
 * Conjuntos de nomes
 * ------------------------------------------------------------------ */

typedef struct {
    const char **itens;
    size_t len, cap;
} OtConjunto;

static int ot_tem(const OtConjunto *s, const char *nome) {
    for (size_t i = 0; i < s->len; i++) {
        if (s->itens[i] == nome) {
            return 1;
        }
    }
    return 0;
}

static void ot_por(OtConjunto *s, const char *nome) {
    if (!ot_tem(s, nome)) {
        OT_PUSH(s->itens, s->len, s->cap, nome);
    }
}

static void ot_tirar(OtConjunto *s, const char *nome) {
    for (size_t i = 0; i < s->len; i++) {
        if (s->itens[i] == nome) {
            s->itens[i] = s->itens[--s->len];
            return;
        }
    }
}

static int ot_conjuntos_iguais(const OtConjunto *a, const OtConjunto *b) {
    if (a->len != b->len) {
        return 0;
    }
    for (size_t i = 0; i < a->len; i++) {
        if (!ot_tem(b, a->itens[i])) {
            return 0;
        }
    }
    return 1;
}

static void ot_copiar_conjunto(OtConjunto *destino, const OtConjunto *origem) {
    destino->len = 0;
    for (size_t i = 0; i < origem->len; i++) {
        OT_PUSH(destino->itens, destino->len, destino->cap, origem->itens[i]);
    }
}

/* ------------------------------------------------------------------ *
 * Operandos e instruções
 * ------------------------------------------------------------------ */

/* -?[0-9]+ */
static int ot_forma_inteira(const char *s) {
    if (!s) {
        return 0;
    }
    if (*s == '-') {
        s++;
    }
    if (!*s) {
        return 0;
    }
    for (; *s; s++) {
        if (!isdigit((unsigned char)*s)) {
            return 0;
        }
    }
    return 1;
}

/* Valor de um literal inteiro que cabe em 32 bits. */
static int ot_inteiro(const char *operando, long long *valor) {
    if (!operando || strlen(operando) > 11 || !ot_forma_inteira(operando)) {
        return 0;
    }
    long long v = strtoll(operando, NULL, 10);
    if (v < OT_INT_MIN || v > OT_INT_MAX) {
        return 0;
    }
    *valor = v;
    return 1;
}

static long long ot_int32(long long valor) {
    long long m = valor % 4294967296LL;
    if (m < 0) {
        m += 4294967296LL;
    }
    return m >= 2147483648LL ? m - 4294967296LL : m;
}

/* Código de um literal de caractere ASCII ('A', '\n'). */
static int ot_codigo_char(const char *operando, long long *codigo) {
    size_t n = operando ? strlen(operando) : 0;
    if (n < 3 || operando[0] != '\'' || operando[n - 1] != '\'') {
        return 0;
    }
    const char *corpo = operando + 1;
    size_t m = n - 2;
    if (m == 1 && corpo[0] != '\\' && (unsigned char)corpo[0] < 128) {
        *codigo = (unsigned char)corpo[0];
        return 1;
    }
    if (m == 2 && corpo[0] == '\\') {
        switch (corpo[1]) {
        case 'n': *codigo = 10; return 1;
        case 't': *codigo = 9; return 1;
        case 'r': *codigo = 13; return 1;
        case '\\': *codigo = 92; return 1;
        case '\'': *codigo = 39; return 1;
        case '"': *codigo = 34; return 1;
        default: return 0;
        }
    }
    return 0;
}

static const char *ot_numero(Otimizador *o, long long valor) {
    char buf[32];
    snprintf(buf, sizeof buf, "%ld", (long)valor); /* cabe em 32 bits */
    return ir_intern(o->p, buf);
}

static const char *ot_logico(Otimizador *o, int valor) {
    return ir_intern(o->p, valor ? "true" : "false");
}

/* Nome definido (escrito) pela instrução, ou NULL. */
static const char *ot_definido(const IrInstr *ins) {
    switch (ins->kind) {
    case IR_COPY:
    case IR_BIN:
    case IR_UN:
    case IR_LOAD:
    case IR_READ:
    case IR_CALL:
        return ins->dst;
    default:
        return NULL;
    }
}

/* Campos da instrução que são operandos lidos (sem nomes de vetor/função). */
static size_t ot_campos_de_uso(IrInstr *ins, const char ***campos) {
    switch (ins->kind) {
    case IR_COPY:
    case IR_UN:
    case IR_PRINT:
    case IR_PARAM:
    case IR_RET:
    case IR_IFF:
    case IR_IFT:
        campos[0] = &ins->a;
        return 1;
    case IR_BIN:
        campos[0] = &ins->a;
        campos[1] = &ins->b;
        return 2;
    case IR_LOAD:
        campos[0] = &ins->b;
        return 1;
    case IR_STORE:
        campos[0] = &ins->b;
        campos[1] = &ins->c;
        return 2;
    default:
        return 0;
    }
}

static void ot_acrescentar_usos(const Otimizador *o, IrInstr *ins, OtConjunto *usos) {
    const char **campos[2];
    size_t n = ot_campos_de_uso(ins, campos);
    for (size_t i = 0; i < n; i++) {
        const char *valor = *campos[i];
        if (valor && !ir_eh_literal(valor)) {
            ot_por(usos, valor);
        }
    }
    if (ins->kind == IR_CALL || ins->kind == IR_RET) {
        for (size_t i = 0; i < o->nglobais; i++) {
            ot_por(usos, o->globais[i]);
        }
    }
}

/* ------------------------------------------------------------------ *
 * Blocos básicos
 * ------------------------------------------------------------------ */

typedef struct {
    size_t inicio, fim;
    size_t succ[2];
    size_t nsucc;
    size_t *pred;
    size_t npred, cappred;
} OtBloco;

typedef struct {
    OtBloco *itens;
    size_t len;
} OtBlocos;

static int ot_desvio(IrKind kind) {
    return kind == IR_GOTO || kind == IR_IFF || kind == IR_IFT || kind == IR_RET;
}

static size_t ot_bloco_do_rotulo(const OtBlocos *bs, const IrFuncao *f, const char *rotulo) {
    for (size_t b = 0; b < bs->len; b++) {
        const IrInstr *primeira = &f->instrs[bs->itens[b].inicio];
        if (primeira->kind == IR_LABEL && primeira->label == rotulo) {
            return b;
        }
    }
    fprintf(stderr, "erro interno: rótulo %s sem bloco\n", rotulo);
    exit(5);
}

static void ot_sucessor(OtBloco *bloco, size_t s) {
    for (size_t i = 0; i < bloco->nsucc; i++) {
        if (bloco->succ[i] == s) {
            return;
        }
    }
    bloco->succ[bloco->nsucc++] = s;
}

static OtBlocos ot_blocos(const IrFuncao *f) {
    OtBlocos bs = {NULL, 0};
    if (f->len == 0) {
        return bs;
    }
    char *lider = calloc(f->len, 1);
    if (!lider) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    lider[0] = 1;
    for (size_t i = 0; i < f->len; i++) {
        if (f->instrs[i].kind == IR_LABEL) {
            lider[i] = 1;
        }
        if (ot_desvio(f->instrs[i].kind) && i + 1 < f->len) {
            lider[i + 1] = 1;
        }
    }
    size_t n = 0;
    for (size_t i = 0; i < f->len; i++) {
        n += lider[i] != 0;
    }
    bs.itens = calloc(n, sizeof *bs.itens);
    if (!bs.itens) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    for (size_t i = 0; i < f->len; i++) {
        if (lider[i]) {
            if (bs.len > 0) {
                bs.itens[bs.len - 1].fim = i;
            }
            bs.itens[bs.len++].inicio = i;
        }
    }
    bs.itens[bs.len - 1].fim = f->len;
    free(lider);

    for (size_t b = 0; b < bs.len; b++) {
        OtBloco *bloco = &bs.itens[b];
        const IrInstr *ultima = &f->instrs[bloco->fim - 1];
        int tem_seguinte = b + 1 < bs.len;
        if (ultima->kind == IR_GOTO) {
            ot_sucessor(bloco, ot_bloco_do_rotulo(&bs, f, ultima->label));
        } else if (ultima->kind == IR_IFF || ultima->kind == IR_IFT) {
            ot_sucessor(bloco, ot_bloco_do_rotulo(&bs, f, ultima->label));
            if (tem_seguinte) {
                ot_sucessor(bloco, b + 1);
            }
        } else if (ultima->kind != IR_RET && tem_seguinte) {
            ot_sucessor(bloco, b + 1);
        }
    }
    for (size_t b = 0; b < bs.len; b++) {
        for (size_t i = 0; i < bs.itens[b].nsucc; i++) {
            OtBloco *s = &bs.itens[bs.itens[b].succ[i]];
            OT_PUSH(s->pred, s->npred, s->cappred, b);
        }
    }
    return bs;
}

static void ot_blocos_free(OtBlocos *bs) {
    for (size_t b = 0; b < bs->len; b++) {
        free(bs->itens[b].pred);
    }
    free(bs->itens);
}

/* ------------------------------------------------------------------ *
 * 1. Simplificação local
 * ------------------------------------------------------------------ */

static const char *ot_dobrar_binario(Otimizador *o, const char *op, const char *a, const char *b) {
    long long x, y;
    if (ot_inteiro(a, &x) && ot_inteiro(b, &y)) {
        if (ot_eh(op, "+")) {
            return ot_numero(o, ot_int32(x + y));
        }
        if (ot_eh(op, "-")) {
            return ot_numero(o, ot_int32(x - y));
        }
        if (ot_eh(op, "*")) {
            return ot_numero(o, ot_int32(x * y));
        }
        if (ot_eh(op, "/") || ot_eh(op, "%")) {
            if (y == 0 || (x == OT_INT_MIN && y == -1)) {
                return NULL;
            }
            long long q = llabs(x) / llabs(y);
            if ((x < 0) != (y < 0)) {
                q = -q;
            }
            return ot_numero(o, ot_eh(op, "/") ? q : x - q * y);
        }
        if (ot_eh(op, "<")) return ot_logico(o, x < y);
        if (ot_eh(op, "<=")) return ot_logico(o, x <= y);
        if (ot_eh(op, ">")) return ot_logico(o, x > y);
        if (ot_eh(op, ">=")) return ot_logico(o, x >= y);
        if (ot_eh(op, "==")) return ot_logico(o, x == y);
        if (ot_eh(op, "!=")) return ot_logico(o, x != y);
        return NULL;
    }
    int a_logico = ot_eh(a, "true") || ot_eh(a, "false");
    int b_logico = ot_eh(b, "true") || ot_eh(b, "false");
    if (a_logico && b_logico) {
        int p = ot_eh(a, "true"), q = ot_eh(b, "true");
        if (ot_eh(op, "and")) return ot_logico(o, p && q);
        if (ot_eh(op, "or")) return ot_logico(o, p || q);
        if (ot_eh(op, "==")) return ot_logico(o, p == q);
        if (ot_eh(op, "!=")) return ot_logico(o, p != q);
    }
    return NULL;
}

/* Operando equivalente a `a op b` por identidade algébrica, ou NULL. */
static const char *ot_identidade(Otimizador *o, const char *op, const char *a, const char *b) {
    if (ot_eh(op, "+")) {
        if (ot_eh(b, "0")) return a;
        if (ot_eh(a, "0")) return b;
    } else if (ot_eh(op, "-")) {
        if (ot_eh(b, "0")) return a;
    } else if (ot_eh(op, "*")) {
        if (ot_eh(b, "1")) return a;
        if (ot_eh(a, "1")) return b;
        if (ot_eh(a, "0") || ot_eh(b, "0")) return ir_intern(o->p, "0");
    } else if (ot_eh(op, "/")) {
        if (ot_eh(b, "1")) return a;
    } else if (ot_eh(op, "and")) {
        if (ot_eh(b, "true")) return a;
        if (ot_eh(a, "true")) return b;
        if (ot_eh(a, "false") || ot_eh(b, "false")) return ir_intern(o->p, "false");
    } else if (ot_eh(op, "or")) {
        if (ot_eh(b, "false")) return a;
        if (ot_eh(a, "false")) return b;
        if (ot_eh(a, "true") || ot_eh(b, "true")) return ir_intern(o->p, "true");
    }
    return NULL;
}

static const char *ot_dobrar_unario(Otimizador *o, const char *op, const char *a) {
    long long x;
    if (ot_eh(op, "neg")) {
        return ot_inteiro(a, &x) ? ot_numero(o, ot_int32(-x)) : NULL;
    }
    if (ot_eh(op, "not")) {
        if (ot_eh(a, "true")) return ir_intern(o->p, "false");
        if (ot_eh(a, "false")) return ir_intern(o->p, "true");
        return NULL;
    }
    if (ot_eh(op, "itof")) {
        if (!ot_forma_inteira(a)) {
            return NULL;
        }
        size_t n = strlen(a);
        char *texto = ot_realloc(NULL, n + 3);
        memcpy(texto, a, n);
        memcpy(texto + n, ".0", 3);
        const char *unico = ir_intern(o->p, texto);
        free(texto);
        return unico;
    }
    if (ot_eh(op, "ctoi")) {
        return ot_codigo_char(a, &x) ? ot_numero(o, x) : NULL;
    }
    return NULL;
}

static IrInstr ot_copia(const char *dst, const char *a) {
    IrInstr ins = {IR_COPY, dst, a, NULL, NULL, NULL, NULL, 0};
    return ins;
}

static int ot_simplificar(Otimizador *o, IrFuncao *f) {
    int mudou = 0;
    size_t j = 0;
    for (size_t i = 0; i < f->len; i++) {
        IrInstr ins = f->instrs[i];
        if (ins.kind == IR_BIN) {
            const char *valor = ot_dobrar_binario(o, ins.op, ins.a, ins.b);
            if (!valor) {
                valor = ot_identidade(o, ins.op, ins.a, ins.b);
            }
            if (valor) {
                ins = ot_copia(ins.dst, valor);
                mudou = 1;
            }
        } else if (ins.kind == IR_UN) {
            const char *valor = ot_dobrar_unario(o, ins.op, ins.a);
            if (valor) {
                ins = ot_copia(ins.dst, valor);
                mudou = 1;
            }
        } else if ((ins.kind == IR_IFF || ins.kind == IR_IFT) &&
                   (ot_eh(ins.a, "true") || ot_eh(ins.a, "false"))) {
            mudou = 1;
            int salta = ot_eh(ins.a, "false") == (ins.kind == IR_IFF);
            if (!salta) {
                continue;
            }
            IrInstr desvio = {IR_GOTO, NULL, NULL, NULL, NULL, NULL, ins.label, 0};
            ins = desvio;
        }
        if (ins.kind == IR_COPY && ins.dst == ins.a) {
            mudou = 1;
            continue;
        }
        f->instrs[j++] = ins;
    }
    f->len = j;
    return mudou;
}

/* ------------------------------------------------------------------ *
 * 2. Propagação de constantes e cópias
 * ------------------------------------------------------------------ */

typedef struct {
    const char *x; /* a variável ... */
    const char *c; /* ... vale este operando */
} OtFato;

typedef struct {
    OtFato *itens;
    size_t len, cap;
    int topo; /* ainda sem informação (o None do Python) */
} OtFatos;

static const char *ot_valor_de(const OtFatos *fatos, const char *x) {
    for (size_t i = 0; i < fatos->len; i++) {
        if (fatos->itens[i].x == x) {
            return fatos->itens[i].c;
        }
    }
    return NULL;
}

static void ot_copiar_fatos(OtFatos *destino, const OtFatos *origem) {
    destino->len = 0;
    destino->topo = origem->topo;
    for (size_t i = 0; i < origem->len; i++) {
        OT_PUSH(destino->itens, destino->len, destino->cap, origem->itens[i]);
    }
}

static int ot_fatos_iguais(const OtFatos *a, const OtFatos *b) {
    if (a->topo || b->topo) {
        return a->topo == b->topo;
    }
    if (a->len != b->len) {
        return 0;
    }
    for (size_t i = 0; i < a->len; i++) {
        if (ot_valor_de(b, a->itens[i].x) != a->itens[i].c) {
            return 0;
        }
    }
    return 1;
}

/* `nome` mudou: some o fato sobre ele e todo fato que o usava como valor. */
static void ot_matar(OtFatos *fatos, const char *nome) {
    size_t j = 0;
    for (size_t i = 0; i < fatos->len; i++) {
        if (fatos->itens[i].x != nome && fatos->itens[i].c != nome) {
            fatos->itens[j++] = fatos->itens[i];
        }
    }
    fatos->len = j;
}

static void ot_transferir(const Otimizador *o, const IrInstr *ins, OtFatos *fatos) {
    if (ins->kind == IR_CALL) {
        size_t j = 0;
        for (size_t i = 0; i < fatos->len; i++) {
            if (!ot_global(o, fatos->itens[i].x) && !ot_global(o, fatos->itens[i].c)) {
                fatos->itens[j++] = fatos->itens[i];
            }
        }
        fatos->len = j;
    }
    const char *destino = ot_definido(ins);
    if (!destino) {
        return;
    }
    const char *valor = NULL;
    if (ins->kind == IR_COPY) {
        valor = ot_valor_de(fatos, ins->a);
        if (!valor) {
            valor = ins->a;
        }
    }
    ot_matar(fatos, destino);
    if (valor && valor != destino) {
        OtFato fato = {destino, valor};
        OT_PUSH(fatos->itens, fatos->len, fatos->cap, fato);
    }
}

static int ot_propagar(Otimizador *o, IrFuncao *f) {
    OtBlocos bs = ot_blocos(f);
    OtFatos *entrada = calloc(bs.len ? bs.len : 1, sizeof *entrada);
    OtFatos *saida = calloc(bs.len ? bs.len : 1, sizeof *saida);
    if (!entrada || !saida) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    for (size_t b = 0; b < bs.len; b++) {
        entrada[b].topo = saida[b].topo = 1;
    }
    OtFatos fatos = {NULL, 0, 0, 0};

    int mudou = 1;
    while (mudou) {
        mudou = 0;
        for (size_t b = 0; b < bs.len; b++) {
            const OtBloco *bloco = &bs.itens[b];
            fatos.len = 0;
            fatos.topo = 0;
            if (b != 0) {
                /* Interseção: só os fatos que valem em todos os caminhos. */
                int algum = 0;
                for (size_t k = 0; k < bloco->npred; k++) {
                    const OtFatos *outro = &saida[bloco->pred[k]];
                    if (outro->topo) {
                        continue;
                    }
                    if (!algum) {
                        ot_copiar_fatos(&fatos, outro);
                        algum = 1;
                        continue;
                    }
                    size_t j = 0;
                    for (size_t i = 0; i < fatos.len; i++) {
                        if (ot_valor_de(outro, fatos.itens[i].x) == fatos.itens[i].c) {
                            fatos.itens[j++] = fatos.itens[i];
                        }
                    }
                    fatos.len = j;
                }
                fatos.topo = !algum;
            }
            ot_copiar_fatos(&entrada[b], &fatos);
            if (!fatos.topo) {
                for (size_t i = bloco->inicio; i < bloco->fim; i++) {
                    ot_transferir(o, &f->instrs[i], &fatos);
                }
            }
            if (!ot_fatos_iguais(&fatos, &saida[b])) {
                ot_copiar_fatos(&saida[b], &fatos);
                mudou = 1;
            }
        }
    }

    int reescreveu = 0;
    for (size_t b = 0; b < bs.len; b++) {
        ot_copiar_fatos(&fatos, &entrada[b]);
        if (fatos.topo) {
            fatos.len = 0;
            fatos.topo = 0;
        }
        for (size_t i = bs.itens[b].inicio; i < bs.itens[b].fim; i++) {
            IrInstr *ins = &f->instrs[i];
            const char **campos[2];
            size_t n = ot_campos_de_uso(ins, campos);
            for (size_t k = 0; k < n; k++) {
                const char *valor = *campos[k] ? ot_valor_de(&fatos, *campos[k]) : NULL;
                if (valor) {
                    *campos[k] = valor;
                    reescreveu = 1;
                }
            }
            ot_transferir(o, ins, &fatos);
        }
    }

    for (size_t b = 0; b < bs.len; b++) {
        free(entrada[b].itens);
        free(saida[b].itens);
    }
    free(entrada);
    free(saida);
    free(fatos.itens);
    ot_blocos_free(&bs);
    return reescreveu;
}

/* ------------------------------------------------------------------ *
 * 3. Subexpressões comuns (local)
 * ------------------------------------------------------------------ */

typedef struct {
    IrKind kind;
    const char *op, *a, *b;
    const char *guardiao; /* quem guarda o valor */
} OtDisponivel;

static int ot_disponivel_usa(const OtDisponivel *d, const char *nome) {
    return d->a == nome || d->b == nome || d->guardiao == nome;
}

static int ot_cse(Otimizador *o, IrFuncao *f) {
    OtBlocos bs = ot_blocos(f);
    OtDisponivel *disp = NULL;
    size_t ndisp = 0, capdisp = 0;
    int mudou = 0;
    for (size_t b = 0; b < bs.len; b++) {
        ndisp = 0;
        for (size_t i = bs.itens[b].inicio; i < bs.itens[b].fim; i++) {
            IrInstr *ins = &f->instrs[i];
            if (ins->kind == IR_BIN || ins->kind == IR_UN) {
                for (size_t k = 0; k < ndisp; k++) {
                    const OtDisponivel *d = &disp[k];
                    if (d->kind == ins->kind && d->op == ins->op && d->a == ins->a &&
                        d->b == ins->b) {
                        *ins = ot_copia(ins->dst, d->guardiao);
                        mudou = 1;
                        break;
                    }
                }
            }
            if (ins->kind == IR_CALL) {
                size_t j = 0;
                for (size_t k = 0; k < ndisp; k++) {
                    const OtDisponivel *d = &disp[k];
                    int toca = (d->a && ot_global(o, d->a)) || (d->b && ot_global(o, d->b)) ||
                               ot_global(o, d->guardiao);
                    if (!toca) {
                        disp[j++] = disp[k];
                    }
                }
                ndisp = j;
            }
            const char *destino = ot_definido(ins);
            if (destino) {
                size_t j = 0;
                for (size_t k = 0; k < ndisp; k++) {
                    if (!ot_disponivel_usa(&disp[k], destino)) {
                        disp[j++] = disp[k];
                    }
                }
                ndisp = j;
            }
            if ((ins->kind == IR_BIN || ins->kind == IR_UN) && ins->dst != ins->a &&
                ins->dst != ins->b) {
                OtDisponivel novo = {ins->kind, ins->op, ins->a, ins->b, ins->dst};
                OT_PUSH(disp, ndisp, capdisp, novo);
            }
        }
    }
    free(disp);
    ot_blocos_free(&bs);
    return mudou;
}

/* ------------------------------------------------------------------ *
 * 4. Limpeza do fluxo de controle
 * ------------------------------------------------------------------ */

static int ot_limpar(IrFuncao *f) {
    int mudou_alguma = 0;
    for (;;) {
        int mudou = 0;

        /* Blocos inalcançáveis a partir da entrada. */
        OtBlocos bs = ot_blocos(f);
        if (bs.len > 0) {
            char *alcancado = calloc(bs.len, 1);
            size_t *pilha = ot_realloc(NULL, (bs.len * 2 + 1) * sizeof *pilha);
            size_t topo = 0, total = 0;
            pilha[topo++] = 0;
            while (topo > 0) {
                size_t b = pilha[--topo];
                if (alcancado[b]) {
                    continue;
                }
                alcancado[b] = 1;
                total++;
                for (size_t k = 0; k < bs.itens[b].nsucc; k++) {
                    pilha[topo++] = bs.itens[b].succ[k];
                }
            }
            if (total < bs.len) {
                size_t j = 0;
                for (size_t b = 0; b < bs.len; b++) {
                    if (alcancado[b]) {
                        for (size_t i = bs.itens[b].inicio; i < bs.itens[b].fim; i++) {
                            f->instrs[j++] = f->instrs[i];
                        }
                    }
                }
                f->len = j;
                mudou = 1;
            }
            free(alcancado);
            free(pilha);
        }
        ot_blocos_free(&bs);

        /* Desvio para um rótulo que já vem logo em seguida. */
        size_t j = 0;
        for (size_t i = 0; i < f->len; i++) {
            IrInstr *ins = &f->instrs[i];
            if (ins->kind == IR_GOTO || ins->kind == IR_IFF || ins->kind == IR_IFT) {
                int seguinte = 0;
                for (size_t k = i + 1; k < f->len && f->instrs[k].kind == IR_LABEL; k++) {
                    if (f->instrs[k].label == ins->label) {
                        seguinte = 1;
                    }
                }
                if (seguinte) {
                    mudou = 1;
                    continue;
                }
            }
            f->instrs[j++] = *ins;
        }
        f->len = j;

        /* Rótulos sem nenhum desvio apontando para eles. */
        j = 0;
        for (size_t i = 0; i < f->len; i++) {
            IrInstr *ins = &f->instrs[i];
            if (ins->kind == IR_LABEL) {
                int usado = 0;
                for (size_t k = 0; k < f->len && !usado; k++) {
                    IrKind kk = f->instrs[k].kind;
                    usado = (kk == IR_GOTO || kk == IR_IFF || kk == IR_IFT) &&
                            f->instrs[k].label == ins->label;
                }
                if (!usado) {
                    mudou = 1;
                    continue;
                }
            }
            f->instrs[j++] = *ins;
        }
        f->len = j;

        if (!mudou) {
            return mudou_alguma;
        }
        mudou_alguma = 1;
    }
}

/* ------------------------------------------------------------------ *
 * 5. Código morto (variáveis vivas)
 * ------------------------------------------------------------------ */

static int ot_removivel(const IrInstr *ins) {
    if (ins->kind != IR_COPY && ins->kind != IR_BIN && ins->kind != IR_UN) {
        return 0;
    }
    if (ins->kind == IR_BIN && (ot_eh(ins->op, "/") || ot_eh(ins->op, "%"))) {
        long long divisor;
        return ot_inteiro(ins->b, &divisor) && divisor != 0;
    }
    return 1;
}

static void ot_vivas_na_saida(const Otimizador *o, const OtBlocos *bs, size_t b,
                              const OtConjunto *vivas_entrada, OtConjunto *vivas) {
    vivas->len = 0;
    if (bs->itens[b].nsucc == 0) {
        for (size_t i = 0; i < o->nglobais; i++) {
            ot_por(vivas, o->globais[i]); /* fim da função */
        }
        return;
    }
    for (size_t k = 0; k < bs->itens[b].nsucc; k++) {
        const OtConjunto *s = &vivas_entrada[bs->itens[b].succ[k]];
        for (size_t i = 0; i < s->len; i++) {
            ot_por(vivas, s->itens[i]);
        }
    }
}

static int ot_mortos(Otimizador *o, IrFuncao *f) {
    OtBlocos bs = ot_blocos(f);
    size_t nb = bs.len ? bs.len : 1;
    OtConjunto *vivas_entrada = calloc(nb, sizeof *vivas_entrada);
    OtConjunto *vivas_saida = calloc(nb, sizeof *vivas_saida);
    char *manter = calloc(f->len ? f->len : 1, 1);
    if (!vivas_entrada || !vivas_saida || !manter) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    OtConjunto vivas = {NULL, 0, 0};

    int mudou = 1;
    while (mudou) {
        mudou = 0;
        for (size_t b = bs.len; b-- > 0;) {
            ot_vivas_na_saida(o, &bs, b, vivas_entrada, &vivas);
            ot_copiar_conjunto(&vivas_saida[b], &vivas);
            for (size_t i = bs.itens[b].fim; i-- > bs.itens[b].inicio;) {
                IrInstr *ins = &f->instrs[i];
                const char *destino = ot_definido(ins);
                if (destino) {
                    ot_tirar(&vivas, destino);
                }
                ot_acrescentar_usos(o, ins, &vivas);
            }
            if (!ot_conjuntos_iguais(&vivas, &vivas_entrada[b])) {
                ot_copiar_conjunto(&vivas_entrada[b], &vivas);
                mudou = 1;
            }
        }
    }

    int removeu = 0;
    memset(manter, 1, f->len);
    for (size_t b = 0; b < bs.len; b++) {
        ot_copiar_conjunto(&vivas, &vivas_saida[b]);
        for (size_t i = bs.itens[b].fim; i-- > bs.itens[b].inicio;) {
            IrInstr *ins = &f->instrs[i];
            const char *destino = ot_definido(ins);
            int morto = destino && !ot_tem(&vivas, destino) && !ot_global(o, destino);
            if (morto && ot_removivel(ins)) {
                manter[i] = 0;
                removeu = 1;
                continue;
            }
            if (morto && ins->kind == IR_CALL) {
                ins->dst = NULL;
                removeu = 1;
            }
            if (destino) {
                ot_tirar(&vivas, destino);
            }
            ot_acrescentar_usos(o, ins, &vivas);
        }
    }
    size_t j = 0;
    for (size_t i = 0; i < f->len; i++) {
        if (manter[i]) {
            f->instrs[j++] = f->instrs[i];
        }
    }
    f->len = j;

    for (size_t b = 0; b < bs.len; b++) {
        free(vivas_entrada[b].itens);
        free(vivas_saida[b].itens);
    }
    free(vivas_entrada);
    free(vivas_saida);
    free(vivas.itens);
    free(manter);
    ot_blocos_free(&bs);
    return removeu;
}

/* ------------------------------------------------------------------ *
 * 6. Laços: código invariante e redução de força (aula 19)
 * ------------------------------------------------------------------ */

/* Matriz n x n de bytes: dom[b*n + d] != 0 se d domina b. */
typedef struct {
    char *dom;
    char *alcancavel;
    size_t n;
} OtDominadores;

static OtDominadores ot_dominadores(const OtBlocos *bs) {
    size_t n = bs->len;
    OtDominadores d = {calloc(n * n + 1, 1), calloc(n + 1, 1), n};
    if (!d.dom || !d.alcancavel) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    if (n == 0) {
        return d;
    }
    size_t *pilha = ot_realloc(NULL, (n * 2 + 1) * sizeof *pilha);
    size_t topo = 0;
    d.alcancavel[0] = 1;
    pilha[topo++] = 0;
    while (topo > 0) {
        size_t b = pilha[--topo];
        for (size_t k = 0; k < bs->itens[b].nsucc; k++) {
            size_t s = bs->itens[b].succ[k];
            if (!d.alcancavel[s]) {
                d.alcancavel[s] = 1;
                pilha[topo++] = s;
            }
        }
    }
    free(pilha);
    for (size_t b = 0; b < n; b++) {
        if (!d.alcancavel[b]) {
            continue;
        }
        for (size_t x = 0; x < n; x++) {
            d.dom[b * n + x] = b == 0 ? x == 0 : d.alcancavel[x];
        }
    }
    char *novo = ot_realloc(NULL, n);
    int mudou = 1;
    while (mudou) {
        mudou = 0;
        for (size_t b = 1; b < n; b++) {
            if (!d.alcancavel[b]) {
                continue;
            }
            int algum = 0;
            memset(novo, 0, n);
            for (size_t k = 0; k < bs->itens[b].npred; k++) {
                size_t p = bs->itens[b].pred[k];
                if (!d.alcancavel[p]) {
                    continue;
                }
                for (size_t x = 0; x < n; x++) {
                    novo[x] = algum ? (novo[x] && d.dom[p * n + x]) : d.dom[p * n + x];
                }
                algum = 1;
            }
            novo[b] = 1;
            if (memcmp(novo, &d.dom[b * n], n) != 0) {
                memcpy(&d.dom[b * n], novo, n);
                mudou = 1;
            }
        }
    }
    free(novo);
    return d;
}

/* Laços naturais: corpo[h*n + x] != 0 se o bloco x está no laço de cabeçalho h. */
static char *ot_lacos_naturais(const OtBlocos *bs, const OtDominadores *d, char *eh_cabecalho) {
    size_t n = bs->len;
    char *corpo = calloc(n * n + 1, 1);
    size_t *pilha = ot_realloc(NULL, (n * n + n + 1) * sizeof *pilha);
    if (!corpo) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    for (size_t b = 0; b < n; b++) {
        if (!d->alcancavel[b]) {
            continue;
        }
        for (size_t k = 0; k < bs->itens[b].nsucc; k++) {
            size_t h = bs->itens[b].succ[k];
            if (!d->dom[b * n + h]) {
                continue;
            }
            /* aresta de retorno b -> h */
            eh_cabecalho[h] = 1;
            corpo[h * n + h] = 1;
            size_t topo = 0;
            pilha[topo++] = b;
            while (topo > 0) {
                size_t x = pilha[--topo];
                if (corpo[h * n + x]) {
                    continue;
                }
                corpo[h * n + x] = 1;
                for (size_t m = 0; m < bs->itens[x].npred; m++) {
                    pilha[topo++] = bs->itens[x].pred[m];
                }
            }
        }
    }
    free(pilha);
    return corpo;
}

/* Divisor que nunca causa erro: literal numérico diferente de zero. */
static int ot_divisor_seguro(const char *operando) {
    if (!operando || !ir_eh_literal(operando) || operando[0] == '\'' || operando[0] == '"') {
        return 0;
    }
    if (ot_eh(operando, "true") || ot_eh(operando, "false")) {
        return 0;
    }
    return strtod(operando, NULL) != 0.0;
}

typedef struct {
    const char *nome;
    int n;
} OtContagem;

typedef struct {
    OtContagem *itens;
    size_t len, cap;
} OtContagens;

static int ot_contagem(const OtContagens *c, const char *nome) {
    for (size_t i = 0; i < c->len; i++) {
        if (c->itens[i].nome == nome) {
            return c->itens[i].n;
        }
    }
    return 0;
}

static void ot_contar(OtContagens *c, const char *nome) {
    for (size_t i = 0; i < c->len; i++) {
        if (c->itens[i].nome == nome) {
            c->itens[i].n++;
            return;
        }
    }
    OtContagem nova = {nome, 1};
    OT_PUSH(c->itens, c->len, c->cap, nova);
}

static const char *ot_novo_temporario(Otimizador *o, const IrFuncao *f) {
    long maior = 0;
    for (size_t i = 0; i < f->len; i++) {
        const IrInstr *ins = &f->instrs[i];
        const char *campos[4] = {ins->dst, ins->a, ins->b, ins->c};
        size_t ncampos = ins->kind == IR_CALL ? 1 : 4;
        for (size_t k = 0; k < ncampos; k++) {
            if (campos[k] && ir_eh_temporario(campos[k])) {
                long v = strtol(campos[k] + 1, NULL, 10);
                if (v > maior) {
                    maior = v;
                }
            }
        }
    }
    char buf[32];
    snprintf(buf, sizeof buf, "t%ld", maior + 1);
    return ir_intern(o->p, buf);
}

/* Insere `ins` na posição `pos` de f->instrs. */
static void ot_inserir(IrFuncao *f, size_t pos, IrInstr ins) {
    OT_PUSH(f->instrs, f->len, f->cap, ins); /* garante espaço */
    memmove(&f->instrs[pos + 1], &f->instrs[pos], (f->len - 1 - pos) * sizeof *f->instrs);
    f->instrs[pos] = ins;
}

/* Passo `c` se `d` é `i = i ± c` (direto ou via `u = i ± c; i = u`). */
static int ot_passo_de_inducao(const IrFuncao *f, const size_t *indices, size_t nind,
                               const IrInstr *d, const char *i_var, const OtContagens *na_funcao,
                               long long *passo) {
    const IrInstr *conta = NULL;
    if (d->kind == IR_BIN) {
        conta = d;
    } else if (d->kind == IR_COPY && ir_eh_temporario(d->a) && ot_contagem(na_funcao, d->a) == 1) {
        for (size_t j = 0; j < nind; j++) {
            if (ot_definido(&f->instrs[indices[j]]) == d->a) {
                conta = &f->instrs[indices[j]];
                break;
            }
        }
        if (!conta || conta->kind != IR_BIN) {
            return 0;
        }
    } else {
        return 0;
    }
    long long c;
    if (ot_eh(conta->op, "+")) {
        if (conta->a == i_var && ot_inteiro(conta->b, &c)) {
            *passo = c;
            return 1;
        }
        if (conta->b == i_var && ot_inteiro(conta->a, &c)) {
            *passo = c;
            return 1;
        }
    }
    if (ot_eh(conta->op, "-") && conta->a == i_var && ot_inteiro(conta->b, &c)) {
        *passo = -c;
        return 1;
    }
    return 0;
}

static int ot_transformar_laco(Otimizador *o, IrFuncao *f, const OtBlocos *bs, size_t h,
                               const char *corpo) {
    size_t n = bs->len;
    size_t inicio_h = bs->itens[h].inicio;
    const char *rotulo_h = f->instrs[inicio_h].kind == IR_LABEL ? f->instrs[inicio_h].label : NULL;

    /* Pré-cabeçalho: a única entrada de fora tem de ser a queda do bloco anterior. */
    size_t nexternos = 0, externo = 0;
    for (size_t k = 0; k < bs->itens[h].npred; k++) {
        size_t p = bs->itens[h].pred[k];
        if (!corpo[h * n + p]) {
            nexternos++;
            externo = p;
        }
    }
    if (h == 0) {
        if (nexternos > 0) {
            return 0;
        }
    } else {
        if (nexternos != 1 || externo != h - 1) {
            return 0;
        }
        const IrInstr *anterior = &f->instrs[bs->itens[h - 1].fim - 1];
        if ((anterior->kind == IR_GOTO || anterior->kind == IR_IFF || anterior->kind == IR_IFT) &&
            anterior->label == rotulo_h) {
            return 0;
        }
    }

    size_t *indices = ot_realloc(NULL, (f->len + 1) * sizeof *indices);
    size_t nind = 0;
    for (size_t i = 0; i < f->len; i++) {
        for (size_t b = 0; b < n; b++) {
            if (corpo[h * n + b] && i >= bs->itens[b].inicio && i < bs->itens[b].fim) {
                indices[nind++] = i;
                break;
            }
        }
    }
    OtContagens no_laco = {NULL, 0, 0}, na_funcao = {NULL, 0, 0};
    int tem_chamada = 0;
    for (size_t j = 0; j < nind; j++) {
        const char *d = ot_definido(&f->instrs[indices[j]]);
        if (d) {
            ot_contar(&no_laco, d);
        }
        tem_chamada |= f->instrs[indices[j]].kind == IR_CALL;
    }
    for (size_t i = 0; i < f->len; i++) {
        const char *d = ot_definido(&f->instrs[i]);
        if (d) {
            ot_contar(&na_funcao, d);
        }
    }

    int resultado = 0;

    /* (a) Código invariante. */
    OtConjunto invariantes = {NULL, 0, 0};
    char *mover = calloc(f->len + 1, 1);
    size_t nmover = 0;
    for (size_t j = 0; j < nind; j++) {
        const IrInstr *ins = &f->instrs[indices[j]];
        if (ins->kind != IR_BIN && ins->kind != IR_UN) {
            continue;
        }
        if (!ir_eh_temporario(ins->dst) || ot_contagem(&na_funcao, ins->dst) != 1) {
            continue;
        }
        if (ins->kind == IR_BIN && (ot_eh(ins->op, "/") || ot_eh(ins->op, "%")) &&
            !ot_divisor_seguro(ins->b)) {
            continue;
        }
        const char *operandos[2] = {ins->a, ins->kind == IR_BIN ? ins->b : NULL};
        int ok = 1;
        for (size_t k = 0; k < 2 && ok; k++) {
            const char *x = operandos[k];
            if (!x || ir_eh_literal(x) || ot_tem(&invariantes, x)) {
                continue;
            }
            ok = ot_contagem(&no_laco, x) == 0 && !(tem_chamada && ot_global(o, x));
        }
        if (ok) {
            mover[indices[j]] = 1;
            nmover++;
            ot_por(&invariantes, ins->dst);
        }
    }
    if (nmover > 0) {
        IrInstr *novas = ot_realloc(NULL, (f->len + 1) * sizeof *novas);
        size_t antes = 0;
        for (size_t i = 0; i < inicio_h; i++) {
            antes += mover[i] != 0;
        }
        size_t posicao = inicio_h - antes;
        /* resto[:posicao] + movidas + resto[posicao:] */
        size_t j = 0, r = 0;
        for (size_t i = 0; i < f->len && r < posicao; i++) {
            if (!mover[i]) {
                novas[j++] = f->instrs[i];
                r++;
            }
        }
        for (size_t i = 0; i < f->len; i++) {
            if (mover[i]) {
                novas[j++] = f->instrs[i];
            }
        }
        r = 0;
        for (size_t i = 0; i < f->len; i++) {
            if (!mover[i]) {
                if (r >= posicao) {
                    novas[j++] = f->instrs[i];
                }
                r++;
            }
        }
        memcpy(f->instrs, novas, j * sizeof *novas);
        f->len = j;
        free(novas);
        resultado = 1;
    }
    free(mover);
    free(invariantes.itens);

    /* (b) Redução de força. */
    for (size_t jd = 0; jd < nind && !resultado; jd++) {
        size_t di = indices[jd];
        const IrInstr *d = &f->instrs[di];
        const char *i_var = ot_definido(d);
        if (!i_var || ir_eh_temporario(i_var) || ot_global(o, i_var) ||
            ot_contagem(&no_laco, i_var) != 1) {
            continue;
        }
        long long passo;
        if (!ot_passo_de_inducao(f, indices, nind, d, i_var, &na_funcao, &passo)) {
            continue;
        }
        for (size_t jm = 0; jm < nind; jm++) {
            size_t mi = indices[jm];
            const IrInstr *m = &f->instrs[mi];
            if (m->kind != IR_BIN || !ot_eh(m->op, "*") || mi == di) {
                continue;
            }
            if (!ir_eh_temporario(m->dst) || ot_contagem(&na_funcao, m->dst) != 1) {
                continue;
            }
            long long k;
            if (m->a == i_var && ot_inteiro(m->b, &k)) {
            } else if (m->b == i_var && ot_inteiro(m->a, &k)) {
            } else {
                continue;
            }
            const char *s = ot_novo_temporario(o, f);
            f->instrs[mi] = ot_copia(m->dst, s);
            IrInstr atualizacao = {IR_BIN, s, s, ot_numero(o, ot_int32(k * passo)), NULL,
                                   ir_intern(o->p, "+"), NULL, 0};
            IrInstr inicial = {IR_BIN, s, i_var, ot_numero(o, k), NULL, ir_intern(o->p, "*"),
                               NULL, 0};
            ot_inserir(f, di + 1, atualizacao);
            ot_inserir(f, inicio_h + (di < inicio_h ? 1 : 0), inicial);
            resultado = 1;
            break;
        }
    }

    free(indices);
    free(no_laco.itens);
    free(na_funcao.itens);
    return resultado;
}

/* Teto de transformações de laço por chamada (cada uma recalcula os blocos). */
#define OT_MAX_LACOS 100

static int ot_lacos(Otimizador *o, IrFuncao *f) {
    int mudou = 0;
    for (int volta = 0; volta < OT_MAX_LACOS; volta++) {
        OtBlocos bs = ot_blocos(f);
        OtDominadores d = ot_dominadores(&bs);
        char *eh_cabecalho = calloc(bs.len + 1, 1);
        char *corpo = ot_lacos_naturais(&bs, &d, eh_cabecalho);
        int transformou = 0;
        for (size_t h = 0; h < bs.len && !transformou; h++) {
            if (eh_cabecalho[h]) {
                transformou = ot_transformar_laco(o, f, &bs, h, corpo);
            }
        }
        free(corpo);
        free(eh_cabecalho);
        free(d.dom);
        free(d.alcancavel);
        ot_blocos_free(&bs);
        if (!transformou) {
            break;
        }
        mudou = 1;
    }
    return mudou;
}

/* ------------------------------------------------------------------ *
 * Pipeline
 * ------------------------------------------------------------------ */

/* Temporários renumerados em ordem de aparição no texto. */
static void ot_renumerar(Otimizador *o, IrFuncao *f) {
    OtFato *novos = NULL; /* x = temporário antigo, c = nome novo */
    size_t nnovos = 0, capnovos = 0;
    for (size_t i = 0; i < f->len; i++) {
        IrInstr *ins = &f->instrs[i];
        /* Na `call`, o campo `a` é o nome da função, não um operando. */
        const char **campos[4] = {&ins->dst, &ins->a, &ins->b, &ins->c};
        size_t ncampos = ins->kind == IR_CALL ? 1 : 4;
        for (size_t k = 0; k < ncampos; k++) {
            const char *valor = *campos[k];
            if (!valor || !ir_eh_temporario(valor)) {
                continue;
            }
            const char *novo = NULL;
            for (size_t m = 0; m < nnovos; m++) {
                if (novos[m].x == valor) {
                    novo = novos[m].c;
                }
            }
            if (!novo) {
                char buf[32];
                snprintf(buf, sizeof buf, "t%d", (int)nnovos + 1);
                novo = ir_intern(o->p, buf);
                OtFato par = {valor, novo};
                OT_PUSH(novos, nnovos, capnovos, par);
            }
            *campos[k] = novo;
        }
    }
    /* O tipo guardado para o destino de um `read` acompanha o novo nome; o de
     * um temporário que sumiu é descartado (o nome pode ter passado a outro). */
    size_t j = 0;
    for (size_t i = 0; i < f->ntipos; i++) {
        IrTipo par = f->tipos[i];
        if (ir_eh_temporario(par.nome)) {
            const char *novo = NULL;
            for (size_t m = 0; m < nnovos; m++) {
                if (novos[m].x == par.nome) {
                    novo = novos[m].c;
                }
            }
            if (!novo) {
                continue;
            }
            par.nome = novo;
        }
        f->tipos[j++] = par;
    }
    f->ntipos = j;
    free(novos);
}

void otimizador_otimizar(IrPrograma *programa) {
    Otimizador o = {programa, NULL, 0};
    o.globais = ot_realloc(NULL, (programa->nglobais ? programa->nglobais : 1) * sizeof *o.globais);
    for (size_t i = 0; i < programa->nglobais; i++) {
        o.globais[o.nglobais++] = programa->globais[i].nome;
    }
    for (size_t i = 0; i < programa->nfuncoes; i++) {
        IrFuncao *f = &programa->funcoes[i];
        for (int rodada = 0; rodada < OT_MAX_RODADAS; rodada++) {
            int mudou = ot_simplificar(&o, f);
            mudou |= ot_propagar(&o, f);
            mudou |= ot_cse(&o, f);
            mudou |= ot_limpar(f);
            mudou |= ot_mortos(&o, f);
            mudou |= ot_lacos(&o, f);
            if (!mudou) {
                break;
            }
        }
        ot_renumerar(&o, f);
    }
    free(o.globais);
}
