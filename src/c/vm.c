/* Máquina virtual de pilha do MINIC — ver vm.h e src/python/vm.py. */

#include "vm.h"

#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VM_PROFUNDIDADE_MAXIMA 10000
#define VM_INT_MIN (-2147483647LL - 1)

static void *vm_realloc(void *ptr, size_t tamanho) {
    void *novo = realloc(ptr, tamanho);
    if (!novo) {
        fprintf(stderr, "erro: memória insuficiente\n");
        exit(1);
    }
    return novo;
}

#define VM_PUSH(arr, len, cap, item)                                     \
    do {                                                                 \
        if ((len) == (cap)) {                                            \
            (cap) = (cap) ? (cap) * 2 : 16;                              \
            (arr) = vm_realloc((arr), (cap) * sizeof *(arr));            \
        }                                                                \
        (arr)[(len)++] = (item);                                         \
    } while (0)

static char *vm_format(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    char *s = vm_realloc(NULL, (size_t)n + 1);
    va_start(args, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, args);
    va_end(args);
    return s;
}

/* ------------------------------------------------------------------ *
 * Valores
 * ------------------------------------------------------------------ */

typedef enum { V_INT, V_FLOAT, V_BOOL, V_STR, V_VETOR } VmTag;

typedef struct VmVetor VmVetor;

typedef struct {
    VmTag tag;
    long long i; /* V_INT (32 bits) e V_BOOL */
    double f;
    const char *s; /* V_STR: bytes (char ou cadeia) */
    size_t slen;
    VmVetor *v;
} Valor;

struct VmVetor {
    char *nome; /* como no código (sem o sufixo #...) */
    size_t tamanho;
    Valor *itens;
};

static long long vm_int32(long long v) {
    long long m = v % 4294967296LL;
    if (m < 0) {
        m += 4294967296LL;
    }
    return m >= 2147483648LL ? m - 4294967296LL : m;
}

static Valor vm_int(long long i) {
    Valor v = {V_INT, i, 0, NULL, 0, NULL};
    return v;
}

static Valor vm_float(double f) {
    Valor v = {V_FLOAT, 0, f, NULL, 0, NULL};
    return v;
}

static Valor vm_bool(int b) {
    Valor v = {V_BOOL, b != 0, 0, NULL, 0, NULL};
    return v;
}

static Valor vm_str(const char *s, size_t len) {
    Valor v = {V_STR, 0, 0, s, len, NULL};
    return v;
}

static Valor vm_inicial(const char *tipo) {
    if (tipo && strcmp(tipo, "float") == 0) {
        return vm_float(0.0);
    }
    if (tipo && strcmp(tipo, "bool") == 0) {
        return vm_bool(0);
    }
    if (tipo && strcmp(tipo, "char") == 0) {
        return vm_str("\0", 1);
    }
    return vm_int(0);
}

static VmVetor *vm_vetor_novo(const char *nome, const char *tipo, size_t tamanho) {
    VmVetor *v = vm_realloc(NULL, sizeof *v);
    size_t n = strcspn(nome, "#");
    v->nome = vm_realloc(NULL, n + 1);
    memcpy(v->nome, nome, n);
    v->nome[n] = '\0';
    v->tamanho = tamanho;
    v->itens = vm_realloc(NULL, (tamanho ? tamanho : 1) * sizeof *v->itens);
    for (size_t i = 0; i < tamanho; i++) {
        v->itens[i] = vm_inicial(tipo);
    }
    return v;
}

static void vm_vetor_free(VmVetor *v) {
    free(v->nome);
    free(v->itens);
    free(v);
}

/* Código do primeiro caractere (UTF-8), como o ord() do Python sobre uma string
 * lida com surrogateescape: um byte inválido b vira 0xDC00 + b. Devolve em
 * *tam quantos bytes o caractere ocupa. */
static long vm_codigo(const unsigned char *s, size_t len, size_t *tam) {
    unsigned char c = s[0];
    size_t n;
    long cp, minimo;
    if (c < 0x80) {
        *tam = 1;
        return c;
    } else if ((c & 0xE0) == 0xC0) {
        n = 2, cp = c & 0x1F, minimo = 0x80;
    } else if ((c & 0xF0) == 0xE0) {
        n = 3, cp = c & 0x0F, minimo = 0x800;
    } else if ((c & 0xF8) == 0xF0) {
        n = 4, cp = c & 0x07, minimo = 0x10000;
    } else {
        *tam = 1;
        return 0xDC00 + c;
    }
    if (len < n) {
        *tam = 1;
        return 0xDC00 + c;
    }
    for (size_t k = 1; k < n; k++) {
        if ((s[k] & 0xC0) != 0x80) {
            *tam = 1;
            return 0xDC00 + c;
        }
        cp = (cp << 6) | (s[k] & 0x3F);
    }
    if (cp < minimo || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        *tam = 1;
        return 0xDC00 + c;
    }
    *tam = n;
    return cp;
}

/* Literal da IR (aspas e escapes já no texto) -> valor. */
static char *vm_decodificar(const char *corpo, size_t len, size_t *saida_len) {
    char *s = vm_realloc(NULL, len + 1);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (corpo[i] == '\\' && i + 1 < len) {
            char c = corpo[++i];
            switch (c) {
            case 'n': s[j++] = '\n'; break;
            case 't': s[j++] = '\t'; break;
            case 'r': s[j++] = '\r'; break;
            default: s[j++] = c; break;
            }
        } else {
            s[j++] = corpo[i];
        }
    }
    s[j] = '\0';
    *saida_len = j;
    return s;
}

/* ------------------------------------------------------------------ *
 * Saída e formato do float
 * ------------------------------------------------------------------ */

/* "%.*g" com expoente de pelo menos dois dígitos (o msvcrt do Windows usa três). */
static void vm_g(char *buf, size_t tam, int precisao, double x) {
    snprintf(buf, tam, "%.*g", precisao, x);
    char *e = strchr(buf, 'e');
    if (!e) {
        return;
    }
    char *digitos = e + 2; /* depois de "e+" ou "e-" */
    while (digitos[0] == '0' && strlen(digitos) > 2) {
        memmove(digitos, digitos + 1, strlen(digitos));
    }
}

static void vm_formatar_float(char *buf, size_t tam, double x) {
    if (isnan(x)) {
        snprintf(buf, tam, "nan");
        return;
    }
    if (isinf(x)) {
        snprintf(buf, tam, x > 0 ? "inf" : "-inf");
        return;
    }
    for (int p = 1; p <= 17; p++) {
        vm_g(buf, tam, p, x);
        if (strtod(buf, NULL) == x) {
            break;
        }
    }
    if (!strchr(buf, '.') && !strchr(buf, 'e')) {
        strcat(buf, ".0");
    }
}

static void vm_imprimir(Valor v) {
    char buf[64];
    switch (v.tag) {
    case V_BOOL:
        fputs(v.i ? "true" : "false", stdout);
        break;
    case V_INT:
        printf("%ld", (long)v.i);
        break;
    case V_FLOAT:
        vm_formatar_float(buf, sizeof buf, v.f);
        fputs(buf, stdout);
        break;
    case V_STR:
        fwrite(v.s, 1, v.slen, stdout);
        break;
    case V_VETOR:
        break;
    }
    fputc('\n', stdout);
}

/* ------------------------------------------------------------------ *
 * Programa decodificado
 * ------------------------------------------------------------------ */

enum {
    OP_PUSH, OP_LOAD, OP_STORE, OP_GLOAD, OP_GSTORE, OP_ALOAD, OP_ASTORE, OP_GALOAD, OP_GASTORE,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEG, OP_LT, OP_LE, OP_GT, OP_GE, OP_EQ, OP_NE,
    OP_AND, OP_OR, OP_NOT, OP_ITOF, OP_CTOI, OP_JMP, OP_JZ, OP_JNZ, OP_CALL, OP_RET, OP_POP,
    OP_PRINT, OP_READ
};

static const char *VM_NOMES[] = {
    "PUSH", "LOAD", "STORE", "GLOAD", "GSTORE", "ALOAD", "ASTORE", "GALOAD", "GASTORE",
    "ADD", "SUB", "MUL", "DIV", "MOD", "NEG", "LT", "LE", "GT", "GE", "EQ", "NE",
    "AND", "OR", "NOT", "ITOF", "CTOI", "JMP", "JZ", "JNZ", "CALL", "RET", "POP",
    "PRINT", "READ",
};

typedef struct {
    int op;
    long arg;        /* slot, alvo do desvio, índice da global ou da função */
    int n;           /* CALL: argumentos */
    Valor literal;   /* PUSH */
    const char *tipo; /* READ */
} VmOp;

typedef struct {
    const VmFuncao *f;
    VmOp *codigo;
    size_t len;
} VmCodigo;

typedef struct {
    size_t funcao;
    Valor *slots;
    size_t pc;
} VmQuadro;

typedef struct {
    const VmPrograma *p;
    VmCodigo *funcoes;
    size_t nfuncoes;
    Valor *globais;
    char **literais; /* strings decodificadas dos PUSH, liberadas no fim */
    size_t nliterais, capliterais;
    /* entrada */
    char *entrada;
    size_t entrada_len, entrada_pos;
    int entrada_lida;
    char **lidos; /* chars lidos por READ, liberados no fim */
    size_t nlidos, caplidos;
    /* execução: guardada aqui, e não em variáveis locais, para que um erro
     * (longjmp) possa liberar tudo num lugar só */
    Valor *pilha;
    size_t npilha, cappilha;
    VmQuadro *quadros; /* chamadores */
    size_t nquadros, capquadros;
    size_t funcao; /* função e quadro atuais */
    Valor *slots;
    /* erro */
    jmp_buf falha;
    char *erro;
} Vm;

static void vm_erro(Vm *vm, const char *funcao, char *mensagem) {
    vm->erro = vm_format("Erro de execução em %s: %s", funcao, mensagem);
    free(mensagem);
    longjmp(vm->falha, 1);
}

static long vm_indice_funcao(const Vm *vm, const char *nome) {
    for (size_t i = 0; i < vm->p->nfuncoes; i++) {
        if (strcmp(vm->p->funcoes[i].nome, nome) == 0) {
            return (long)i;
        }
    }
    return -1;
}

static long vm_indice_global(const Vm *vm, const char *nome) {
    for (size_t i = 0; i < vm->p->nglobais; i++) {
        if (strcmp(vm->p->globais[i].nome, nome) == 0) {
            return (long)i;
        }
    }
    return -1;
}

static Valor vm_literal(Vm *vm, const char *texto) {
    if (texto[0] == '\'' || texto[0] == '"') {
        size_t len;
        char *s = vm_decodificar(texto + 1, strlen(texto) - 2, &len);
        VM_PUSH(vm->literais, vm->nliterais, vm->capliterais, s);
        return vm_str(s, len);
    }
    if (strcmp(texto, "true") == 0) {
        return vm_bool(1);
    }
    if (strcmp(texto, "false") == 0) {
        return vm_bool(0);
    }
    if (strchr(texto, '.')) {
        return vm_float(strtod(texto, NULL));
    }
    /* Inteiro com transbordamento em 32 bits, dígito a dígito (como o int()
     * do Python seguido do corte em 32 bits, mesmo para literais enormes). */
    int negativo = texto[0] == '-';
    unsigned long long v = 0;
    for (const char *c = texto + negativo; *c; c++) {
        v = (v * 10 + (unsigned long long)(*c - '0')) % 4294967296ULL;
    }
    long long r = (long long)v;
    return vm_int(vm_int32(negativo ? -r : r));
}

static void vm_decodificar_programa(Vm *vm) {
    vm->nfuncoes = vm->p->nfuncoes;
    vm->funcoes = vm_realloc(NULL, (vm->nfuncoes ? vm->nfuncoes : 1) * sizeof *vm->funcoes);
    for (size_t fi = 0; fi < vm->p->nfuncoes; fi++) {
        const VmFuncao *f = &vm->p->funcoes[fi];
        VmCodigo *c = &vm->funcoes[fi];
        c->f = f;
        c->codigo = vm_realloc(NULL, (f->len ? f->len : 1) * sizeof *c->codigo);
        c->len = 0;
        /* rótulos: posição na lista sem os LABEL */
        const char **rotulos = vm_realloc(NULL, (f->len ? f->len : 1) * sizeof *rotulos);
        size_t *posicoes = vm_realloc(NULL, (f->len ? f->len : 1) * sizeof *posicoes);
        size_t nrot = 0, pos = 0;
        for (size_t i = 0; i < f->len; i++) {
            if (strcmp(f->codigo[i].op, "LABEL") == 0) {
                rotulos[nrot] = f->codigo[i].arg;
                posicoes[nrot++] = pos;
            } else {
                pos++;
            }
        }
        for (size_t i = 0; i < f->len; i++) {
            const VmInstr *ins = &f->codigo[i];
            if (strcmp(ins->op, "LABEL") == 0) {
                continue;
            }
            VmOp op;
            memset(&op, 0, sizeof op);
            for (int k = 0; k < (int)(sizeof VM_NOMES / sizeof VM_NOMES[0]); k++) {
                if (strcmp(VM_NOMES[k], ins->op) == 0) {
                    op.op = k;
                }
            }
            switch (op.op) {
            case OP_PUSH:
                op.literal = vm_literal(vm, ins->arg);
                break;
            case OP_LOAD:
            case OP_STORE:
            case OP_ALOAD:
            case OP_ASTORE:
                op.arg = strtol(ins->arg, NULL, 10);
                break;
            case OP_GLOAD:
            case OP_GSTORE:
            case OP_GALOAD:
            case OP_GASTORE:
                op.arg = vm_indice_global(vm, ins->arg);
                break;
            case OP_JMP:
            case OP_JZ:
            case OP_JNZ:
                for (size_t r = 0; r < nrot; r++) {
                    if (strcmp(rotulos[r], ins->arg) == 0) {
                        op.arg = (long)posicoes[r];
                    }
                }
                break;
            case OP_CALL:
                op.arg = vm_indice_funcao(vm, ins->arg);
                op.n = ins->n;
                break;
            case OP_READ:
                op.tipo = ins->arg;
                break;
            default:
                break;
            }
            c->codigo[c->len++] = op;
        }
        free(rotulos);
        free(posicoes);
    }
}

/* ------------------------------------------------------------------ *
 * Entrada (read)
 * ------------------------------------------------------------------ */

static int vm_espaco(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static Valor vm_ler(Vm *vm, const char *funcao, const char *tipo) {
    if (!vm->entrada_lida) {
        size_t cap = 4096;
        vm->entrada = vm_realloc(NULL, cap);
        size_t n;
        while ((n = fread(vm->entrada + vm->entrada_len, 1, cap - vm->entrada_len, stdin)) > 0) {
            vm->entrada_len += n;
            if (vm->entrada_len == cap) {
                cap *= 2;
                vm->entrada = vm_realloc(vm->entrada, cap);
            }
        }
        vm->entrada_lida = 1;
    }
    while (vm->entrada_pos < vm->entrada_len && vm_espaco(vm->entrada[vm->entrada_pos])) {
        vm->entrada_pos++;
    }
    if (vm->entrada_pos >= vm->entrada_len) {
        vm_erro(vm, funcao, vm_format("entrada esgotada: read de %s sem valor.", tipo));
    }
    const char *item = vm->entrada + vm->entrada_pos;
    size_t len = 0;
    while (vm->entrada_pos + len < vm->entrada_len && !vm_espaco(item[len])) {
        len++;
    }
    vm->entrada_pos += len;

    size_t k = item[0] == '-' ? 1 : 0;
    size_t digitos = 0;
    while (k + digitos < len && item[k + digitos] >= '0' && item[k + digitos] <= '9') {
        digitos++;
    }
    if (strcmp(tipo, "int") == 0 && digitos > 0 && k + digitos == len) {
        unsigned long long v = 0;
        for (size_t i = k; i < len; i++) {
            v = v * 10 + (unsigned long long)(item[i] - '0');
            if (v > 4294967296ULL) {
                v = 4294967296ULL; /* já fora do intervalo; evita estourar */
            }
        }
        long long valor = k ? -(long long)v : (long long)v;
        if (valor >= VM_INT_MIN && valor <= 2147483647LL) {
            return vm_int(valor);
        }
    } else if (strcmp(tipo, "float") == 0 && digitos > 0) {
        size_t fim = k + digitos;
        if (fim < len && item[fim] == '.') {
            size_t decimais = 0;
            while (fim + 1 + decimais < len && item[fim + 1 + decimais] >= '0' &&
                   item[fim + 1 + decimais] <= '9') {
                decimais++;
            }
            fim = decimais > 0 ? fim + 1 + decimais : len + 1;
        }
        if (fim == len) {
            char *copia = vm_format("%.*s", (int)len, item);
            double f = strtod(copia, NULL);
            free(copia);
            return vm_float(f);
        }
    } else if (strcmp(tipo, "bool") == 0 &&
               ((len == 4 && memcmp(item, "true", 4) == 0) ||
                (len == 5 && memcmp(item, "false", 5) == 0))) {
        return vm_bool(len == 4);
    } else if (strcmp(tipo, "char") == 0) {
        size_t tam;
        vm_codigo((const unsigned char *)item, len, &tam);
        if (tam == len) {
            char *s = vm_format("%.*s", (int)len, item);
            VM_PUSH(vm->lidos, vm->nlidos, vm->caplidos, s);
            return vm_str(s, len);
        }
    }
    vm_erro(vm, funcao, vm_format("entrada “%.*s” inválida para read de %s.", (int)len, item, tipo));
    return vm_int(0);
}

/* ------------------------------------------------------------------ *
 * Execução
 * ------------------------------------------------------------------ */

static Valor *vm_quadro_novo(const VmFuncao *f) {
    Valor *slots = vm_realloc(NULL, (f->nslots ? f->nslots : 1) * sizeof *slots);
    for (size_t i = 0; i < f->nslots; i++) {
        const VmSlot *s = &f->slots[i];
        if (s->tamanho >= 0) {
            Valor v = {V_VETOR, 0, 0, NULL, 0, vm_vetor_novo(s->nome, s->tipo, (size_t)s->tamanho)};
            slots[i] = v;
        } else {
            slots[i] = vm_inicial(s->tipo);
        }
    }
    return slots;
}

static void vm_quadro_free(const VmFuncao *f, Valor *slots) {
    for (size_t i = 0; i < f->nslots; i++) {
        if (f->slots[i].tamanho >= 0) {
            vm_vetor_free(slots[i].v);
        }
    }
    free(slots);
}

static int vm_verdade(Valor v) { return v.tag == V_FLOAT ? v.f != 0.0 : v.i != 0; }

static Valor vm_aritmetica(Vm *vm, const char *funcao, int op, Valor a, Valor b) {
    if (a.tag == V_FLOAT || b.tag == V_FLOAT) {
        double x = a.tag == V_FLOAT ? a.f : (double)a.i;
        double y = b.tag == V_FLOAT ? b.f : (double)b.i;
        if (op == OP_ADD) return vm_float(x + y);
        if (op == OP_SUB) return vm_float(x - y);
        if (op == OP_MUL) return vm_float(x * y);
        if (y == 0.0) {
            vm_erro(vm, funcao, vm_format("divisão por zero."));
        }
        return vm_float(x / y);
    }
    long long x = a.i, y = b.i;
    if (op == OP_ADD) return vm_int(vm_int32(x + y));
    if (op == OP_SUB) return vm_int(vm_int32(x - y));
    if (op == OP_MUL) return vm_int(vm_int32(x * y));
    if (y == 0) {
        vm_erro(vm, funcao,
                vm_format(op == OP_DIV ? "divisão por zero." : "resto de divisão por zero."));
    }
    long long q = llabs(x) / llabs(y);
    if ((x < 0) != (y < 0)) {
        q = -q;
    }
    return vm_int(op == OP_DIV ? vm_int32(q) : vm_int32(x - q * y));
}

static int vm_comparar(int op, Valor a, Valor b) {
    if (a.tag == V_FLOAT || b.tag == V_FLOAT) {
        double x = a.tag == V_FLOAT ? a.f : (double)a.i;
        double y = b.tag == V_FLOAT ? b.f : (double)b.i;
        switch (op) {
        case OP_LT: return x < y;
        case OP_LE: return x <= y;
        case OP_GT: return x > y;
        case OP_GE: return x >= y;
        case OP_EQ: return x == y;
        default: return x != y;
        }
    }
    if (a.tag == V_STR && b.tag == V_STR) {
        int igual = a.slen == b.slen && memcmp(a.s, b.s, a.slen) == 0;
        return op == OP_EQ ? igual : !igual;
    }
    switch (op) {
    case OP_LT: return a.i < b.i;
    case OP_LE: return a.i <= b.i;
    case OP_GT: return a.i > b.i;
    case OP_GE: return a.i >= b.i;
    case OP_EQ: return a.i == b.i;
    default: return a.i != b.i;
    }
}

static VmVetor *vm_conferir(Vm *vm, const char *funcao, Valor vetor, Valor indice) {
    VmVetor *v = vetor.v;
    if (indice.i < 0 || (unsigned long long)indice.i >= v->tamanho) {
        vm_erro(vm, funcao,
                vm_format("índice %lld fora dos limites do vetor “%s” (tamanho %lu).", indice.i,
                          v->nome, (unsigned long)v->tamanho));
    }
    return v;
}

/* Libera a pilha e todos os quadros (no fim normal ou depois de um erro). */
static void vm_liberar_execucao(Vm *vm) {
    for (size_t q = 0; q < vm->nquadros; q++) {
        vm_quadro_free(vm->funcoes[vm->quadros[q].funcao].f, vm->quadros[q].slots);
    }
    if (vm->slots) {
        vm_quadro_free(vm->funcoes[vm->funcao].f, vm->slots);
    }
    free(vm->pilha);
    free(vm->quadros);
    vm->pilha = NULL;
    vm->quadros = NULL;
    vm->slots = NULL;
    vm->npilha = vm->cappilha = vm->nquadros = vm->capquadros = 0;
}

#define EMPILHAR(valor) VM_PUSH(vm->pilha, vm->npilha, vm->cappilha, (valor))
#define DESEMPILHAR() (vm->pilha[--vm->npilha])
#define TOPO (vm->pilha[vm->npilha - 1])

/* Roda a função `inicio` até ela retornar. */
static void vm_rodar(Vm *vm, size_t inicio, long long *executadas) {
    vm->funcao = inicio;
    const VmCodigo *codigo = &vm->funcoes[inicio];
    vm->slots = vm_quadro_novo(codigo->f);
    size_t pc = 0;

    for (;;) {
        const VmOp *ins = &codigo->codigo[pc++];
        (*executadas)++;
        const char *nome = codigo->f->nome;
        Valor *slots = vm->slots;
        switch (ins->op) {
        case OP_PUSH:
            EMPILHAR(ins->literal);
            break;
        case OP_LOAD:
            EMPILHAR(slots[ins->arg]);
            break;
        case OP_STORE:
            slots[ins->arg] = DESEMPILHAR();
            break;
        case OP_GLOAD:
            EMPILHAR(vm->globais[ins->arg]);
            break;
        case OP_GSTORE:
            vm->globais[ins->arg] = DESEMPILHAR();
            break;
        case OP_ALOAD:
        case OP_GALOAD: {
            Valor vetor = ins->op == OP_ALOAD ? slots[ins->arg] : vm->globais[ins->arg];
            Valor indice = DESEMPILHAR();
            VmVetor *v = vm_conferir(vm, nome, vetor, indice);
            EMPILHAR(v->itens[indice.i]);
            break;
        }
        case OP_ASTORE:
        case OP_GASTORE: {
            Valor vetor = ins->op == OP_ASTORE ? slots[ins->arg] : vm->globais[ins->arg];
            Valor valor = DESEMPILHAR();
            Valor indice = DESEMPILHAR();
            VmVetor *v = vm_conferir(vm, nome, vetor, indice);
            v->itens[indice.i] = valor;
            break;
        }
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD: {
            Valor b = DESEMPILHAR();
            Valor a = DESEMPILHAR();
            EMPILHAR(vm_aritmetica(vm, nome, ins->op, a, b));
            break;
        }
        case OP_NEG: {
            Valor a = DESEMPILHAR();
            EMPILHAR(a.tag == V_FLOAT ? vm_float(-a.f) : vm_int(vm_int32(-a.i)));
            break;
        }
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
        case OP_EQ:
        case OP_NE: {
            Valor b = DESEMPILHAR();
            Valor a = DESEMPILHAR();
            EMPILHAR(vm_bool(vm_comparar(ins->op, a, b)));
            break;
        }
        case OP_AND:
        case OP_OR: {
            Valor b = DESEMPILHAR();
            Valor a = DESEMPILHAR();
            int r = ins->op == OP_AND ? (vm_verdade(a) && vm_verdade(b))
                                      : (vm_verdade(a) || vm_verdade(b));
            EMPILHAR(vm_bool(r));
            break;
        }
        case OP_NOT:
            TOPO = vm_bool(!vm_verdade(TOPO));
            break;
        case OP_ITOF:
            TOPO = vm_float((double)TOPO.i);
            break;
        case OP_CTOI: {
            Valor c = TOPO;
            size_t tam;
            TOPO = vm_int(c.slen ? vm_codigo((const unsigned char *)c.s, c.slen, &tam) : 0);
            break;
        }
        case OP_JMP:
            pc = (size_t)ins->arg;
            break;
        case OP_JZ:
            if (!vm_verdade(DESEMPILHAR())) {
                pc = (size_t)ins->arg;
            }
            break;
        case OP_JNZ:
            if (vm_verdade(DESEMPILHAR())) {
                pc = (size_t)ins->arg;
            }
            break;
        case OP_CALL: {
            if (vm->nquadros + 1 >= VM_PROFUNDIDADE_MAXIMA) {
                vm_erro(vm, nome,
                        vm_format("estouro da pilha de chamadas (mais de %d chamadas aninhadas).",
                                  VM_PROFUNDIDADE_MAXIMA));
            }
            VmQuadro chamador = {vm->funcao, vm->slots, pc};
            VM_PUSH(vm->quadros, vm->nquadros, vm->capquadros, chamador);
            vm->funcao = (size_t)ins->arg;
            codigo = &vm->funcoes[vm->funcao];
            vm->slots = vm_quadro_novo(codigo->f);
            for (int k = 0; k < ins->n; k++) {
                vm->slots[k] = vm->pilha[vm->npilha - (size_t)ins->n + (size_t)k];
            }
            vm->npilha -= (size_t)ins->n;
            pc = 0;
            break;
        }
        case OP_RET:
            if (vm->nquadros == 0) {
                vm_liberar_execucao(vm);
                return;
            }
            vm_quadro_free(codigo->f, vm->slots);
            vm->nquadros--;
            vm->funcao = vm->quadros[vm->nquadros].funcao;
            vm->slots = vm->quadros[vm->nquadros].slots;
            pc = vm->quadros[vm->nquadros].pc;
            codigo = &vm->funcoes[vm->funcao];
            break;
        case OP_POP:
            vm->npilha--;
            break;
        case OP_PRINT:
            vm_imprimir(DESEMPILHAR());
            break;
        case OP_READ: {
            Valor v = vm_ler(vm, nome, ins->tipo);
            EMPILHAR(v);
            break;
        }
        }
    }
}

int vm_executar(const VmPrograma *programa, char **erro, long long *executadas) {
    Vm *vm = vm_realloc(NULL, sizeof *vm);
    memset(vm, 0, sizeof *vm);
    vm->p = programa;
    *erro = NULL;
    *executadas = 0;
    vm_decodificar_programa(vm);
    vm->globais = vm_realloc(NULL, (programa->nglobais ? programa->nglobais : 1) * sizeof *vm->globais);
    for (size_t i = 0; i < programa->nglobais; i++) {
        const VmGlobal *g = &programa->globais[i];
        if (g->tamanho) {
            Valor v = {V_VETOR, 0, 0, NULL, 0,
                       vm_vetor_novo(g->nome, g->tipo, (size_t)strtol(g->tamanho, NULL, 10))};
            vm->globais[i] = v;
        } else {
            vm->globais[i] = vm_inicial(g->tipo);
        }
    }

    int codigo = 0;
    if (setjmp(vm->falha) == 0) {
        long init = vm_indice_funcao(vm, "@init");
        if (init >= 0) {
            vm_rodar(vm, (size_t)init, executadas);
        }
        vm_rodar(vm, (size_t)vm_indice_funcao(vm, "main"), executadas);
    } else {
        *erro = vm->erro;
        codigo = 6;
        vm_liberar_execucao(vm);
    }
    fflush(stdout);

    for (size_t i = 0; i < programa->nglobais; i++) {
        if (programa->globais[i].tamanho) {
            vm_vetor_free(vm->globais[i].v);
        }
    }
    free(vm->globais);
    for (size_t i = 0; i < vm->nfuncoes; i++) {
        free(vm->funcoes[i].codigo);
    }
    free(vm->funcoes);
    for (size_t i = 0; i < vm->nliterais; i++) {
        free(vm->literais[i]);
    }
    free(vm->literais);
    for (size_t i = 0; i < vm->nlidos; i++) {
        free(vm->lidos[i]);
    }
    free(vm->lidos);
    free(vm->entrada);
    free(vm);
    return codigo;
}
