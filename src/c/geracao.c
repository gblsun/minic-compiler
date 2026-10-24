/* Back-end do MINIC — ver geracao.h e src/python/geracao.py. */

#include "geracao.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *ger_realloc(void *ptr, size_t tamanho) {
    void *novo = realloc(ptr, tamanho);
    if (!novo) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    return novo;
}

#define GER_PUSH(arr, len, cap, item)                                    \
    do {                                                                 \
        if ((len) == (cap)) {                                            \
            (cap) = (cap) ? (cap) * 2 : 8;                               \
            (arr) = ger_realloc((arr), (cap) * sizeof *(arr));           \
        }                                                                \
        (arr)[(len)++] = (item);                                         \
    } while (0)

static char *ger_format(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    char *s = ger_realloc(NULL, (size_t)n + 1);
    va_start(args, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, args);
    va_end(args);
    return s;
}

/* Texto que cresce dobrando de tamanho (o "".join do Python). */
typedef struct {
    char *buf;
    size_t len, cap;
} GerTexto;

static void ger_escrever(GerTexto *t, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    while (t->len + (size_t)n + 1 > t->cap) {
        t->cap = t->cap ? t->cap * 2 : 256;
        t->buf = ger_realloc(t->buf, t->cap);
    }
    va_start(args, fmt);
    vsnprintf(t->buf + t->len, (size_t)n + 1, fmt, args);
    va_end(args);
    t->len += (size_t)n;
}

static int ger_eh(const char *a, const char *b) { return a && strcmp(a, b) == 0; }

static int ger_global(const IrPrograma *ir, const char *nome) {
    return ir_tipo_global(ir, nome) != NULL;
}

/* ------------------------------------------------------------------ *
 * Registro de ativação
 * ------------------------------------------------------------------ */

static const IrSimbolo *ger_simbolo(const IrFuncao *f, const char *nome_ir) {
    const IrSimbolo *achado = NULL;
    for (size_t i = 0; i < f->nsimbolos; i++) {
        if (f->simbolos[i].nome_ir == nome_ir) {
            achado = &f->simbolos[i]; /* como o dict do Python: o último vale */
        }
    }
    return achado;
}

static int ger_tem_slot(const VmSlot *slots, size_t n, const char *nome) {
    for (size_t i = 0; i < n; i++) {
        if (slots[i].nome == nome) {
            return 1;
        }
    }
    return 0;
}

static void ger_slot(VmSlot **slots, size_t *n, size_t *cap, const char *nome, const char *tipo,
                     long tamanho, int vetor_param) {
    VmSlot slot = {(int)*n, nome, tipo, tamanho, vetor_param};
    GER_PUSH(*slots, *n, *cap, slot);
}

/* Slots do quadro: parâmetros, locais declarados, temporários. */
static VmSlot *ger_quadro(const IrPrograma *ir, const IrFuncao *f, size_t *nslots,
                          size_t *capslots) {
    VmSlot *slots = NULL;
    *nslots = 0;
    *capslots = 0;
    for (size_t i = 0; i < f->nparams; i++) {
        const IrSimbolo *s = ger_simbolo(f, f->params[i]);
        ger_slot(&slots, nslots, capslots, f->params[i], s->tipo, -1,
                 ger_eh(s->categoria, "parâmetro vetor"));
    }
    for (size_t i = 0; i < f->nsimbolos; i++) {
        const IrSimbolo *s = &f->simbolos[i];
        if (ger_tem_slot(slots, *nslots, s->nome_ir)) {
            continue;
        }
        long tamanho = s->tamanho ? strtol(s->tamanho, NULL, 10) : -1;
        ger_slot(&slots, nslots, capslots, s->nome_ir, s->tipo, tamanho, 0);
    }
    for (size_t i = 0; i < f->len; i++) {
        const IrInstr *ins = &f->instrs[i];
        const char *nomes[4] = {NULL, NULL, NULL, NULL};
        if (ins->kind == IR_CALL) {
            nomes[0] = ins->dst;
        } else if (ins->kind != IR_GOTO && ins->kind != IR_LABEL) {
            nomes[0] = ins->dst;
            nomes[1] = ins->a;
            nomes[2] = ins->b;
            nomes[3] = ins->c;
        }
        for (size_t k = 0; k < 4; k++) {
            const char *nome = nomes[k];
            if (!nome || ger_tem_slot(slots, *nslots, nome) || ger_global(ir, nome) ||
                ir_eh_literal(nome)) {
                continue;
            }
            ger_slot(&slots, nslots, capslots, nome, ir_tipo_local(f, nome), -1, 0);
        }
    }
    return slots;
}

/* ------------------------------------------------------------------ *
 * Tradução
 * ------------------------------------------------------------------ */

typedef struct {
    IrPrograma *ir;
    const IrFuncao *f;
    VmFuncao *saida;
} Gerador;

static const char *ger_operador(const char *op) {
    static const char *tabela[][2] = {
        {"+", "ADD"}, {"-", "SUB"}, {"*", "MUL"}, {"/", "DIV"}, {"%", "MOD"},
        {"<", "LT"},  {"<=", "LE"}, {">", "GT"},  {">=", "GE"}, {"==", "EQ"},
        {"!=", "NE"}, {"and", "AND"}, {"or", "OR"}, {"neg", "NEG"}, {"not", "NOT"},
        {"itof", "ITOF"}, {"ctoi", "CTOI"},
    };
    for (size_t i = 0; i < sizeof tabela / sizeof tabela[0]; i++) {
        if (strcmp(tabela[i][0], op) == 0) {
            return tabela[i][1];
        }
    }
    return NULL;
}

static void ger_emitir(Gerador *g, const char *op, const char *arg, int n,
                       const char *comentario) {
    VmInstr ins = {op, arg, n, comentario};
    GER_PUSH(g->saida->codigo, g->saida->len, g->saida->cap, ins);
}

static int ger_posicao(const Gerador *g, const char *nome) {
    for (size_t i = 0; i < g->saida->nslots; i++) {
        if (g->saida->slots[i].nome == nome) {
            return (int)i;
        }
    }
    return -1;
}

static const char *ger_numero(Gerador *g, int n) {
    char buf[16];
    snprintf(buf, sizeof buf, "%d", n);
    return ir_intern(g->ir, buf);
}

static void ger_empilhar(Gerador *g, const char *operando) {
    int pos;
    if (ir_eh_literal(operando)) {
        ger_emitir(g, "PUSH", operando, 0, NULL);
    } else if ((pos = ger_posicao(g, operando)) >= 0) {
        ger_emitir(g, "LOAD", ger_numero(g, pos), 0, operando);
    } else {
        ger_emitir(g, "GLOAD", operando, 0, NULL);
    }
}

static void ger_guardar(Gerador *g, const char *nome) {
    int pos = ger_posicao(g, nome);
    if (pos >= 0) {
        ger_emitir(g, "STORE", ger_numero(g, pos), 0, nome);
    } else {
        ger_emitir(g, "GSTORE", nome, 0, NULL);
    }
}

/* ALOAD/ASTORE no vetor local (slot) ou GALOAD/GASTORE no global (nome). */
static void ger_vetor(Gerador *g, int escrita, const char *nome) {
    int pos = ger_posicao(g, nome);
    if (pos >= 0) {
        ger_emitir(g, escrita ? "ASTORE" : "ALOAD", ger_numero(g, pos), 0, nome);
    } else {
        ger_emitir(g, escrita ? "GASTORE" : "GALOAD", nome, 0, NULL);
    }
}

static int ger_instr(Gerador *g, const IrInstr *ins, char **erro) {
    switch (ins->kind) {
    case IR_COPY:
        ger_empilhar(g, ins->a);
        ger_guardar(g, ins->dst);
        break;
    case IR_BIN:
        ger_empilhar(g, ins->a);
        ger_empilhar(g, ins->b);
        ger_emitir(g, ger_operador(ins->op), NULL, 0, NULL);
        ger_guardar(g, ins->dst);
        break;
    case IR_UN:
        ger_empilhar(g, ins->a);
        ger_emitir(g, ger_operador(ins->op), NULL, 0, NULL);
        ger_guardar(g, ins->dst);
        break;
    case IR_LOAD:
        ger_empilhar(g, ins->b);
        ger_vetor(g, 0, ins->a);
        ger_guardar(g, ins->dst);
        break;
    case IR_STORE:
        ger_empilhar(g, ins->b);
        ger_empilhar(g, ins->c);
        ger_vetor(g, 1, ins->a);
        break;
    case IR_PRINT:
        ger_empilhar(g, ins->a);
        ger_emitir(g, "PRINT", NULL, 0, NULL);
        break;
    case IR_READ: {
        const char *tipo = ir_tipo_local(g->f, ins->dst);
        if (!tipo) {
            tipo = ir_tipo_global(g->ir, ins->dst);
        }
        if (!tipo) {
            *erro = ger_format("tipo desconhecido para read de “%s”.", ins->dst);
            return 5;
        }
        ger_emitir(g, "READ", tipo, 0, NULL);
        ger_guardar(g, ins->dst);
        break;
    }
    case IR_PARAM:
        ger_empilhar(g, ins->a);
        break;
    case IR_CALL:
        ger_emitir(g, "CALL", ins->a, ins->n, NULL);
        if (ins->dst) {
            ger_guardar(g, ins->dst);
        } else {
            ger_emitir(g, "POP", NULL, 0, NULL);
        }
        break;
    case IR_RET:
        ger_empilhar(g, ins->a ? ins->a : ir_intern(g->ir, "0"));
        ger_emitir(g, "RET", NULL, 0, NULL);
        break;
    case IR_GOTO:
        ger_emitir(g, "JMP", ins->label, 0, NULL);
        break;
    case IR_IFF:
    case IR_IFT:
        ger_empilhar(g, ins->a);
        ger_emitir(g, ins->kind == IR_IFF ? "JZ" : "JNZ", ins->label, 0, NULL);
        break;
    case IR_LABEL:
        ger_emitir(g, "LABEL", ins->label, 0, NULL);
        break;
    }
    return 0;
}

int geracao_gerar(IrPrograma *ir, VmPrograma *saida, char **erro) {
    *erro = NULL;
    int tem_main = 0;
    for (size_t i = 0; i < ir->nfuncoes; i++) {
        tem_main |= strcmp(ir->funcoes[i].nome, "main") == 0;
    }
    if (!tem_main) {
        *erro = ger_format("o programa não define a função main.");
        return 5;
    }
    for (size_t i = 0; i < ir->nglobais; i++) {
        VmGlobal g = {ir->globais[i].nome, ir->globais[i].tipo, ir->globais[i].tamanho};
        GER_PUSH(saida->globais, saida->nglobais, saida->capglobais, g);
    }
    for (size_t i = 0; i < ir->nfuncoes; i++) {
        const IrFuncao *f = &ir->funcoes[i];
        VmFuncao vf;
        memset(&vf, 0, sizeof vf);
        vf.nome = f->nome;
        vf.nparams = (int)f->nparams;
        vf.slots = ger_quadro(ir, f, &vf.nslots, &vf.capslots);
        Gerador g = {ir, f, &vf};
        for (size_t j = 0; j < f->len; j++) {
            int codigo = ger_instr(&g, &f->instrs[j], erro);
            if (codigo) {
                free(vf.slots);
                free(vf.codigo);
                return codigo;
            }
        }
        if (vf.len == 0 || strcmp(vf.codigo[vf.len - 1].op, "RET") != 0) {
            ger_emitir(&g, "PUSH", ir_intern(ir, "0"), 0, NULL); /* fim sem return: void */
            ger_emitir(&g, "RET", NULL, 0, NULL);
        }
        GER_PUSH(saida->funcoes, saida->nfuncoes, saida->capfuncoes, vf);
    }
    return 0;
}

void geracao_free(VmPrograma *programa) {
    for (size_t i = 0; i < programa->nfuncoes; i++) {
        free(programa->funcoes[i].slots);
        free(programa->funcoes[i].codigo);
    }
    free(programa->funcoes);
    free(programa->globais);
    memset(programa, 0, sizeof *programa);
}

/* ------------------------------------------------------------------ *
 * Texto (--target=asm)
 * ------------------------------------------------------------------ */

char *geracao_formatar(const VmPrograma *programa) {
    GerTexto t = {NULL, 0, 0};
    ger_escrever(&t, "; MINIC — código para a máquina de pilha (MVM)\n");
    for (size_t i = 0; i < programa->nglobais; i++) {
        const VmGlobal *g = &programa->globais[i];
        if (g->tamanho) {
            ger_escrever(&t, ".global %s %s[%s]\n", g->nome, g->tipo, g->tamanho);
        } else {
            ger_escrever(&t, ".global %s %s\n", g->nome, g->tipo);
        }
    }
    for (size_t i = 0; i < programa->nfuncoes; i++) {
        const VmFuncao *f = &programa->funcoes[i];
        ger_escrever(&t, ".func %s %d\n", f->nome, f->nparams);
        for (size_t j = 0; j < f->nslots; j++) {
            const VmSlot *s = &f->slots[j];
            if (!s->tipo) {
                ger_escrever(&t, ".slot %d %s\n", s->indice, s->nome);
            } else if (s->tamanho >= 0) {
                ger_escrever(&t, ".slot %d %s %s[%ld]\n", s->indice, s->nome, s->tipo, s->tamanho);
            } else if (s->vetor_param) {
                ger_escrever(&t, ".slot %d %s %s[]\n", s->indice, s->nome, s->tipo);
            } else {
                ger_escrever(&t, ".slot %d %s %s\n", s->indice, s->nome, s->tipo);
            }
        }
        for (size_t j = 0; j < f->len; j++) {
            const VmInstr *ins = &f->codigo[j];
            if (strcmp(ins->op, "LABEL") == 0) {
                ger_escrever(&t, "%s:\n", ins->arg);
                continue;
            }
            ger_escrever(&t, "  %s", ins->op);
            if (ins->arg) {
                ger_escrever(&t, " %s", ins->arg);
            }
            if (strcmp(ins->op, "CALL") == 0) {
                ger_escrever(&t, " %d", ins->n);
            }
            if (ins->comentario) {
                ger_escrever(&t, "  ; %s", ins->comentario);
            }
            ger_escrever(&t, "\n");
        }
        ger_escrever(&t, ".endfunc\n");
    }
    return t.buf;
}

/* ------------------------------------------------------------------ *
 * Tabela de símbolos (--symbols)
 * ------------------------------------------------------------------ */

typedef struct {
    const char *funcao; /* "" para o conjunto global */
    const char *nome;
} GerUso;

typedef struct {
    GerUso *itens;
    size_t len, cap;
} GerUsos;

static int ger_usado(const GerUsos *u, const char *funcao, const char *nome) {
    for (size_t i = 0; i < u->len; i++) {
        if (strcmp(u->itens[i].funcao, funcao) == 0 && strcmp(u->itens[i].nome, nome) == 0) {
            return 1;
        }
    }
    return 0;
}

static void ger_usar(GerUsos *u, const char *funcao, const char *nome) {
    if (!ger_usado(u, funcao, nome)) {
        GerUso uso = {funcao, nome};
        GER_PUSH(u->itens, u->len, u->cap, uso);
    }
}

static GerUsos ger_usos(const IrPrograma *ir) {
    GerUsos u = {NULL, 0, 0};
    for (size_t i = 0; i < ir->nfuncoes; i++) {
        const IrFuncao *f = &ir->funcoes[i];
        for (size_t j = 0; j < f->len; j++) {
            const IrInstr *ins = &f->instrs[j];
            const char *lidos[3] = {NULL, NULL, NULL};
            switch (ins->kind) {
            case IR_COPY:
            case IR_UN:
            case IR_PRINT:
            case IR_PARAM:
            case IR_RET:
            case IR_IFF:
            case IR_IFT:
                lidos[0] = ins->a;
                break;
            case IR_BIN:
            case IR_LOAD:
                lidos[0] = ins->a;
                lidos[1] = ins->b;
                break;
            case IR_STORE: /* escrever no elemento usa o vetor */
                lidos[0] = ins->a;
                lidos[1] = ins->b;
                lidos[2] = ins->c;
                break;
            case IR_CALL:
                ger_usar(&u, "", ins->a); /* função chamada */
                break;
            default:
                break;
            }
            for (size_t k = 0; k < 3; k++) {
                if (lidos[k] && !ir_eh_literal(lidos[k])) {
                    ger_usar(&u, f->nome, lidos[k]);
                    ger_usar(&u, "", lidos[k]);
                }
            }
        }
    }
    return u;
}

static void ger_nome_e_tipo(GerTexto *t, const IrSimbolo *s) {
    if (strcmp(s->nome, s->nome_ir) == 0) {
        ger_escrever(t, "  %s: %s ", s->nome, s->categoria);
    } else {
        ger_escrever(t, "  %s (%s): %s ", s->nome, s->nome_ir, s->categoria);
    }
    if (ger_eh(s->categoria, "vetor")) {
        ger_escrever(t, "%s[%s]", s->tipo, s->tamanho);
    } else if (ger_eh(s->categoria, "parâmetro vetor")) {
        ger_escrever(t, "%s[]", s->tipo);
    } else {
        ger_escrever(t, "%s", s->tipo);
    }
}

static void ger_estado(GerTexto *t, const IrSimbolo *s, int usado) {
    if (!ger_eh(s->categoria, "função")) {
        ger_escrever(t, "%s, ", s->inicializada ? "inicializada" : "não inicializada");
    }
    ger_escrever(t, "%s", usado ? "usada" : "não usada");
}

char *geracao_simbolos(IrPrograma *ir) {
    GerUsos usos = ger_usos(ir);
    GerTexto t = {NULL, 0, 0};
    ger_escrever(&t, "escopo global (nível 0):\n");
    for (size_t i = 0; i < ir->nsimbolos; i++) {
        const IrSimbolo *s = &ir->simbolos[i];
        ger_nome_e_tipo(&t, s);
        ger_escrever(&t, " — linha %d, coluna %d; ", s->linha, s->coluna);
        ger_estado(&t, s, ger_usado(&usos, "", s->nome_ir));
        ger_escrever(&t, "\n");
    }
    for (size_t i = 0; i < ir->nfuncoes; i++) {
        const IrFuncao *f = &ir->funcoes[i];
        size_t nslots, capslots;
        VmSlot *slots = ger_quadro(ir, f, &nslots, &capslots);
        ger_escrever(&t, "função %s (registro de ativação: %d posições):\n", f->nome, (int)nslots);
        for (size_t j = 0; j < nslots; j++) {
            const IrSimbolo *s = ger_simbolo(f, slots[j].nome);
            if (!s) {
                ger_escrever(&t, "  %s: temporário — deslocamento %d\n", slots[j].nome,
                             slots[j].indice);
                continue;
            }
            ger_nome_e_tipo(&t, s);
            ger_escrever(&t, ", nível %d — linha %d, coluna %d; ", s->nivel, s->linha, s->coluna);
            ger_estado(&t, s, ger_usado(&usos, f->nome, s->nome_ir));
            ger_escrever(&t, "; deslocamento %d\n", slots[j].indice);
        }
        free(slots);
    }
    free(usos.itens);
    return t.buf;
}
