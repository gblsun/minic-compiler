/* Analisador semântico do MINIC (etapa 3): nomes, escopos, tipos e contextos.
 *
 * Transcrição para C de src/python/semantico.py — mesmas passagens, mesma
 * ordem de visita e as mesmas mensagens, para que as duas implementações
 * produzam saída idêntica byte a byte. As regras estão em docs/semantica.md.
 */

#ifndef SEMANTICO_H
#define SEMANTICO_H

#include <stddef.h>

#include "ast.h"

typedef struct {
    const char *codigo; /* "SEM001", ... (literal estático) */
    int linha;
    int coluna;
    char *mensagem;     /* alocada; o diagnóstico é dono */
    size_t ordem;       /* ordem de descoberta, para desempatar a ordenação */
} Diagnostico;

typedef struct {
    Diagnostico *itens;
    size_t len;
    size_t cap;
} Diagnosticos;

/* Analisa um programa sintaticamente válido e preenche `saida` (que deve vir
 * zerada) com os diagnósticos, já ordenados por (linha, coluna) e, no empate,
 * pela ordem de descoberta. Nenhum diagnóstico = programa aceito. */
void semantico_analisar(const Ast *programa, Diagnosticos *saida);

void diagnosticos_free(Diagnosticos *diagnosticos);

/* "SEM001 — linha L, coluna C: mensagem". O chamador é dono da string. */
char *diagnostico_to_string(const Diagnostico *diagnostico);

/* Relatório no formato do gabarito da AP 2: um diagnóstico por linha e o
 * resumo no fim, separados por CRLF e sem quebra depois da última linha. O
 * chamador é dono da string. */
char *semantico_relatorio(const Diagnosticos *diagnosticos);

#endif /* SEMANTICO_H */
