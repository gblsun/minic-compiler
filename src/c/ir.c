/* Representação intermediária (IR) do MINIC — ver ir.h e src/python/ir.py. */

#include "ir.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *irg_realloc(void *ptr, size_t tamanho) {
    void *novo = realloc(ptr, tamanho);
    if (!novo) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    return novo;
}

/* Acrescenta `item` ao vetor dinâmico (arr, len, cap), dobrando quando enche. */
#define IRG_PUSH(arr, len, cap, item)                                    \
    do {                                                                 \
        if ((len) == (cap)) {                                            \
            (cap) = (cap) ? (cap) * 2 : 8;                               \
            (arr) = irg_realloc((arr), (cap) * sizeof *(arr));           \
        }                                                                \
        (arr)[(len)++] = (item);                                         \
    } while (0)

static char *irg_format(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    char *s = irg_realloc(NULL, (size_t)n + 1);
    va_start(args, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, args);
    va_end(args);
    return s;
}

/* ------------------------------------------------------------------ *
 * Pool de strings
 * ------------------------------------------------------------------ */

static size_t irg_hash(const char *s) {
    size_t h = 2166136261u; /* FNV-1a */
    for (; *s; s++) {
        h = (h ^ (unsigned char)*s) * 16777619u;
    }
    return h;
}

static void irg_pool_crescer(IrPool *pool) {
    size_t cap = pool->cap ? pool->cap * 2 : 64;
    char **slots = calloc(cap, sizeof *slots);
    if (!slots) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    for (size_t i = 0; i < pool->cap; i++) {
        if (pool->slots[i]) {
            size_t j = irg_hash(pool->slots[i]) & (cap - 1);
            while (slots[j]) {
                j = (j + 1) & (cap - 1);
            }
            slots[j] = pool->slots[i];
        }
    }
    free(pool->slots);
    pool->slots = slots;
    pool->cap = cap;
}

const char *ir_intern(IrPrograma *programa, const char *texto) {
    IrPool *pool = &programa->pool;
    if ((pool->len + 1) * 2 > pool->cap) {
        irg_pool_crescer(pool);
    }
    size_t i = irg_hash(texto) & (pool->cap - 1);
    while (pool->slots[i]) {
        if (strcmp(pool->slots[i], texto) == 0) {
            return pool->slots[i];
        }
        i = (i + 1) & (pool->cap - 1);
    }
    size_t n = strlen(texto);
    char *copia = irg_realloc(NULL, n + 1);
    memcpy(copia, texto, n + 1);
    pool->slots[i] = copia;
    pool->len++;
    return copia;
}

/* Interna uma string recém-alocada e a libera. */
static const char *irg_intern_livre(IrPrograma *programa, char *texto) {
    const char *unico = ir_intern(programa, texto);
    free(texto);
    return unico;
}

void ir_programa_free(IrPrograma *programa) {
    for (size_t i = 0; i < programa->nfuncoes; i++) {
        free(programa->funcoes[i].params);
        free(programa->funcoes[i].instrs);
        free(programa->funcoes[i].simbolos);
        free(programa->funcoes[i].tipos);
    }
    free(programa->funcoes);
    free(programa->globais);
    free(programa->simbolos);
    for (size_t i = 0; i < programa->pool.cap; i++) {
        free(programa->pool.slots[i]);
    }
    free(programa->pool.slots);
    memset(programa, 0, sizeof *programa);
}

/* ------------------------------------------------------------------ *
 * Operandos e instruções
 * ------------------------------------------------------------------ */

int ir_eh_literal(const char *operando) {
    unsigned char c = (unsigned char)operando[0];
    if (isdigit(c) || c == '\'' || c == '"') {
        return 1;
    }
    if (c == '-' && isdigit((unsigned char)operando[1])) {
        return 1;
    }
    return strcmp(operando, "true") == 0 || strcmp(operando, "false") == 0;
}

int ir_eh_temporario(const char *nome) {
    if (nome[0] != 't' || nome[1] == '\0') {
        return 0;
    }
    for (const char *s = nome + 1; *s; s++) {
        if (!isdigit((unsigned char)*s)) {
            return 0;
        }
    }
    return 1;
}

void ir_emitir(IrFuncao *funcao, IrInstr instr) {
    IRG_PUSH(funcao->instrs, funcao->len, funcao->cap, instr);
}

const char *ir_tipo_local(const IrFuncao *funcao, const char *nome) {
    for (size_t i = funcao->ntipos; i-- > 0;) {
        if (strcmp(funcao->tipos[i].nome, nome) == 0) {
            return funcao->tipos[i].tipo;
        }
    }
    return NULL;
}

const char *ir_tipo_global(const IrPrograma *programa, const char *nome) {
    for (size_t i = 0; i < programa->nglobais; i++) {
        if (strcmp(programa->globais[i].nome, nome) == 0) {
            return programa->globais[i].tipo;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ *
 * Texto
 * ------------------------------------------------------------------ */

typedef struct {
    char *buf;
    size_t len, cap;
} IrgTexto;

static void irg_escrever(IrgTexto *t, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (t->len + (size_t)n + 1 > t->cap) {
        while (t->len + (size_t)n + 1 > t->cap) {
            t->cap = t->cap ? t->cap * 2 : 256;
        }
        t->buf = irg_realloc(t->buf, t->cap);
    }
    va_start(args, fmt);
    vsnprintf(t->buf + t->len, (size_t)n + 1, fmt, args);
    va_end(args);
    t->len += (size_t)n;
}

static void irg_instr_texto(IrgTexto *t, const IrInstr *ins) {
    switch (ins->kind) {
    case IR_COPY:
        irg_escrever(t, "  %s = %s\n", ins->dst, ins->a);
        break;
    case IR_BIN:
        irg_escrever(t, "  %s = %s %s %s\n", ins->dst, ins->a, ins->op, ins->b);
        break;
    case IR_UN:
        irg_escrever(t, "  %s = %s %s\n", ins->dst, ins->op, ins->a);
        break;
    case IR_LOAD:
        irg_escrever(t, "  %s = load %s[%s]\n", ins->dst, ins->a, ins->b);
        break;
    case IR_STORE:
        irg_escrever(t, "  store %s[%s], %s\n", ins->a, ins->b, ins->c);
        break;
    case IR_PRINT:
        irg_escrever(t, "  print %s\n", ins->a);
        break;
    case IR_READ:
        irg_escrever(t, "  read %s\n", ins->dst);
        break;
    case IR_PARAM:
        irg_escrever(t, "  param %s\n", ins->a);
        break;
    case IR_CALL:
        if (ins->dst) {
            irg_escrever(t, "  %s = call %s, %d\n", ins->dst, ins->a, ins->n);
        } else {
            irg_escrever(t, "  call %s, %d\n", ins->a, ins->n);
        }
        break;
    case IR_RET:
        if (ins->a) {
            irg_escrever(t, "  return %s\n", ins->a);
        } else {
            irg_escrever(t, "  return\n");
        }
        break;
    case IR_GOTO:
        irg_escrever(t, "  goto %s\n", ins->label);
        break;
    case IR_IFF:
        irg_escrever(t, "  ifFalse %s goto %s\n", ins->a, ins->label);
        break;
    case IR_IFT:
        irg_escrever(t, "  ifTrue %s goto %s\n", ins->a, ins->label);
        break;
    case IR_LABEL:
        irg_escrever(t, "%s:\n", ins->label);
        break;
    }
}

char *ir_formatar(const IrPrograma *programa) {
    IrgTexto t = {NULL, 0, 0};
    irg_escrever(&t, "%s", "");
    for (size_t i = 0; i < programa->nglobais; i++) {
        const IrGlobal *g = &programa->globais[i];
        if (g->tamanho) {
            irg_escrever(&t, "global %s[%s]\n", g->nome, g->tamanho);
        } else {
            irg_escrever(&t, "global %s\n", g->nome);
        }
    }
    for (size_t i = 0; i < programa->nfuncoes; i++) {
        const IrFuncao *f = &programa->funcoes[i];
        irg_escrever(&t, "func %s", f->nome);
        for (size_t j = 0; j < f->nparams; j++) {
            irg_escrever(&t, "%s%s", j == 0 ? "(" : ", ", f->params[j]);
        }
        irg_escrever(&t, "%s\n", f->nparams ? ")" : "");
        for (size_t j = 0; j < f->len; j++) {
            irg_instr_texto(&t, &f->instrs[j]);
        }
        irg_escrever(&t, "endfunc\n");
    }
    return t.buf;
}

/* ------------------------------------------------------------------ *
 * Gerador
 * ------------------------------------------------------------------ */

typedef struct {
    const char *fonte; /* nome no código */
    const char *nome_ir;
    const char *tipo;
    int eh_vetor;
} IrgVariavel;

typedef struct {
    IrgVariavel *itens;
    size_t len, cap;
} IrgEscopo;

typedef struct {
    const char *saida;        /* destino de break */
    const char *continuacao;  /* destino de continue (no for, criado sob demanda) */
} IrgLaco;

typedef struct {
    const char *nome;
    int n;
} IrgContador;

typedef struct {
    const char *op; /* operando com o valor (NULL: chamada sem valor) */
    const char *tipo;
} IrgValor;

typedef struct {
    IrPrograma *p;
    const Ast *programa;
    IrgVariavel *globais;
    size_t nglobais, capglobais;
    IrgEscopo *escopos;
    size_t nescopos, capescopos;
    IrFuncao funcao; /* função em construção */
    const char *tipo_retorno;
    int temporarios;
    IrgContador *rotulos;
    size_t nrotulos, caprotulos;
    IrgContador *sufixos;
    size_t nsufixos, capsufixos;
    const char **sufixar; /* nomes que ganham sufixo nesta função */
    size_t nsufixar, capsufixar;
    IrgLaco *lacos;
    size_t nlacos, caplacos;
} IrGerador;

static IrgValor irg_expr(IrGerador *g, const Ast *no);
static void irg_comando(IrGerador *g, const Ast *no);

/* Tipo de cada categoria de literal (`real` é o nome do token de float). */
static const char *irg_tipo_literal(const char *categoria) {
    if (strcmp(categoria, "real") == 0) {
        return "float";
    }
    return categoria; /* int, bool, char, string */
}

static int irg_contem_chamada(const Ast *no) {
    if (!no) {
        return 0;
    }
    switch (no->kind) {
    case AST_CALL:
        return 1;
    case AST_BINARY:
    case AST_ASSIGN:
    case AST_INDEX:
        return irg_contem_chamada(no->a) || irg_contem_chamada(no->b);
    case AST_UNARY:
        return irg_contem_chamada(no->a);
    default:
        return 0;
    }
}

/* Nomes declarados num corpo de função, em ordem (para decidir sufixos). */
static void irg_coletar_declaracoes(const Ast *no, const char ***saida, size_t *len,
                                    size_t *cap) {
    if (!no) {
        return;
    }
    if (no->kind == AST_BLOCK) {
        for (size_t i = 0; i < no->items_len; i++) {
            const Ast *item = no->items[i];
            if (item->kind == AST_VAR_DECL) {
                IRG_PUSH(*saida, *len, *cap, (const char *)item->text);
            } else {
                irg_coletar_declaracoes(item, saida, len, cap);
            }
        }
    } else if (no->kind == AST_IF) {
        irg_coletar_declaracoes(no->b, saida, len, cap);
        irg_coletar_declaracoes(no->c, saida, len, cap);
    } else if (no->kind == AST_WHILE) {
        irg_coletar_declaracoes(no->b, saida, len, cap);
    } else if (no->kind == AST_FOR) {
        irg_coletar_declaracoes(no->d, saida, len, cap);
    }
}

static const IrgVariavel *irg_global(const IrGerador *g, const char *nome) {
    for (size_t i = g->nglobais; i-- > 0;) {
        if (strcmp(g->globais[i].fonte, nome) == 0) {
            return &g->globais[i];
        }
    }
    return NULL;
}

static void irg_entrar(IrGerador *g, const char *nome, const char *tipo_retorno,
                       const char **declaracoes, size_t ndecl) {
    memset(&g->funcao, 0, sizeof g->funcao);
    g->funcao.nome = ir_intern(g->p, nome);
    g->tipo_retorno = tipo_retorno;
    g->temporarios = 0;
    g->nrotulos = 0;
    g->nsufixos = 0;
    for (size_t i = 0; i < g->nescopos; i++) {
        free(g->escopos[i].itens);
    }
    g->nescopos = 0;
    IrgEscopo vazio = {NULL, 0, 0};
    IRG_PUSH(g->escopos, g->nescopos, g->capescopos, vazio);

    g->nsufixar = 0;
    for (size_t i = 0; i < ndecl; i++) {
        const char *nome_i = declaracoes[i];
        int ja = 0;
        for (size_t j = 0; j < g->nsufixar; j++) {
            if (strcmp(g->sufixar[j], nome_i) == 0) {
                ja = 1;
            }
        }
        if (ja) {
            continue;
        }
        int contagem = 0;
        for (size_t j = 0; j < ndecl; j++) {
            contagem += strcmp(declaracoes[j], nome_i) == 0;
        }
        if (contagem > 1 || irg_global(g, nome_i) || ir_eh_temporario(nome_i)) {
            IRG_PUSH(g->sufixar, g->nsufixar, g->capsufixar, nome_i);
        }
    }
}

static void irg_terminar_funcao(IrGerador *g) {
    IRG_PUSH(g->p->funcoes, g->p->nfuncoes, g->p->capfuncoes, g->funcao);
    memset(&g->funcao, 0, sizeof g->funcao);
}

static void irg_emitir(IrGerador *g, IrKind kind, const char *dst, const char *a, const char *op,
                       const char *b, const char *c, const char *label, int n) {
    IrInstr ins = {kind, dst, a, b, c, op, label, n};
    ir_emitir(&g->funcao, ins);
}

static const char *irg_temp(IrGerador *g) {
    g->temporarios++;
    return irg_intern_livre(g->p, irg_format("t%d", g->temporarios));
}

static const char *irg_rotulo(IrGerador *g, const char *base) {
    for (size_t i = 0; i < g->nrotulos; i++) {
        if (strcmp(g->rotulos[i].nome, base) == 0) {
            g->rotulos[i].n++;
            return irg_intern_livre(g->p, irg_format("%s%d", base, g->rotulos[i].n));
        }
    }
    IrgContador novo = {base, 1};
    IRG_PUSH(g->rotulos, g->nrotulos, g->caprotulos, novo);
    return ir_intern(g->p, base);
}

static void irg_registrar_tipo(IrGerador *g, const char *nome_ir, const char *tipo) {
    IrTipo par = {nome_ir, ir_intern(g->p, tipo)};
    IRG_PUSH(g->funcao.tipos, g->funcao.ntipos, g->funcao.captipos, par);
}

/* Declara o parâmetro ou a variável local `no` no escopo atual. */
static const char *irg_declarar(IrGerador *g, const Ast *no, const char *categoria,
                                const char *tamanho, int inicializada) {
    const char *nome = no->text;
    const char *tipo = no->type_name;
    const char *nome_ir = NULL;
    for (size_t i = 0; i < g->nsufixar; i++) {
        if (strcmp(g->sufixar[i], nome) == 0) {
            int n = 0;
            for (size_t j = 0; j < g->nsufixos; j++) {
                if (strcmp(g->sufixos[j].nome, nome) == 0) {
                    n = ++g->sufixos[j].n;
                }
            }
            if (n == 0) {
                IrgContador novo = {nome, 1};
                IRG_PUSH(g->sufixos, g->nsufixos, g->capsufixos, novo);
                n = 1;
            }
            nome_ir = irg_intern_livre(g->p, irg_format("%s#%d", nome, n));
            break;
        }
    }
    if (!nome_ir) {
        nome_ir = ir_intern(g->p, nome);
    }
    int eh_vetor = tamanho != NULL || (no->kind == AST_PARAM && no->is_array);
    if (eh_vetor && strcmp(categoria, "parâmetro") == 0) {
        categoria = "parâmetro vetor";
    }
    IrgEscopo *escopo = &g->escopos[g->nescopos - 1];
    IrgVariavel var = {nome, nome_ir, tipo, eh_vetor};
    IRG_PUSH(escopo->itens, escopo->len, escopo->cap, var);
    IrSimbolo simbolo = {ir_intern(g->p, nome), nome_ir, ir_intern(g->p, categoria),
                         ir_intern(g->p, tipo), tamanho ? ir_intern(g->p, tamanho) : NULL,
                         (int)g->nescopos, no->line, no->column, inicializada};
    IRG_PUSH(g->funcao.simbolos, g->funcao.nsimbolos, g->funcao.capsimbolos, simbolo);
    irg_registrar_tipo(g, nome_ir, tipo);
    return nome_ir;
}

static const IrgVariavel *irg_buscar(const IrGerador *g, const char *nome) {
    for (size_t i = g->nescopos; i-- > 0;) {
        const IrgEscopo *escopo = &g->escopos[i];
        for (size_t j = escopo->len; j-- > 0;) {
            if (strcmp(escopo->itens[j].fonte, nome) == 0) {
                return &escopo->itens[j];
            }
        }
    }
    return irg_global(g, nome);
}

/* Conversão implícita explícita na IR: char -> int e int -> float. */
static const char *irg_converter(IrGerador *g, const char *operando, const char *de,
                                 const char *para) {
    if (strcmp(de, "char") == 0 && (strcmp(para, "int") == 0 || strcmp(para, "float") == 0)) {
        const char *t = irg_temp(g);
        irg_emitir(g, IR_UN, t, operando, ir_intern(g->p, "ctoi"), NULL, NULL, NULL, 0);
        operando = t;
        de = "int";
    }
    if (strcmp(de, "int") == 0 && strcmp(para, "float") == 0) {
        if (ir_eh_literal(operando)) {
            return irg_intern_livre(g->p, irg_format("%s.0", operando));
        }
        const char *t = irg_temp(g);
        irg_emitir(g, IR_UN, t, operando, ir_intern(g->p, "itof"), NULL, NULL, NULL, 0);
        return t;
    }
    return operando;
}

static void irg_atribuir_variavel(IrGerador *g, const char *nome_ir, const char *tipo,
                                  const Ast *valor) {
    IrgValor v = irg_expr(g, valor);
    const char *operando = irg_converter(g, v.op, v.tipo, tipo);
    irg_emitir(g, IR_COPY, nome_ir, operando, NULL, NULL, NULL, NULL, 0);
}

static const char *irg_endereco(IrGerador *g, const Ast *indice) {
    IrgValor v = irg_expr(g, indice);
    const char *operando = irg_converter(g, v.op, v.tipo, "int");
    const char *t = irg_temp(g);
    irg_emitir(g, IR_BIN, t, operando, ir_intern(g->p, "*"), ir_intern(g->p, "1"), NULL, NULL,
               0);
    return t;
}

/* -- comandos -------------------------------------------------------- */

static void irg_item(IrGerador *g, const Ast *item) {
    if (item->kind == AST_VAR_DECL) {
        const char *categoria = item->b ? "vetor" : "variável";
        const char *tamanho = item->b ? item->b->text : NULL;
        const char *nome_ir = irg_declarar(g, item, categoria, tamanho, item->a != NULL);
        if (item->a) {
            irg_atribuir_variavel(g, nome_ir, item->type_name, item->a);
        }
    } else {
        irg_comando(g, item);
    }
}

static IrgValor irg_chamada(IrGerador *g, const Ast *no, int com_valor);

static void irg_efeito(IrGerador *g, const Ast *expr) {
    if (expr->kind == AST_CALL) {
        irg_chamada(g, expr, 0);
    } else {
        irg_expr(g, expr);
    }
}

static void irg_if(IrGerador *g, const Ast *no) {
    IrgValor cond = irg_expr(g, no->a);
    if (!no->c) {
        const char *fim = irg_rotulo(g, "L_end_if");
        irg_emitir(g, IR_IFF, NULL, cond.op, NULL, NULL, NULL, fim, 0);
        irg_comando(g, no->b);
        irg_emitir(g, IR_LABEL, NULL, NULL, NULL, NULL, NULL, fim, 0);
        return;
    }
    const char *senao = irg_rotulo(g, "L_else");
    const char *fim = irg_rotulo(g, "L_end");
    irg_emitir(g, IR_IFF, NULL, cond.op, NULL, NULL, NULL, senao, 0);
    irg_comando(g, no->b);
    irg_emitir(g, IR_GOTO, NULL, NULL, NULL, NULL, NULL, fim, 0);
    irg_emitir(g, IR_LABEL, NULL, NULL, NULL, NULL, NULL, senao, 0);
    irg_comando(g, no->c);
    irg_emitir(g, IR_LABEL, NULL, NULL, NULL, NULL, NULL, fim, 0);
}

static void irg_for(IrGerador *g, const Ast *no) {
    if (no->a) {
        irg_efeito(g, no->a);
    }
    const char *teste = irg_rotulo(g, "L_test");
    const char *fim = irg_rotulo(g, "L_end");
    irg_emitir(g, IR_LABEL, NULL, NULL, NULL, NULL, NULL, teste, 0);
    if (no->b) {
        IrgValor cond = irg_expr(g, no->b);
        irg_emitir(g, IR_IFF, NULL, cond.op, NULL, NULL, NULL, fim, 0);
    }
    /* Pelo índice, e não por ponteiro: laços aninhados podem realocar o vetor. */
    size_t indice = g->nlacos;
    IrgLaco laco = {fim, NULL};
    IRG_PUSH(g->lacos, g->nlacos, g->caplacos, laco);
    irg_comando(g, no->d);
    const char *continuacao = g->lacos[indice].continuacao;
    g->nlacos--;
    if (continuacao) {
        irg_emitir(g, IR_LABEL, NULL, NULL, NULL, NULL, NULL, continuacao, 0);
    }
    if (no->c) {
        irg_efeito(g, no->c);
    }
    irg_emitir(g, IR_GOTO, NULL, NULL, NULL, NULL, NULL, teste, 0);
    irg_emitir(g, IR_LABEL, NULL, NULL, NULL, NULL, NULL, fim, 0);
}

static void irg_comando(IrGerador *g, const Ast *no) {
    switch (no->kind) {
    case AST_BLOCK: {
        IrgEscopo vazio = {NULL, 0, 0};
        IRG_PUSH(g->escopos, g->nescopos, g->capescopos, vazio);
        for (size_t i = 0; i < no->items_len; i++) {
            irg_item(g, no->items[i]);
        }
        g->nescopos--;
        free(g->escopos[g->nescopos].itens);
        break;
    }
    case AST_EXPR_STMT:
        if (no->a) {
            irg_efeito(g, no->a);
        }
        break;
    case AST_IF:
        irg_if(g, no);
        break;
    case AST_WHILE: {
        const char *teste = irg_rotulo(g, "L_test");
        const char *fim = irg_rotulo(g, "L_end");
        irg_emitir(g, IR_LABEL, NULL, NULL, NULL, NULL, NULL, teste, 0);
        IrgValor cond = irg_expr(g, no->a);
        irg_emitir(g, IR_IFF, NULL, cond.op, NULL, NULL, NULL, fim, 0);
        IrgLaco laco = {fim, teste};
        IRG_PUSH(g->lacos, g->nlacos, g->caplacos, laco);
        irg_comando(g, no->b);
        g->nlacos--;
        irg_emitir(g, IR_GOTO, NULL, NULL, NULL, NULL, NULL, teste, 0);
        irg_emitir(g, IR_LABEL, NULL, NULL, NULL, NULL, NULL, fim, 0);
        break;
    }
    case AST_FOR:
        irg_for(g, no);
        break;
    case AST_RETURN:
        if (!no->a) {
            irg_emitir(g, IR_RET, NULL, NULL, NULL, NULL, NULL, NULL, 0);
        } else {
            IrgValor v = irg_expr(g, no->a);
            const char *operando = irg_converter(g, v.op, v.tipo, g->tipo_retorno);
            irg_emitir(g, IR_RET, NULL, operando, NULL, NULL, NULL, NULL, 0);
        }
        break;
    case AST_BREAK:
        irg_emitir(g, IR_GOTO, NULL, NULL, NULL, NULL, NULL, g->lacos[g->nlacos - 1].saida, 0);
        break;
    case AST_CONTINUE: {
        IrgLaco *laco = &g->lacos[g->nlacos - 1];
        if (!laco->continuacao) {
            laco->continuacao = irg_rotulo(g, "L_step");
        }
        irg_emitir(g, IR_GOTO, NULL, NULL, NULL, NULL, NULL, laco->continuacao, 0);
        break;
    }
    case AST_PRINT: {
        IrgValor v = irg_expr(g, no->a);
        irg_emitir(g, IR_PRINT, NULL, v.op, NULL, NULL, NULL, NULL, 0);
        break;
    }
    case AST_READ:
        if (no->a->kind == AST_INDEX) {
            const char *endereco = irg_endereco(g, no->a->b);
            const char *t = irg_temp(g);
            irg_emitir(g, IR_READ, t, NULL, NULL, NULL, NULL, NULL, 0);
            const IrgVariavel *var = irg_buscar(g, no->a->a->text);
            const char *vetor = var->nome_ir;
            irg_registrar_tipo(g, t, var->tipo); /* o back-end precisa saber o que ler */
            irg_emitir(g, IR_STORE, NULL, vetor, NULL, endereco, t, NULL, 0);
        } else {
            const char *nome = irg_buscar(g, no->a->text)->nome_ir;
            irg_emitir(g, IR_READ, nome, NULL, NULL, NULL, NULL, NULL, 0);
        }
        break;
    default:
        fprintf(stderr, "erro interno: comando desconhecido na geração da IR\n");
        exit(5);
    }
}

/* -- expressões ------------------------------------------------------ */

static const Ast *irg_funcao_declarada(const IrGerador *g, const char *nome) {
    const Ast *achada = NULL;
    for (size_t i = 0; i < g->programa->items_len; i++) {
        const Ast *item = g->programa->items[i];
        if (item->kind == AST_FUNCTION && strcmp(item->text, nome) == 0) {
            achada = item; /* como o dict do Python: a última definição vale */
        }
    }
    return achada;
}

static IrgValor irg_chamada(IrGerador *g, const Ast *no, int com_valor) {
    const Ast *funcao = irg_funcao_declarada(g, no->a->text);
    size_t n = no->items_len < funcao->items_len ? no->items_len : funcao->items_len;
    const char **operandos = irg_realloc(NULL, (n ? n : 1) * sizeof *operandos);
    for (size_t i = 0; i < n; i++) {
        const Ast *param = funcao->items[i];
        IrgValor v = irg_expr(g, no->items[i]);
        operandos[i] = param->is_array ? v.op : irg_converter(g, v.op, v.tipo, param->type_name);
    }
    for (size_t i = 0; i < n; i++) {
        irg_emitir(g, IR_PARAM, NULL, operandos[i], NULL, NULL, NULL, NULL, 0);
    }
    free(operandos);
    const char *nome = ir_intern(g->p, no->a->text);
    IrgValor resultado = {NULL, funcao->type_name};
    if (!com_valor) {
        irg_emitir(g, IR_CALL, NULL, nome, NULL, NULL, NULL, NULL, (int)n);
        return resultado;
    }
    resultado.op = irg_temp(g);
    irg_emitir(g, IR_CALL, resultado.op, nome, NULL, NULL, NULL, NULL, (int)n);
    return resultado;
}

static IrgValor irg_curto_circuito(IrGerador *g, const Ast *no) {
    IrgValor esquerda = irg_expr(g, no->a);
    const char *t = irg_temp(g);
    const char *fim = irg_rotulo(g, "L_sc");
    irg_emitir(g, IR_COPY, t, esquerda.op, NULL, NULL, NULL, NULL, 0);
    IrKind desvio = strcmp(no->text, "&&") == 0 ? IR_IFF : IR_IFT;
    irg_emitir(g, desvio, NULL, t, NULL, NULL, NULL, fim, 0);
    IrgValor direita = irg_expr(g, no->b);
    irg_emitir(g, IR_COPY, t, direita.op, NULL, NULL, NULL, NULL, 0);
    irg_emitir(g, IR_LABEL, NULL, NULL, NULL, NULL, NULL, fim, 0);
    IrgValor resultado = {t, "bool"};
    return resultado;
}

static int irg_relacional(const char *op) {
    return strcmp(op, "<") == 0 || strcmp(op, "<=") == 0 || strcmp(op, ">") == 0 ||
           strcmp(op, ">=") == 0 || strcmp(op, "==") == 0 || strcmp(op, "!=") == 0;
}

static IrgValor irg_binario(IrGerador *g, const Ast *no) {
    const char *op = no->text;
    int logico = strcmp(op, "&&") == 0 || strcmp(op, "||") == 0;
    if (logico && irg_contem_chamada(no->b)) {
        return irg_curto_circuito(g, no);
    }

    IrgValor esquerda = irg_expr(g, no->a);
    IrgValor direita = irg_expr(g, no->b);
    IrgValor resultado;
    if (logico) {
        resultado.op = irg_temp(g);
        resultado.tipo = "bool";
        const char *op_ir = ir_intern(g->p, strcmp(op, "&&") == 0 ? "and" : "or");
        irg_emitir(g, IR_BIN, resultado.op, esquerda.op, op_ir, direita.op, NULL, NULL, 0);
        return resultado;
    }

    const char *comum;
    if ((strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) && strcmp(esquerda.tipo, "bool") == 0) {
        comum = "bool";
    } else if (strcmp(op, "%") == 0) {
        comum = "int";
    } else if (strcmp(esquerda.tipo, "float") == 0 || strcmp(direita.tipo, "float") == 0) {
        comum = "float";
    } else {
        comum = "int";
    }
    const char *a = irg_converter(g, esquerda.op, esquerda.tipo, comum);
    const char *b = irg_converter(g, direita.op, direita.tipo, comum);
    resultado.op = irg_temp(g);
    irg_emitir(g, IR_BIN, resultado.op, a, ir_intern(g->p, op), b, NULL, NULL, 0);
    resultado.tipo = irg_relacional(op) ? "bool" : comum;
    return resultado;
}

static IrgValor irg_atribuicao(IrGerador *g, const Ast *no) {
    IrgValor resultado;
    if (no->a->kind == AST_INDEX) {
        const char *endereco = irg_endereco(g, no->a->b);
        const IrgVariavel *var = irg_buscar(g, no->a->a->text);
        IrgValor v = irg_expr(g, no->b);
        const char *operando = irg_converter(g, v.op, v.tipo, var->tipo);
        irg_emitir(g, IR_STORE, NULL, var->nome_ir, NULL, endereco, operando, NULL, 0);
        resultado.op = operando;
        resultado.tipo = var->tipo;
        return resultado;
    }
    const IrgVariavel *var = irg_buscar(g, no->a->text);
    const char *nome_ir = var->nome_ir;
    const char *tipo = var->tipo;
    irg_atribuir_variavel(g, nome_ir, tipo, no->b);
    resultado.op = nome_ir;
    resultado.tipo = tipo;
    return resultado;
}

static IrgValor irg_expr(IrGerador *g, const Ast *no) {
    IrgValor resultado = {NULL, NULL};
    switch (no->kind) {
    case AST_LIT:
        /* O parser já guarda char e cadeia com aspas e escapes ('\n'). */
        resultado.op = ir_intern(g->p, no->text);
        resultado.tipo = irg_tipo_literal(no->type_name);
        return resultado;
    case AST_ID: {
        const IrgVariavel *var = irg_buscar(g, no->text);
        resultado.op = var->nome_ir;
        resultado.tipo = var->tipo;
        return resultado;
    }
    case AST_INDEX: {
        const char *endereco = irg_endereco(g, no->b);
        const IrgVariavel *var = irg_buscar(g, no->a->text);
        resultado.op = irg_temp(g);
        resultado.tipo = var->tipo;
        irg_emitir(g, IR_LOAD, resultado.op, var->nome_ir, NULL, endereco, NULL, NULL, 0);
        return resultado;
    }
    case AST_CALL:
        return irg_chamada(g, no, 1);
    case AST_UNARY: {
        IrgValor v = irg_expr(g, no->a);
        if (strcmp(no->text, "!") == 0) {
            resultado.op = irg_temp(g);
            resultado.tipo = "bool";
            irg_emitir(g, IR_UN, resultado.op, v.op, ir_intern(g->p, "not"), NULL, NULL, NULL,
                       0);
            return resultado;
        }
        /* "-": char é promovido a int antes */
        if (strcmp(v.tipo, "char") == 0) {
            v.op = irg_converter(g, v.op, "char", "int");
            v.tipo = "int";
        }
        resultado.op = irg_temp(g);
        resultado.tipo = v.tipo;
        irg_emitir(g, IR_UN, resultado.op, v.op, ir_intern(g->p, "neg"), NULL, NULL, NULL, 0);
        return resultado;
    }
    case AST_BINARY:
        return irg_binario(g, no);
    case AST_ASSIGN:
        return irg_atribuicao(g, no);
    default:
        fprintf(stderr, "erro interno: expressão desconhecida na geração da IR\n");
        exit(5);
    }
}

/* -- programa -------------------------------------------------------- */

static void irg_funcao(IrGerador *g, const Ast *no) {
    const char **declaracoes = NULL;
    size_t ndecl = 0, capdecl = 0;
    for (size_t i = 0; i < no->items_len; i++) {
        IRG_PUSH(declaracoes, ndecl, capdecl, (const char *)no->items[i]->text);
    }
    irg_coletar_declaracoes(no->a, &declaracoes, &ndecl, &capdecl);
    irg_entrar(g, no->text, no->type_name, declaracoes, ndecl);
    free(declaracoes);

    for (size_t i = 0; i < no->items_len; i++) {
        const Ast *p = no->items[i];
        const char *nome_ir = irg_declarar(g, p, "parâmetro", NULL, 1);
        IRG_PUSH(g->funcao.params, g->funcao.nparams, g->funcao.capparams, nome_ir);
    }
    /* O corpo divide o escopo com os parâmetros (como na análise semântica). */
    for (size_t i = 0; i < no->a->items_len; i++) {
        irg_item(g, no->a->items[i]);
    }
    irg_terminar_funcao(g);
}

static const char *irg_nome_global(IrPrograma *p, const char *nome) {
    return ir_eh_temporario(nome) ? irg_intern_livre(p, irg_format("%s#g", nome))
                                  : ir_intern(p, nome);
}

void ir_gerar(const Ast *programa, IrPrograma *saida) {
    IrGerador g;
    memset(&g, 0, sizeof g);
    g.p = saida;
    g.programa = programa;

    for (size_t i = 0; i < programa->items_len; i++) {
        const Ast *item = programa->items[i];
        if (item->kind == AST_FUNCTION) {
            IrSimbolo s = {ir_intern(saida, item->text), ir_intern(saida, item->text),
                           ir_intern(saida, "função"), ir_intern(saida, item->type_name),
                           NULL, 0, item->line, item->column, 1};
            IRG_PUSH(saida->simbolos, saida->nsimbolos, saida->capsimbolos, s);
        } else if (item->kind == AST_VAR_DECL) {
            IrSimbolo s = {ir_intern(saida, item->text), irg_nome_global(saida, item->text),
                           ir_intern(saida, item->b ? "vetor" : "variável"),
                           ir_intern(saida, item->type_name),
                           item->b ? ir_intern(saida, item->b->text) : NULL, 0, item->line,
                           item->column, item->a != NULL};
            IRG_PUSH(saida->simbolos, saida->nsimbolos, saida->capsimbolos, s);
        }
    }

    irg_entrar(&g, "@init", "void", NULL, 0);
    for (size_t i = 0; i < programa->items_len; i++) {
        const Ast *item = programa->items[i];
        if (item->kind == AST_VAR_DECL) {
            const char *nome = irg_nome_global(saida, item->text);
            IrGlobal global = {nome, item->b ? ir_intern(saida, item->b->text) : NULL,
                               ir_intern(saida, item->type_name)};
            IRG_PUSH(saida->globais, saida->nglobais, saida->capglobais, global);
            IrgVariavel var = {item->text, nome, item->type_name, item->b != NULL};
            IRG_PUSH(g.globais, g.nglobais, g.capglobais, var);
            if (item->a) {
                irg_atribuir_variavel(&g, nome, item->type_name, item->a);
            }
        } else if (item->kind != AST_FUNCTION) {
            irg_comando(&g, item); /* comando solto no topo */
        }
    }
    if (g.funcao.len > 0) {
        irg_terminar_funcao(&g);
    } else {
        free(g.funcao.instrs);
        free(g.funcao.simbolos);
        free(g.funcao.tipos);
    }

    for (size_t i = 0; i < programa->items_len; i++) {
        if (programa->items[i]->kind == AST_FUNCTION) {
            irg_funcao(&g, programa->items[i]);
        }
    }

    for (size_t i = 0; i < g.nescopos; i++) {
        free(g.escopos[i].itens);
    }
    free(g.escopos);
    free(g.globais);
    free(g.rotulos);
    free(g.sufixos);
    free(g.sufixar);
    free(g.lacos);
}
