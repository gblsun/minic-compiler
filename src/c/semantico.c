/* Implementação do analisador semântico — ver semantico.h.
 *
 * A ordem das funções acompanha src/python/semantico.py: tipos, texto e
 * posição das expressões, análise de fluxo, e o analisador (declarações,
 * comandos, expressões). Todos os nomes `static` têm prefixo `sem_` porque o
 * minic.c da raiz junta este arquivo com o lexer e o parser numa unidade de
 * tradução só.
 */

#include "semantico.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tabela_simbolos.h"

/* ------------------------------------------------------------------ *
 * Utilitários de string
 * ------------------------------------------------------------------ */

static char *sem_format(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list copia;
    va_copy(copia, args);
    int n = vsnprintf(NULL, 0, fmt, copia);
    va_end(copia);
    char *buf = n < 0 ? NULL : malloc((size_t)n + 1);
    if (!buf) {
        fprintf(stderr, "erro: memória insuficiente na análise semântica\n");
        exit(1);
    }
    vsnprintf(buf, (size_t)n + 1, fmt, args);
    va_end(args);
    return buf;
}

/* ------------------------------------------------------------------ *
 * Tipos: pares (base, eh_vetor), como as tuplas do Python
 * ------------------------------------------------------------------ */

typedef struct {
    const char *base;
    int vetor;
} SemTipo;

static const SemTipo T_INT = {"int", 0};
static const SemTipo T_FLOAT = {"float", 0};
static const SemTipo T_BOOL = {"bool", 0};
static const SemTipo T_CHAR = {"char", 0};
static const SemTipo T_STRING = {"string", 0};
static const SemTipo T_VOID = {"void", 0};
/* Sentinela de um nó que já gerou diagnóstico: quem o recebe não reclama de
 * novo (evita cascata). */
static const SemTipo T_ERRO = {"erro", 0};

static SemTipo sem_tipo(const char *base, int vetor) {
    SemTipo t = {base, vetor};
    return t;
}

static int sem_igual(SemTipo a, SemTipo b) {
    return a.vetor == b.vetor && strcmp(a.base, b.base) == 0;
}

static int sem_e_erro(SemTipo t) { return sem_igual(t, T_ERRO); }

static char *sem_nome_tipo(SemTipo t) {
    return t.vetor ? sem_format("vetor de %s", t.base) : sem_format("%s", t.base);
}

/* int, float e char (que é promovido a int, Seção 5 da especificação). */
static int sem_numerico(SemTipo t) {
    return !t.vetor && (strcmp(t.base, "int") == 0 || strcmp(t.base, "float") == 0 ||
                        strcmp(t.base, "char") == 0);
}

static int sem_inteiro(SemTipo t) {
    return !t.vetor && (strcmp(t.base, "int") == 0 || strcmp(t.base, "char") == 0);
}

/* Ponto único da regra de compatibilidade: tipos iguais, int -> float e
 * char -> int. Vetores nunca são compatíveis aqui. */
static int sem_compativel(SemTipo valor, SemTipo esperado) {
    if (valor.vetor || esperado.vetor) {
        return 0;
    }
    if (sem_igual(valor, esperado)) {
        return 1;
    }
    return (sem_igual(valor, T_INT) && sem_igual(esperado, T_FLOAT)) ||
           (sem_igual(valor, T_CHAR) && sem_igual(esperado, T_INT));
}

static SemTipo sem_tipo_literal(const char *categoria) {
    if (strcmp(categoria, "int") == 0) return T_INT;
    if (strcmp(categoria, "real") == 0) return T_FLOAT;
    if (strcmp(categoria, "bool") == 0) return T_BOOL;
    if (strcmp(categoria, "char") == 0) return T_CHAR;
    return T_STRING;
}

static const char *sem_descricao_literal(const char *categoria) {
    if (strcmp(categoria, "int") == 0) return "o literal inteiro";
    if (strcmp(categoria, "real") == 0) return "o literal real";
    if (strcmp(categoria, "bool") == 0) return "o literal lógico";
    if (strcmp(categoria, "char") == 0) return "o literal de caractere";
    return "a cadeia";
}

/* strtoll satura em LLONG_MAX, o mesmo teto que o Python aplica
 * (MAIOR_CONSTANTE), então as duas versões imprimem o mesmo número. */
static long long sem_valor_inteiro(const char *texto) {
    errno = 0;
    return strtoll(texto, NULL, 10);
}

/* Valor de um literal inteiro, com sinal opcional (`-1`): 1 se for constante. */
static int sem_constante_inteira(const Ast *no, long long *valor) {
    int negativo = 0;
    if (no->kind == AST_UNARY && strcmp(no->text, "-") == 0) {
        negativo = 1;
        no = no->a;
    }
    if (no->kind != AST_LIT || strcmp(no->type_name, "int") != 0) {
        return 0;
    }
    long long v = sem_valor_inteiro(no->text);
    *valor = negativo ? -v : v;
    return 1;
}

static int sem_zero_constante(const Ast *no) {
    if (no->kind != AST_LIT) {
        return 0;
    }
    if (strcmp(no->type_name, "int") == 0) {
        return sem_valor_inteiro(no->text) == 0;
    }
    if (strcmp(no->type_name, "real") == 0) {
        return strtod(no->text, NULL) == 0.0;
    }
    return 0;
}

/* ------------------------------------------------------------------ *
 * Expressões como texto e posição de início
 * ------------------------------------------------------------------ */

static int sem_precedencia(const char *op) {
    if (strcmp(op, "||") == 0) return 1;
    if (strcmp(op, "&&") == 0) return 2;
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) return 3;
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return 5;
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "%") == 0) return 6;
    return 4; /* <, >, <=, >= */
}

static char *sem_texto(const Ast *no);

static char *sem_entre_parenteses(char *texto) {
    char *com = sem_format("(%s)", texto);
    free(texto);
    return com;
}

/* Base de `[...]` ou `(...)`: só um primário dispensa parênteses. */
static char *sem_texto_posfixo(const Ast *no) {
    char *texto = sem_texto(no);
    if (no->kind == AST_BINARY || no->kind == AST_UNARY || no->kind == AST_ASSIGN) {
        return sem_entre_parenteses(texto);
    }
    return texto;
}

/* Reescreve uma expressão como código-fonte, com parênteses só onde a
 * precedência exige e um espaço em volta de cada operador binário. */
static char *sem_texto(const Ast *no) {
    switch (no->kind) {
    case AST_LIT:
    case AST_ID:
        return sem_format("%s", no->text);
    case AST_INDEX: {
        char *base = sem_texto_posfixo(no->a);
        char *indice = sem_texto(no->b);
        char *texto = sem_format("%s[%s]", base, indice);
        free(base);
        free(indice);
        return texto;
    }
    case AST_CALL: {
        char *args = sem_format("%s", "");
        for (size_t i = 0; i < no->items_len; i++) {
            char *arg = sem_texto(no->items[i]);
            char *junto = sem_format("%s%s%s", args, i ? ", " : "", arg);
            free(args);
            free(arg);
            args = junto;
        }
        char *callee = sem_texto_posfixo(no->a);
        char *texto = sem_format("%s(%s)", callee, args);
        free(callee);
        free(args);
        return texto;
    }
    case AST_UNARY: {
        char *operando = sem_texto(no->a);
        if (no->a->kind == AST_BINARY || no->a->kind == AST_ASSIGN) {
            operando = sem_entre_parenteses(operando);
        }
        char *texto = sem_format("%s%s", no->text, operando);
        free(operando);
        return texto;
    }
    case AST_BINARY: {
        int nivel = sem_precedencia(no->text);
        char *esquerda = sem_texto(no->a);
        if (no->a->kind == AST_ASSIGN ||
            (no->a->kind == AST_BINARY && sem_precedencia(no->a->text) < nivel)) {
            esquerda = sem_entre_parenteses(esquerda);
        }
        /* `<=` à direita: os binários são associativos à esquerda. */
        char *direita = sem_texto(no->b);
        if (no->b->kind == AST_ASSIGN ||
            (no->b->kind == AST_BINARY && sem_precedencia(no->b->text) <= nivel)) {
            direita = sem_entre_parenteses(direita);
        }
        char *texto = sem_format("%s %s %s", esquerda, no->text, direita);
        free(esquerda);
        free(direita);
        return texto;
    }
    case AST_ASSIGN: {
        char *alvo = sem_texto(no->a);
        char *valor = sem_texto(no->b);
        char *texto = sem_format("%s = %s", alvo, valor);
        free(alvo);
        free(valor);
        return texto;
    }
    default:
        fprintf(stderr, "erro interno: nó %d não é expressão\n", (int)no->kind);
        exit(1);
    }
}

/* Posição do primeiro token da expressão (o Binary guarda a do operador). */
static const Ast *sem_inicio(const Ast *no) {
    for (;;) {
        if (no->kind == AST_BINARY || no->kind == AST_ASSIGN || no->kind == AST_INDEX ||
            no->kind == AST_CALL) {
            no = no->a;
        } else {
            return no;
        }
    }
}

/* ------------------------------------------------------------------ *
 * Análise de fluxo (retorno em todos os caminhos)
 * ------------------------------------------------------------------ */

/* O comando pode terminar normalmente? Regra conservadora da apostila. */
static int sem_completa(const Ast *comando) {
    switch (comando->kind) {
    case AST_RETURN:
    case AST_BREAK:
    case AST_CONTINUE:
        return 0;
    case AST_IF:
        if (comando->c == NULL) {
            return 1;
        }
        return sem_completa(comando->b) || sem_completa(comando->c);
    case AST_BLOCK:
        for (size_t i = 0; i < comando->items_len; i++) {
            if (!sem_completa(comando->items[i])) {
                return 0;
            }
        }
        return 1;
    default:
        return 1;
    }
}

static char *sem_motivo_da_queda(const Ast *comando) {
    switch (comando->kind) {
    case AST_IF: {
        char *condicao = sem_texto(comando->a);
        const char *valor =
            comando->c != NULL && sem_completa(comando->b) ? "verdadeiro" : "falso";
        char *motivo =
            sem_format("o ramo em que “%s” é %s alcança o fim do corpo", condicao, valor);
        free(condicao);
        return motivo;
    }
    case AST_WHILE:
        return sem_format("o laço while da linha %d pode terminar e alcançar o fim do corpo",
                          comando->line);
    case AST_FOR:
        return sem_format("o laço for da linha %d pode terminar e alcançar o fim do corpo",
                          comando->line);
    case AST_BLOCK:
        if (comando->items_len > 0) {
            return sem_motivo_da_queda(comando->items[comando->items_len - 1]);
        }
        return sem_format("o fim do corpo é alcançado sem return");
    default:
        return sem_format("o fim do corpo é alcançado sem return");
    }
}

/* ------------------------------------------------------------------ *
 * O analisador
 * ------------------------------------------------------------------ */

typedef struct {
    TabelaSimbolos tabela;
    Diagnosticos *diagnosticos;
    const Ast *funcao; /* nó AST_FUNCTION cujo corpo está sendo visitado */
    int lacos;         /* profundidade de laços (contexto de break/continue) */
} Analisador;

/* Registra um diagnóstico; o analisador passa a ser dono de `mensagem`. */
static void sem_erro(Analisador *an, const char *codigo, const Ast *posicao, char *mensagem) {
    Diagnosticos *d = an->diagnosticos;
    if (d->len == d->cap) {
        size_t nova = d->cap == 0 ? 16 : d->cap * 2;
        Diagnostico *novo = realloc(d->itens, nova * sizeof(Diagnostico));
        if (!novo) {
            fprintf(stderr, "erro: memória insuficiente na análise semântica\n");
            exit(1);
        }
        d->itens = novo;
        d->cap = nova;
    }
    Diagnostico *diag = &d->itens[d->len];
    diag->codigo = codigo;
    diag->linha = posicao->line;
    diag->coluna = posicao->column;
    diag->mensagem = mensagem;
    diag->ordem = d->len;
    d->len++;
}

static SemTipo sem_tipo_de(Analisador *an, const Ast *no);
static SemTipo sem_valor(Analisador *an, const Ast *expressao, const char *contexto);
static void sem_comando(Analisador *an, const Ast *comando);
static SemTipo sem_destino(Analisador *an, const Ast *alvo, const char *comando);

/* -- declarações ---------------------------------------------------- */

static int sem_declarar(Analisador *an, Simbolo *simbolo, const Ast *posicao) {
    Simbolo *anterior = tabela_inserir(&an->tabela, simbolo);
    if (anterior == NULL) {
        return 1;
    }
    char *descricao = simbolo_descricao(anterior);
    sem_erro(an, "SEM002", posicao,
             sem_format("“%s” já declarado neste escopo; declaração anterior na linha %d, "
                        "coluna %d (%s).",
                        simbolo->nome, anterior->linha, anterior->coluna, descricao));
    free(descricao);
    free(simbolo);
    return 0;
}

static Simbolo *sem_simbolo_de_variavel(const Ast *decl) {
    if (decl->b == NULL) {
        return simbolo_novo(decl->text, CAT_VARIAVEL, decl->type_name, decl->line,
                            decl->column);
    }
    Simbolo *s = simbolo_novo(decl->text, CAT_VETOR, decl->type_name, decl->line, decl->column);
    s->eh_vetor = 1;
    long long tamanho;
    if (sem_constante_inteira(decl->b, &tamanho) && tamanho > 0) {
        s->tem_tamanho = 1;
        s->tamanho = tamanho;
    }
    return s;
}

/* Passagem A: assinaturas de funções e nomes globais, em ordem de código. */
static void sem_coletar_globais(Analisador *an, const Ast *programa) {
    for (size_t i = 0; i < programa->items_len; i++) {
        const Ast *item = programa->items[i];
        if (item->kind == AST_FUNCTION) {
            Simbolo *s =
                simbolo_novo(item->text, CAT_FUNCAO, item->type_name, item->line, item->column);
            s->funcao = item;
            sem_declarar(an, s, item);
        } else if (item->kind == AST_VAR_DECL) {
            Simbolo *s = sem_simbolo_de_variavel(item);
            s->visivel = 0; /* passa a valer em sem_variavel_global */
            sem_declarar(an, s, item);
        }
    }
}

static void sem_exigir_compatibilidade(Analisador *an, SemTipo tipo, SemTipo esperado,
                                       const char *destino, const Ast *expressao) {
    if (sem_e_erro(tipo) || sem_e_erro(esperado) || sem_compativel(tipo, esperado)) {
        return;
    }
    char *de = sem_nome_tipo(tipo);
    char *para = sem_nome_tipo(esperado);
    char *texto = sem_texto(expressao);
    sem_erro(an, "SEM003", sem_inicio(expressao),
             sem_format("Não é possível atribuir %s a %s sem conversão permitida (destino "
                        "“%s”; expressão “%s”).",
                        de, para, destino, texto));
    free(de);
    free(para);
    free(texto);
}

/* Tamanho de vetor e inicializador (o nome já está no escopo, como em C). */
static void sem_verificar_declaracao(Analisador *an, const Ast *decl) {
    if (decl->b != NULL) {
        long long tamanho;
        int valido = decl->b->kind == AST_LIT && sem_constante_inteira(decl->b, &tamanho) &&
                     tamanho > 0;
        if (!valido) {
            char *texto = sem_texto(decl->b);
            sem_erro(an, "SEM016", sem_inicio(decl->b),
                     sem_format("Tamanho do vetor “%s” deve ser um literal inteiro positivo; "
                                "recebeu “%s”.",
                                decl->text, texto));
            free(texto);
        }
    }
    if (decl->a != NULL) {
        SemTipo tipo = sem_valor(an, decl->a, "inicializador");
        sem_exigir_compatibilidade(an, tipo, sem_tipo(decl->type_name, 0), decl->text, decl->a);
    }
}

static void sem_variavel_global(Analisador *an, const Ast *decl) {
    Simbolo *s = tabela_buscar_no_escopo_atual(&an->tabela, decl->text);
    /* Só o dono do nome se torna visível; uma redeclaração (já reportada na
     * passagem A) não substitui a primeira. */
    if (s != NULL && s->linha == decl->line && s->coluna == decl->column) {
        s->visivel = 1;
    }
    sem_verificar_declaracao(an, decl);
}

static void sem_variavel_local(Analisador *an, const Ast *decl) {
    sem_declarar(an, sem_simbolo_de_variavel(decl), decl);
    sem_verificar_declaracao(an, decl);
}

static void sem_item_de_bloco(Analisador *an, const Ast *item) {
    if (item->kind == AST_VAR_DECL) {
        sem_variavel_local(an, item);
    } else {
        sem_comando(an, item);
    }
}

static void sem_funcao(Analisador *an, const Ast *funcao) {
    tabela_abrir_escopo(&an->tabela);
    for (size_t i = 0; i < funcao->items_len; i++) {
        const Ast *param = funcao->items[i];
        Simbolo *s = simbolo_novo(param->text, param->is_array ? CAT_VETOR : CAT_PARAMETRO,
                                  param->type_name, param->line, param->column);
        s->eh_vetor = param->is_array;
        sem_declarar(an, s, param);
    }
    /* O corpo usa o mesmo escopo dos parâmetros: redeclarar um parâmetro no
     * nível de fora do corpo é duplicidade, como em C. */
    const Ast *anterior = an->funcao;
    an->funcao = funcao;
    for (size_t i = 0; i < funcao->a->items_len; i++) {
        sem_item_de_bloco(an, funcao->a->items[i]);
    }
    an->funcao = anterior;
    tabela_fechar_escopo(&an->tabela);

    if (strcmp(funcao->type_name, "void") != 0 && sem_completa(funcao->a)) {
        char *motivo = sem_motivo_da_queda(funcao->a);
        sem_erro(an, "SEM011", funcao,
                 sem_format("A função “%s” pode terminar sem retornar %s; %s.", funcao->text,
                            funcao->type_name, motivo));
        free(motivo);
    }
}

/* -- comandos ------------------------------------------------------- */

static void sem_laco(Analisador *an, const Ast *corpo) {
    an->lacos++;
    sem_comando(an, corpo);
    an->lacos--;
}

static void sem_condicao(Analisador *an, const Ast *expressao, const char *comando) {
    SemTipo tipo = sem_valor(an, expressao, "condição");
    if (!sem_e_erro(tipo) && !sem_igual(tipo, T_BOOL)) {
        char *nome = sem_nome_tipo(tipo);
        char *texto = sem_texto(expressao);
        sem_erro(an, "SEM005", sem_inicio(expressao),
                 sem_format("Condição de %s deve ter tipo bool; recebeu %s (expressão “%s”).",
                            comando, nome, texto));
        free(nome);
        free(texto);
    }
}

static void sem_retorno(Analisador *an, const Ast *comando) {
    const Ast *expr = comando->a;
    if (an->funcao == NULL) {
        sem_erro(an, "SEM010", comando,
                 sem_format("return só pode ocorrer dentro de uma função."));
        if (expr != NULL) {
            sem_tipo_de(an, expr);
        }
        return;
    }

    const char *nome = an->funcao->text;
    const char *esperado = an->funcao->type_name;
    if (expr == NULL) {
        if (strcmp(esperado, "void") != 0) {
            sem_erro(an, "SEM009", comando,
                     sem_format("Retorno sem valor na função “%s”, que deve retornar %s.", nome,
                                esperado));
        }
        return;
    }

    if (strcmp(esperado, "void") == 0) {
        sem_tipo_de(an, expr);
        char *texto = sem_texto(expr);
        sem_erro(an, "SEM009", sem_inicio(expr),
                 sem_format("A função “%s” é void e não pode retornar valor (expressão “%s”).",
                            nome, texto));
        free(texto);
        return;
    }

    SemTipo tipo = sem_valor(an, expr, "valor de retorno");
    if (!sem_e_erro(tipo) && !sem_compativel(tipo, sem_tipo(esperado, 0))) {
        char *recebido = sem_nome_tipo(tipo);
        sem_erro(an, "SEM009", sem_inicio(expr),
                 sem_format("Retorno %s incompatível com o tipo %s da função “%s”; conversão "
                            "implícita de %s para %s não permitida.",
                            recebido, esperado, nome, recebido, esperado));
        free(recebido);
    }
}

static void sem_comando(Analisador *an, const Ast *comando) {
    switch (comando->kind) {
    case AST_BLOCK:
        tabela_abrir_escopo(&an->tabela);
        for (size_t i = 0; i < comando->items_len; i++) {
            sem_item_de_bloco(an, comando->items[i]);
        }
        tabela_fechar_escopo(&an->tabela);
        break;
    case AST_EXPR_STMT:
        if (comando->a != NULL) {
            sem_tipo_de(an, comando->a); /* chamada void é válida como comando */
        }
        break;
    case AST_IF:
        sem_condicao(an, comando->a, "if");
        sem_comando(an, comando->b);
        if (comando->c != NULL) {
            sem_comando(an, comando->c);
        }
        break;
    case AST_WHILE:
        sem_condicao(an, comando->a, "while");
        sem_laco(an, comando->b);
        break;
    case AST_FOR:
        if (comando->a != NULL) {
            sem_tipo_de(an, comando->a);
        }
        if (comando->b != NULL) {
            sem_condicao(an, comando->b, "for");
        }
        if (comando->c != NULL) {
            sem_tipo_de(an, comando->c);
        }
        sem_laco(an, comando->d);
        break;
    case AST_RETURN:
        sem_retorno(an, comando);
        break;
    case AST_BREAK:
    case AST_CONTINUE:
        if (an->lacos == 0) {
            sem_erro(an, "SEM010", comando,
                     sem_format("%s só pode ocorrer dentro de um laço.",
                                comando->kind == AST_BREAK ? "break" : "continue"));
        }
        break;
    case AST_PRINT: {
        SemTipo tipo = sem_valor(an, comando->a, "argumento de print");
        if (tipo.vetor) {
            char *texto = sem_texto(comando->a);
            sem_erro(an, "SEM014", sem_inicio(comando->a),
                     sem_format("print não aceita o vetor “%s” inteiro; imprima um elemento "
                                "por vez.",
                                texto));
            free(texto);
        }
        break;
    }
    case AST_READ:
        sem_destino(an, comando->a, "leitura");
        break;
    default:
        fprintf(stderr, "erro interno: comando desconhecido (%d)\n", (int)comando->kind);
        exit(1);
    }
}

/* -- expressões ----------------------------------------------------- */

/* Reporta SEM012 se `tipo` vier de uma chamada void; diz se reportou. */
static int sem_sem_valor(Analisador *an, const Ast *expressao, SemTipo tipo,
                         const char *contexto) {
    if (!sem_igual(tipo, T_VOID)) {
        return 0;
    }
    sem_erro(an, "SEM012", sem_inicio(expressao),
             sem_format("Função “%s” não produz valor (retorno void) e não pode ser usada "
                        "como %s.",
                        expressao->a->text, contexto));
    return 1;
}

/* Tipo de uma expressão usada como valor: chamada void é SEM012. */
static SemTipo sem_valor(Analisador *an, const Ast *expressao, const char *contexto) {
    SemTipo tipo = sem_tipo_de(an, expressao);
    if (sem_sem_valor(an, expressao, tipo, contexto)) {
        return T_ERRO;
    }
    return tipo;
}

static Simbolo *sem_resolver(Analisador *an, const Ast *id) {
    Simbolo *s = tabela_buscar(&an->tabela, id->text);
    if (s == NULL) {
        sem_erro(an, "SEM001", id,
                 sem_format("Identificador “%s” não declarado neste escopo.", id->text));
    }
    return s;
}

static SemTipo sem_identificador(Analisador *an, const Ast *no) {
    Simbolo *s = sem_resolver(an, no);
    if (s == NULL) {
        return T_ERRO;
    }
    if (s->categoria == CAT_FUNCAO) {
        sem_erro(an, "SEM014", no,
                 sem_format("A função “%s” só pode ser usada em chamada.", no->text));
        return T_ERRO;
    }
    return sem_tipo(s->tipo, s->eh_vetor);
}

static SemTipo sem_indexacao(Analisador *an, const Ast *no) {
    SemTipo base_tipo = T_ERRO;
    Simbolo *s = NULL;
    if (no->a->kind == AST_ID) {
        s = sem_resolver(an, no->a);
        if (s != NULL) {
            if (s->eh_vetor) {
                base_tipo = sem_tipo(s->tipo, 1);
            } else {
                sem_erro(an, "SEM014", no->a,
                         sem_format("“%s” não é um vetor e não pode ser indexado; foi declarado "
                                    "como %s na linha %d, coluna %d.",
                                    s->nome, categoria_nome(s->categoria), s->linha, s->coluna));
            }
        }
    } else {
        SemTipo tipo = sem_valor(an, no->a, "operando");
        if (tipo.vetor) {
            base_tipo = tipo;
        } else if (!sem_e_erro(tipo)) {
            char *texto = sem_texto(no->a);
            sem_erro(an, "SEM014", sem_inicio(no->a),
                     sem_format("A expressão “%s” não é um vetor e não pode ser indexada.",
                                texto));
            free(texto);
        }
    }

    SemTipo indice = sem_valor(an, no->b, "índice");
    long long valor;
    if (!sem_e_erro(indice) && !sem_compativel(indice, T_INT)) {
        char *base = sem_texto(no->a);
        char *nome = sem_nome_tipo(indice);
        char *texto = sem_texto(no->b);
        sem_erro(an, "SEM006", sem_inicio(no->b),
                 sem_format("Índice do vetor “%s” deve ser int; recebeu %s (expressão “%s”).",
                            base, nome, texto));
        free(base);
        free(nome);
        free(texto);
    } else if (s != NULL && s->eh_vetor && s->tem_tamanho &&
               sem_constante_inteira(no->b, &valor) && !(0 <= valor && valor < s->tamanho)) {
        char *texto = sem_texto(no->b);
        sem_erro(an, "SEM016", sem_inicio(no->b),
                 sem_format("Índice “%s” fora dos limites do vetor “%s” (tamanho %lld).", texto,
                            s->nome, s->tamanho));
        free(texto);
    }

    return sem_e_erro(base_tipo) ? T_ERRO : sem_tipo(base_tipo.base, 0);
}

static SemTipo sem_chamada(Analisador *an, const Ast *no) {
    Simbolo *funcao = NULL;
    if (no->a->kind != AST_ID) {
        if (!sem_e_erro(sem_tipo_de(an, no->a))) {
            char *texto = sem_texto(no->a);
            sem_erro(an, "SEM014", sem_inicio(no->a),
                     sem_format("A expressão “%s” não designa uma função.", texto));
            free(texto);
        }
    } else {
        Simbolo *s = sem_resolver(an, no->a);
        if (s != NULL && s->categoria != CAT_FUNCAO) {
            sem_erro(an, "SEM014", no->a,
                     sem_format("“%s” não designa uma função; foi declarado como %s na linha "
                                "%d, coluna %d.",
                                s->nome, categoria_nome(s->categoria), s->linha, s->coluna));
        } else if (s != NULL) {
            funcao = s;
        }
    }

    size_t recebidos = no->items_len;
    SemTipo *tipos = malloc((recebidos ? recebidos : 1) * sizeof(SemTipo));
    if (!tipos) {
        fprintf(stderr, "erro: memória insuficiente na análise semântica\n");
        exit(1);
    }
    for (size_t i = 0; i < recebidos; i++) {
        tipos[i] = sem_tipo_de(an, no->items[i]);
    }

    if (funcao == NULL) {
        for (size_t i = 0; i < recebidos; i++) {
            sem_sem_valor(an, no->items[i], tipos[i], "argumento");
        }
        free(tipos);
        return T_ERRO;
    }

    const Ast *definicao = funcao->funcao;
    size_t esperados = definicao->items_len;
    if (esperados != recebidos) {
        sem_erro(an, "SEM007", sem_inicio(no),
                 sem_format("“%s” espera %zu argumento%s, mas recebeu %zu.", funcao->nome,
                            esperados, esperados == 1 ? "" : "s", recebidos));
        for (size_t i = 0; i < recebidos; i++) {
            sem_sem_valor(an, no->items[i], tipos[i], "argumento");
        }
    } else {
        for (size_t i = 0; i < recebidos; i++) {
            const Ast *arg = no->items[i];
            if (sem_sem_valor(an, arg, tipos[i], "argumento") || sem_e_erro(tipos[i])) {
                continue;
            }
            const Ast *param = definicao->items[i];
            SemTipo esperado = sem_tipo(param->type_name, param->is_array);
            int aceito = param->is_array ? sem_igual(tipos[i], esperado)
                                         : sem_compativel(tipos[i], esperado);
            if (!aceito) {
                char *nome_esperado = sem_nome_tipo(esperado);
                char *nome_recebido = sem_nome_tipo(tipos[i]);
                char *texto = sem_texto(arg);
                sem_erro(an, "SEM008", sem_inicio(arg),
                         sem_format("Argumento %zu de “%s”: esperado %s, recebido %s "
                                    "(expressão “%s”).",
                                    i + 1, funcao->nome, nome_esperado, nome_recebido, texto));
                free(nome_esperado);
                free(nome_recebido);
                free(texto);
            }
        }
    }
    free(tipos);

    return strcmp(funcao->tipo, "void") == 0 ? T_VOID : sem_tipo(funcao->tipo, 0);
}

static SemTipo sem_unario(Analisador *an, const Ast *no) {
    SemTipo tipo = sem_valor(an, no->a, "operando");
    if (sem_e_erro(tipo)) {
        return T_ERRO;
    }
    const char *exigencia;
    if (strcmp(no->text, "-") == 0) {
        if (sem_numerico(tipo)) {
            return sem_igual(tipo, T_FLOAT) ? T_FLOAT : T_INT;
        }
        exigencia = "operando numérico";
    } else { /* "!" */
        if (sem_igual(tipo, T_BOOL)) {
            return T_BOOL;
        }
        exigencia = "operando bool";
    }
    char *nome = sem_nome_tipo(tipo);
    sem_erro(an, "SEM004", no,
             sem_format("Operador %s requer %s; recebeu %s.", no->text, exigencia, nome));
    free(nome);
    return T_ERRO;
}

static SemTipo sem_binario(Analisador *an, const Ast *no) {
    SemTipo esquerda = sem_valor(an, no->a, "operando");
    SemTipo direita = sem_valor(an, no->b, "operando");
    if (sem_e_erro(esquerda) || sem_e_erro(direita)) {
        return T_ERRO;
    }

    const char *op = no->text;
    int valido;
    SemTipo resultado;
    const char *exigencia;
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || strcmp(op, "*") == 0 ||
        strcmp(op, "/") == 0) {
        valido = sem_numerico(esquerda) && sem_numerico(direita);
        resultado =
            sem_igual(esquerda, T_FLOAT) || sem_igual(direita, T_FLOAT) ? T_FLOAT : T_INT;
        exigencia = "operandos numéricos";
    } else if (strcmp(op, "%") == 0) {
        valido = sem_inteiro(esquerda) && sem_inteiro(direita);
        resultado = T_INT;
        exigencia = "operandos int";
    } else if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || strcmp(op, "<=") == 0 ||
               strcmp(op, ">=") == 0) {
        valido = sem_numerico(esquerda) && sem_numerico(direita);
        resultado = T_BOOL;
        exigencia = "operandos numéricos";
    } else if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
        valido = (sem_numerico(esquerda) && sem_numerico(direita)) ||
                 (sem_igual(esquerda, T_BOOL) && sem_igual(direita, T_BOOL));
        resultado = T_BOOL;
        exigencia = "operandos de tipos compatíveis";
    } else { /* "&&", "||" */
        valido = sem_igual(esquerda, T_BOOL) && sem_igual(direita, T_BOOL);
        resultado = T_BOOL;
        exigencia = "operandos bool";
    }

    if (!valido) {
        char *nome_esquerda = sem_nome_tipo(esquerda);
        char *nome_direita = sem_nome_tipo(direita);
        sem_erro(an, "SEM004", no,
                 sem_format("Operador %s requer %s; recebeu %s e %s.", op, exigencia,
                            nome_esquerda, nome_direita));
        free(nome_esquerda);
        free(nome_direita);
        return T_ERRO;
    }

    if ((strcmp(op, "/") == 0 || strcmp(op, "%") == 0) && sem_zero_constante(no->b)) {
        char *texto = sem_texto(no->b);
        sem_erro(an, "SEM015", sem_inicio(no->b),
                 sem_format("%s por zero: o divisor é a constante “%s”.",
                            strcmp(op, "/") == 0 ? "Divisão" : "Resto de divisão", texto));
        free(texto);
    }
    return resultado;
}

/* Tipo de um destino de atribuição/leitura, se ele for atribuível (valor-L). */
static SemTipo sem_destino(Analisador *an, const Ast *alvo, const char *comando) {
    if (alvo->kind == AST_INDEX) {
        return sem_tipo_de(an, alvo);
    }
    if (alvo->kind == AST_ID) {
        Simbolo *s = sem_resolver(an, alvo);
        if (s == NULL) {
            return T_ERRO;
        }
        char *motivo;
        if (s->categoria == CAT_FUNCAO) {
            motivo = sem_format("a função “%s” não designa uma variável ou elemento de vetor.",
                                alvo->text);
        } else if (s->eh_vetor) {
            motivo = sem_format("“%s” é um vetor, e só seus elementos (“%s[i]”) podem receber "
                                "valores.",
                                alvo->text, alvo->text);
        } else {
            return sem_tipo(s->tipo, 0);
        }
        sem_erro(an, "SEM013", alvo,
                 sem_format("Destino de %s não é atribuível; %s", comando, motivo));
        free(motivo);
        return T_ERRO;
    }

    char *descricao;
    if (alvo->kind == AST_LIT) {
        descricao = sem_format("%s “%s”", sem_descricao_literal(alvo->type_name), alvo->text);
    } else {
        sem_tipo_de(an, alvo); /* erros próprios da expressão (nome não declarado, ...) */
        char *texto = sem_texto(alvo);
        descricao = sem_format(alvo->kind == AST_CALL ? "a chamada “%s”" : "a expressão “%s”",
                               texto);
        free(texto);
    }
    sem_erro(an, "SEM013", sem_inicio(alvo),
             sem_format("Destino de %s não é atribuível; %s não designa uma variável ou "
                        "elemento de vetor.",
                        comando, descricao));
    free(descricao);
    return T_ERRO;
}

static SemTipo sem_atribuicao(Analisador *an, const Ast *no) {
    SemTipo destino = sem_destino(an, no->a, "atribuição");
    SemTipo valor = sem_valor(an, no->b, "expressão de atribuição");
    char *alvo = sem_texto(no->a);
    sem_exigir_compatibilidade(an, valor, destino, alvo, no->b);
    free(alvo);
    return destino;
}

/* Síntese do tipo de uma expressão (seção 5 da apostila). */
static SemTipo sem_tipo_de(Analisador *an, const Ast *no) {
    switch (no->kind) {
    case AST_LIT:
        return sem_tipo_literal(no->type_name);
    case AST_ID:
        return sem_identificador(an, no);
    case AST_INDEX:
        return sem_indexacao(an, no);
    case AST_CALL:
        return sem_chamada(an, no);
    case AST_UNARY:
        return sem_unario(an, no);
    case AST_BINARY:
        return sem_binario(an, no);
    case AST_ASSIGN:
        return sem_atribuicao(an, no);
    default:
        fprintf(stderr, "erro interno: nó %d não é expressão\n", (int)no->kind);
        exit(1);
    }
}

/* ------------------------------------------------------------------ *
 * Interface pública
 * ------------------------------------------------------------------ */

static int sem_comparar(const void *a, const void *b) {
    const Diagnostico *x = a;
    const Diagnostico *y = b;
    if (x->linha != y->linha) return x->linha < y->linha ? -1 : 1;
    if (x->coluna != y->coluna) return x->coluna < y->coluna ? -1 : 1;
    if (x->ordem != y->ordem) return x->ordem < y->ordem ? -1 : 1;
    return 0;
}

void semantico_analisar(const Ast *programa, Diagnosticos *saida) {
    Analisador an;
    tabela_init(&an.tabela);
    an.diagnosticos = saida;
    an.funcao = NULL;
    an.lacos = 0;

    sem_coletar_globais(&an, programa);
    for (size_t i = 0; i < programa->items_len; i++) {
        const Ast *item = programa->items[i];
        if (item->kind == AST_FUNCTION) {
            sem_funcao(&an, item);
        } else if (item->kind == AST_VAR_DECL) {
            sem_variavel_global(&an, item);
        } else {
            /* Comando solto no topo: verificado no escopo global, fora de
             * função e de laço. */
            sem_comando(&an, item);
        }
    }
    tabela_free(&an.tabela);

    /* qsort não é estável; o campo `ordem` faz o papel da estabilidade do
     * sorted() do Python. */
    if (saida->len > 1) {
        qsort(saida->itens, saida->len, sizeof(Diagnostico), sem_comparar);
    }
}

void diagnosticos_free(Diagnosticos *diagnosticos) {
    for (size_t i = 0; i < diagnosticos->len; i++) {
        free(diagnosticos->itens[i].mensagem);
    }
    free(diagnosticos->itens);
    diagnosticos->itens = NULL;
    diagnosticos->len = diagnosticos->cap = 0;
}

char *diagnostico_to_string(const Diagnostico *diagnostico) {
    return sem_format("%s — linha %d, coluna %d: %s", diagnostico->codigo, diagnostico->linha,
                      diagnostico->coluna, diagnostico->mensagem);
}

char *semantico_relatorio(const Diagnosticos *diagnosticos) {
    char *texto = sem_format("%s", "");
    for (size_t i = 0; i < diagnosticos->len; i++) {
        char *linha = diagnostico_to_string(&diagnosticos->itens[i]);
        char *junto = sem_format("%s%s\r\n", texto, linha);
        free(texto);
        free(linha);
        texto = junto;
    }
    size_t n = diagnosticos->len;
    char *final = sem_format("%sAnálise semântica concluída: %zu erro%s; %s.", texto, n,
                             n == 1 ? "" : "s", n ? "programa rejeitado" : "programa aceito");
    free(texto);
    return final;
}
