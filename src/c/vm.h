/* Máquina virtual de pilha do MINIC (MVM): executa o código de geracao.c.
 *
 * Transcrição para C de src/python/vm.py — mesma semântica (int de 32 bits,
 * divisão truncada, formato do float no print, regras do read) e as mesmas
 * mensagens de erro de execução, para que as duas implementações tenham a
 * mesma saída byte a byte.
 */

#ifndef VM_H
#define VM_H

#include "geracao.h"

/* Executa o programa (lendo stdin e escrevendo stdout). Devolve 0, ou 6 em
 * erro de execução, com a mensagem completa ("Erro de execução em f: ...") em
 * *erro (o chamador é dono). *executadas recebe a contagem de instruções. */
int vm_executar(const VmPrograma *programa, char **erro, long long *executadas);

#endif /* VM_H */
