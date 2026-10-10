/* Tabela de símbolos do MINIC: escopos léxicos aninhados (aula 14).
 *
 * Transcrição para C de src/python/tabela_simbolos.py. A tabela é uma pilha de
 * escopos; cada escopo é uma lista de símbolos (busca linear: um escopo de
 * MINIC tem poucas dezenas de nomes, e a ordem de inserção fica preservada).
 * Operações da Seção 6.2 da especificação: inserir, buscar,
 * buscar_no_escopo_atual, abrir_escopo e fechar_escopo.
 *
 * Como no Python, o módulo só guarda declarações; quem decide o que é erro é
 * o analisador semântico (semantico.c).
 */

#ifndef TABELA_SIMBOLOS_H
#define TABELA_SIMBOLOS_H

#include <stddef.h>

#include "ast.h"

typedef enum { CAT_VARIAVEL, CAT_PARAMETRO, CAT_VETOR, CAT_FUNCAO } Categoria;

/* Nome da categoria nas mensagens: "variável", "parâmetro", "vetor", "função". */
const char *categoria_nome(Categoria categoria);

/* Uma entrada da tabela. `nome` e `tipo` apontam para strings da AST (que
 * vive mais que a tabela durante a análise), então o símbolo não é dono
 * delas. */
typedef struct {
    const char *nome;
    Categoria categoria;
    const char *tipo; /* tipo base; para função, o tipo de retorno */
    int linha;
    int coluna;
    int eh_vetor;
    int tem_tamanho;    /* 0 quando o tamanho é desconhecido (parâmetro vetor) */
    long long tamanho;
    const Ast *funcao;  /* CAT_FUNCAO: o nó da definição (parâmetros em items) */
    /* Variável global coletada na passagem A mas ainda não alcançada na
     * passagem B: existe para a duplicidade, mas não para a busca. */
    int visivel;
} Simbolo;

/* Cria um símbolo visível, sem vetor e sem função associada. */
Simbolo *simbolo_novo(const char *nome, Categoria categoria, const char *tipo, int linha,
                      int coluna);

/* Como o símbolo aparece na mensagem de duplicidade: "tipo int",
 * "vetor de int" ou "função que retorna int". O chamador é dono da string. */
char *simbolo_descricao(const Simbolo *simbolo);

typedef struct {
    Simbolo **itens;
    size_t len;
    size_t cap;
} Escopo;

typedef struct {
    Escopo *escopos; /* escopos[0] é o global */
    size_t len;
    size_t cap;
} TabelaSimbolos;

/* Inicializa com o escopo global já aberto. */
void tabela_init(TabelaSimbolos *tabela);

/* Libera todos os escopos e símbolos. */
void tabela_free(TabelaSimbolos *tabela);

void tabela_abrir_escopo(TabelaSimbolos *tabela);

/* Desempilha o escopo atual, liberando os símbolos dele. */
void tabela_fechar_escopo(TabelaSimbolos *tabela);

/* Declara no escopo atual e passa a ser dona do símbolo. Se o nome já existe
 * neste escopo, NÃO insere e devolve o símbolo anterior — aí o símbolo
 * recusado continua sendo do chamador, que deve liberá-lo. */
Simbolo *tabela_inserir(TabelaSimbolos *tabela, Simbolo *simbolo);

Simbolo *tabela_buscar_no_escopo_atual(const TabelaSimbolos *tabela, const char *nome);

/* Declaração visível mais interna (sombreamento), ou NULL. */
Simbolo *tabela_buscar(const TabelaSimbolos *tabela, const char *nome);

#endif /* TABELA_SIMBOLOS_H */
