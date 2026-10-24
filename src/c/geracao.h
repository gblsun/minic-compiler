/* Back-end do MINIC: IR de três endereços -> código da máquina de pilha (MVM).
 *
 * Transcrição para C de src/python/geracao.py: mesmo registro de ativação
 * (parâmetros, locais declarados, temporários), mesma convenção de chamada,
 * mesmas instruções e o mesmo texto do --target=asm e do --symbols. O modelo
 * de execução e o conjunto de instruções estão descritos lá e em
 * docs/backend.md.
 *
 * As strings (nomes, operandos, argumentos) são internadas no pool da IR: o
 * IrPrograma tem de viver pelo menos tanto quanto o VmPrograma.
 */

#ifndef GERACAO_H
#define GERACAO_H

#include <stddef.h>

#include "ir.h"

typedef struct {
    const char *op;         /* "PUSH", "LOAD", ..., ou "LABEL" */
    const char *arg;        /* NULL quando a instrução não tem argumento */
    int n;                  /* CALL: quantidade de argumentos */
    const char *comentario; /* nome do slot, só para leitura do texto */
} VmInstr;

typedef struct {
    int indice;
    const char *nome; /* nome na IR */
    const char *tipo; /* tipo base; NULL para temporário */
    long tamanho;     /* vetor local: tamanho; senão -1 */
    int vetor_param;  /* parâmetro `tipo nome[]` */
} VmSlot;

typedef struct {
    const char *nome;
    int nparams;
    VmSlot *slots;
    size_t nslots, capslots;
    VmInstr *codigo;
    size_t len, cap;
} VmFuncao;

typedef struct {
    const char *nome;
    const char *tipo;
    const char *tamanho; /* NULL para escalar */
} VmGlobal;

typedef struct {
    VmGlobal *globais;
    size_t nglobais, capglobais;
    VmFuncao *funcoes;
    size_t nfuncoes, capfuncoes;
} VmPrograma;

/* Gera o código. Devolve 0, ou 5 (erro na geração de código, Seção 11.1) com
 * a mensagem em *erro (o chamador é dono da string). `saida` deve vir zerada. */
int geracao_gerar(IrPrograma *ir, VmPrograma *saida, char **erro);

/* O código como texto (o --target=asm). O chamador é dono da string. */
char *geracao_formatar(const VmPrograma *programa);

/* Tabela de símbolos (o --symbols), com o deslocamento de cada nome no
 * registro de ativação. O chamador é dono da string. */
char *geracao_simbolos(IrPrograma *ir);

void geracao_free(VmPrograma *programa);

#endif /* GERACAO_H */
