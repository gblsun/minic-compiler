/* Ponte entre os exercícios da aula 13 e o scanner do projeto (src/c/lexer.c).
 *
 * Equivalente C de comum/fluxo.py. O scanner não é copiado: este módulo usa a
 * interface de src/c/lexer.h e oferece por cima dela um fluxo de tokens com
 * lookahead (next_token/lookahead), mais a leitura do arquivo e os códigos de
 * saída comuns a todos os exercícios:
 *
 *   0 = ok, 1 = erro de uso/arquivo, 2 = erro léxico, 3 = erro de sintaxe,
 *   4 = erro semântico.
 *
 * Compilação: junto com ../../../src/c/lexer.c (ver README da aula 13).
 */

#ifndef FLUXO_H
#define FLUXO_H

#include <stddef.h>

#include "../../../src/c/lexer.h"

enum { OK = 0, ERRO_USO = 1, ERRO_LEXICO = 2, ERRO_SINTAXE = 3, ERRO_SEMANTICO = 4 };

typedef struct {
    Lexer lexer;
    char *fonte; /* o texto lido; o Fluxo é dono dele */
    size_t pos;  /* índice do próximo token a consumir */
} Fluxo;

/* Lê o arquivo (ou stdin, se caminho for "-") inteiro para uma string.
 * Devolve NULL (com a mensagem já impressa em stderr) se não conseguir. */
char *ler_fonte(const char *caminho);

/* Lê, tokeniza e prepara o fluxo. Devolve OK, ERRO_USO ou ERRO_LEXICO (com as
 * mensagens já impressas em stderr). Em caso de erro, o fluxo já foi liberado. */
int fluxo_abrir(Fluxo *f, const char *caminho);
void fluxo_fechar(Fluxo *f);

const Token *fluxo_atual(const Fluxo *f);
const Token *fluxo_espiar(const Fluxo *f, size_t k); /* lookahead */
const Token *fluxo_proximo(Fluxo *f);                /* next_token */
int fluxo_verifica(const Fluxo *f, TokenType tipo);
int fluxo_aceita(Fluxo *f, TokenType tipo); /* consome se casar; 1 = consumiu */

/* Como um token aparece nas mensagens: 'x' ou "fim da entrada".
 * Devolve string alocada (o chamador libera). */
char *descrever(const Token *t);

/* Imprime "Erro de sintaxe na linha L, coluna C: esperado X; encontrado Y." */
void erro_sintaxe(const Token *t, const char *esperado);

/* Utilitários de memória: abortam com mensagem (exit 1) se faltar memória. */
void *aloca(size_t n);
void *realoca(void *p, size_t n);
char *copia(const char *s);
char *formata(const char *fmt, ...);

#endif /* FLUXO_H */
