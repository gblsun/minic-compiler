/* Representação intermediária (IR) do MINIC: código de três endereços.
 *
 * Transcrição para C de src/python/ir.py — mesma tradução, mesma ordem de
 * emissão, mesmos nomes de temporários e rótulos, para que as duas
 * implementações imprimam a mesma IR byte a byte. As decisões (nomes com
 * sufixo, conversões explícitas, vetores, curto-circuito, globais) estão
 * comentadas lá e em docs/ir.md.
 *
 * Todas as strings da IR (nomes, operandos, rótulos, operadores) são
 * **internadas** no `IrPool` do programa: cada texto distinto existe uma vez
 * só, e o programa é dono de todos. Duas consequências: comparar dois
 * operandos é comparar ponteiros, e o otimizador pode trocar um operando por
 * outro sem se preocupar com quem libera o quê.
 *
 *   kind       texto                          campos usados
 *   ---------  -----------------------------  ------------------------
 *   IR_COPY    dst = a                        dst, a
 *   IR_BIN     dst = a op b                   dst, a, op, b
 *   IR_UN      dst = op a                     dst, op, a  (neg, not, itof, ctoi)
 *   IR_LOAD    dst = load a[b]                dst, a, b
 *   IR_STORE   store a[b], c                  a, b, c
 *   IR_PRINT   print a                        a
 *   IR_READ    read dst                       dst
 *   IR_PARAM   param a                        a
 *   IR_CALL    dst = call a, n  /  call a, n  dst (opcional), a, n
 *   IR_RET     return a  /  return            a (opcional)
 *   IR_GOTO    goto label                     label
 *   IR_IFF     ifFalse a goto label           a, label
 *   IR_IFT     ifTrue a goto label            a, label
 *   IR_LABEL   label:                         label
 */

#ifndef IR_H
#define IR_H

#include <stddef.h>

#include "ast.h"

typedef enum {
    IR_COPY,
    IR_BIN,
    IR_UN,
    IR_LOAD,
    IR_STORE,
    IR_PRINT,
    IR_READ,
    IR_PARAM,
    IR_CALL,
    IR_RET,
    IR_GOTO,
    IR_IFF,
    IR_IFT,
    IR_LABEL
} IrKind;

typedef struct {
    IrKind kind;
    const char *dst; /* todos internados no pool do programa (ou NULL) */
    const char *a;
    const char *b;
    const char *c;
    const char *op;
    const char *label;
    int n;
} IrInstr;

/* Uma declaração, como o gerador a viu (Seção 6.1). Não aparece no texto da
 * IR: serve ao back-end (geracao.c) e ao --symbols. Strings internadas. */
typedef struct {
    const char *nome;      /* como no código */
    const char *nome_ir;   /* com o sufixo de sombreamento, se houver */
    const char *categoria; /* variável, parâmetro, parâmetro vetor, vetor, função */
    const char *tipo;      /* tipo base (função: o de retorno) */
    const char *tamanho;   /* vetor declarado: o literal do tamanho; senão NULL */
    int nivel;             /* 0 global, 1 corpo da função, +1 por bloco */
    int linha;
    int coluna;
    int inicializada;
} IrSimbolo;

/* Par nome na IR -> tipo base (variáveis e destinos de `read`). */
typedef struct {
    const char *nome;
    const char *tipo;
} IrTipo;

typedef struct {
    const char *nome;
    const char **params;
    size_t nparams, capparams;
    IrInstr *instrs;
    size_t len, cap;
    IrSimbolo *simbolos; /* parâmetros e locais, em ordem de declaração */
    size_t nsimbolos, capsimbolos;
    IrTipo *tipos;
    size_t ntipos, captipos;
} IrFuncao;

typedef struct {
    const char *nome;
    const char *tamanho; /* NULL para escalar */
    const char *tipo;    /* tipo base */
} IrGlobal;

/* Tabela de espalhamento com endereçamento aberto: texto -> cópia única. */
typedef struct {
    char **slots;
    size_t cap;
    size_t len;
} IrPool;

typedef struct {
    IrGlobal *globais;
    size_t nglobais, capglobais;
    IrFuncao *funcoes;
    size_t nfuncoes, capfuncoes;
    IrSimbolo *simbolos; /* globais e funções, em ordem de código */
    size_t nsimbolos, capsimbolos;
    IrPool pool;
} IrPrograma;

/* Gera a IR (-O0) de um programa que passou pela análise semântica. `saida`
 * deve vir zerada. */
void ir_gerar(const Ast *programa, IrPrograma *saida);

/* A IR inteira como texto, uma instrução por linha, LF no fim de cada uma.
 * O chamador é dono da string. */
char *ir_formatar(const IrPrograma *programa);

void ir_programa_free(IrPrograma *programa);

/* Cópia única de `texto` no pool do programa. */
const char *ir_intern(IrPrograma *programa, const char *texto);

/* Literal (número, caractere, cadeia, true/false) ou nome? */
int ir_eh_literal(const char *operando);

/* `t` seguido só de dígitos: a forma de um temporário. */
int ir_eh_temporario(const char *nome);

/* Acrescenta uma instrução no fim da função. */
void ir_emitir(IrFuncao *funcao, IrInstr instr);

/* Tipo base de `nome` na função (variável local, parâmetro ou destino de
 * `read`), ou NULL. */
const char *ir_tipo_local(const IrFuncao *funcao, const char *nome);

/* Tipo base da global `nome`, ou NULL se não for global. */
const char *ir_tipo_global(const IrPrograma *programa, const char *nome);

#endif /* IR_H */
