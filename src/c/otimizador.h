/* Otimizador da IR do MINIC (-O1).
 *
 * Transcrição para C de src/python/otimizador.py — mesmos passes, na mesma
 * ordem, com as mesmas regras, para que a IR otimizada saia idêntica nas duas
 * implementações. Os passes e os cuidados de corretude estão descritos lá e
 * em docs/ir.md:
 *
 *   1. simplificação local (dobra de constantes, identidades algébricas,
 *      desvio sobre constante);
 *   2. propagação global de constantes e cópias (fluxo de dados forward);
 *   3. eliminação de subexpressões comuns local;
 *   4. limpeza do fluxo de controle (blocos inalcançáveis, desvios e rótulos
 *      inúteis);
 *   5. eliminação de código morto global (variáveis vivas, backward);
 *   6. laços (aula 19): dominadores e laços naturais; código invariante movido
 *      para o pré-cabeçalho e redução de força em variáveis de indução;
 *
 * repetidos até nada mudar; no fim, os temporários são renumerados.
 */

#ifndef OTIMIZADOR_H
#define OTIMIZADOR_H

#include "ir.h"

/* Otimiza todas as funções do programa, no lugar. */
void otimizador_otimizar(IrPrograma *programa);

#endif /* OTIMIZADOR_H */
