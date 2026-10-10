/* Implementação da tabela de símbolos — ver tabela_simbolos.h. */

#include "tabela_simbolos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *ts_alocar(size_t bytes) {
    void *p = malloc(bytes);
    if (!p) {
        fprintf(stderr, "erro: memória insuficiente na tabela de símbolos\n");
        exit(1);
    }
    return p;
}

const char *categoria_nome(Categoria categoria) {
    switch (categoria) {
    case CAT_VARIAVEL:
        return "variável";
    case CAT_PARAMETRO:
        return "parâmetro";
    case CAT_VETOR:
        return "vetor";
    case CAT_FUNCAO:
        return "função";
    }
    return "?";
}

Simbolo *simbolo_novo(const char *nome, Categoria categoria, const char *tipo, int linha,
                      int coluna) {
    Simbolo *s = ts_alocar(sizeof(Simbolo));
    s->nome = nome;
    s->categoria = categoria;
    s->tipo = tipo;
    s->linha = linha;
    s->coluna = coluna;
    s->eh_vetor = 0;
    s->tem_tamanho = 0;
    s->tamanho = 0;
    s->funcao = NULL;
    s->visivel = 1;
    return s;
}

char *simbolo_descricao(const Simbolo *simbolo) {
    const char *prefixo = simbolo->categoria == CAT_FUNCAO ? "função que retorna "
                          : simbolo->eh_vetor              ? "vetor de "
                                                           : "tipo ";
    size_t n = strlen(prefixo) + strlen(simbolo->tipo) + 1;
    char *texto = ts_alocar(n);
    snprintf(texto, n, "%s%s", prefixo, simbolo->tipo);
    return texto;
}

void tabela_init(TabelaSimbolos *tabela) {
    tabela->escopos = NULL;
    tabela->len = 0;
    tabela->cap = 0;
    tabela_abrir_escopo(tabela);
}

void tabela_free(TabelaSimbolos *tabela) {
    while (tabela->len > 0) {
        tabela_fechar_escopo(tabela);
    }
    free(tabela->escopos);
    tabela->escopos = NULL;
    tabela->cap = 0;
}

void tabela_abrir_escopo(TabelaSimbolos *tabela) {
    if (tabela->len == tabela->cap) {
        size_t nova = tabela->cap == 0 ? 8 : tabela->cap * 2;
        Escopo *novo = realloc(tabela->escopos, nova * sizeof(Escopo));
        if (!novo) {
            fprintf(stderr, "erro: memória insuficiente na tabela de símbolos\n");
            exit(1);
        }
        tabela->escopos = novo;
        tabela->cap = nova;
    }
    Escopo *escopo = &tabela->escopos[tabela->len++];
    escopo->itens = NULL;
    escopo->len = 0;
    escopo->cap = 0;
}

void tabela_fechar_escopo(TabelaSimbolos *tabela) {
    Escopo *escopo = &tabela->escopos[--tabela->len];
    for (size_t i = 0; i < escopo->len; i++) {
        free(escopo->itens[i]);
    }
    free(escopo->itens);
}

static Simbolo *ts_procurar(const Escopo *escopo, const char *nome) {
    for (size_t i = 0; i < escopo->len; i++) {
        if (strcmp(escopo->itens[i]->nome, nome) == 0) {
            return escopo->itens[i];
        }
    }
    return NULL;
}

Simbolo *tabela_inserir(TabelaSimbolos *tabela, Simbolo *simbolo) {
    Escopo *atual = &tabela->escopos[tabela->len - 1];
    Simbolo *anterior = ts_procurar(atual, simbolo->nome);
    if (anterior) {
        return anterior;
    }
    if (atual->len == atual->cap) {
        size_t nova = atual->cap == 0 ? 8 : atual->cap * 2;
        Simbolo **novo = realloc(atual->itens, nova * sizeof(Simbolo *));
        if (!novo) {
            fprintf(stderr, "erro: memória insuficiente na tabela de símbolos\n");
            exit(1);
        }
        atual->itens = novo;
        atual->cap = nova;
    }
    atual->itens[atual->len++] = simbolo;
    return NULL;
}

Simbolo *tabela_buscar_no_escopo_atual(const TabelaSimbolos *tabela, const char *nome) {
    return ts_procurar(&tabela->escopos[tabela->len - 1], nome);
}

Simbolo *tabela_buscar(const TabelaSimbolos *tabela, const char *nome) {
    for (size_t i = tabela->len; i-- > 0;) {
        Simbolo *s = ts_procurar(&tabela->escopos[i], nome);
        if (s && s->visivel) {
            return s;
        }
    }
    return NULL;
}
