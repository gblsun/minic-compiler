# Testes da etapa 3 — objetivo, código e resultado

> Gerado por [`tests/gerar_catalogo_semantico.py`](../tests/gerar_catalogo_semantico.py)
> a partir dos próprios arquivos de teste. Não edite à mão: rode o script.

As orientações da AP 2 pedem os **testes propostos com os respectivos
resultados**. Este catálogo mostra cada teste e a saída que as duas
implementações produzem, idêntica byte a byte. As linhas do relatório são
separadas por CRLF na saída real; aqui aparecem como linhas normais.

Como conferir: `python tests/run_semantic_tests.py` (as duas versões e a
equivalência) ou os scripts oficiais da AP 2 (ver
[entrega-ap2.md](entrega-ap2.md)).

## Resumo

| Teste | Resultado | Diagnósticos |
|---|---|---|
| [oficial 01_promocao_numerica](#oficial-01_promocao_numerica) | aceito | — |
| [oficial 02_sombreamento_lexico](#oficial-02_sombreamento_lexico) | aceito | — |
| [oficial 03_logica_e_comparacoes](#oficial-03_logica_e_comparacoes) | aceito | — |
| [oficial 04_vetores_e_parametro_array](#oficial-04_vetores_e_parametro_array) | aceito | — |
| [oficial 05_lacos_aninhados](#oficial-05_lacos_aninhados) | aceito | — |
| [oficial 06_recursao_e_retorno](#oficial-06_recursao_e_retorno) | aceito | — |
| [oficial 07_void_como_comando](#oficial-07_void_como_comando) | aceito | — |
| [oficial 08_retornos_nos_dois_ramos](#oficial-08_retornos_nos_dois_ramos) | aceito | — |
| [oficial 09_atribuicao_associativa](#oficial-09_atribuicao_associativa) | aceito | — |
| [oficial 10_vetor_bool_e_funcoes](#oficial-10_vetor_bool_e_funcoes) | aceito | — |
| [oficial 11_estreitamento_implicito](#oficial-11_estreitamento_implicito) | rejeitado | SEM003 |
| [oficial 12_identificador_nao_declarado](#oficial-12_identificador_nao_declarado) | rejeitado | SEM001 |
| [oficial 13_declaracao_duplicada](#oficial-13_declaracao_duplicada) | rejeitado | SEM002 |
| [oficial 14_condicao_nao_booleana](#oficial-14_condicao_nao_booleana) | rejeitado | SEM005 |
| [oficial 15_indice_float](#oficial-15_indice_float) | rejeitado | SEM006 |
| [oficial 16_aridade_incorreta](#oficial-16_aridade_incorreta) | rejeitado | SEM007 |
| [oficial 17_tipos_de_argumentos](#oficial-17_tipos_de_argumentos) | rejeitado | SEM008, SEM008 |
| [oficial 18_uso_de_void_como_valor](#oficial-18_uso_de_void_como_valor) | rejeitado | SEM012 |
| [oficial 19_retorno_e_cobertura](#oficial-19_retorno_e_cobertura) | rejeitado | SEM009, SEM011 |
| [oficial 20_destino_nao_atribuivel](#oficial-20_destino_nao_atribuivel) | rejeitado | SEM013 |
| [aceito_programa_completo](#aceito_programa_completo) | aceito | — |
| [aceito_recursao_mutua](#aceito_recursao_mutua) | aceito | — |
| [aceito_spec_13_1_programa_valido](#aceito_spec_13_1_programa_valido) | aceito | — |
| [aceito_spec_19_1_fatorial](#aceito_spec_19_1_fatorial) | aceito | — |
| [aceito_spec_19_2_entrada_vetor_repeticao](#aceito_spec_19_2_entrada_vetor_repeticao) | aceito | — |
| [erro_break_continue_return_fora](#erro_break_continue_return_fora) | rejeitado | SEM010, SEM010, SEM010 |
| [erro_cascata_e_multiplos](#erro_cascata_e_multiplos) | rejeitado | SEM001, SEM001, SEM001, SEM001, SEM003, SEM005, SEM005 |
| [erro_categoria](#erro_categoria) | rejeitado | SEM014, SEM014, SEM014, SEM013, SEM013, SEM014, SEM013 |
| [erro_chamadas](#erro_chamadas) | rejeitado | SEM007, SEM007, SEM008, SEM008, SEM012, SEM012, SEM012, SEM012, SEM009 |
| [erro_char_e_conversoes](#erro_char_e_conversoes) | rejeitado | SEM003, SEM003, SEM003, SEM003 |
| [erro_colunas_apos_utf8](#erro_colunas_apos_utf8) | rejeitado | SEM003, SEM001, SEM003 |
| [erro_destinos](#erro_destinos) | rejeitado | SEM013, SEM013, SEM013, SEM013, SEM013 |
| [erro_divisao_por_zero](#erro_divisao_por_zero) | rejeitado | SEM015, SEM015, SEM015 |
| [erro_escopos_e_duplicidade](#erro_escopos_e_duplicidade) | rejeitado | SEM002, SEM002, SEM002, SEM001, SEM002, SEM001 |
| [erro_lexico_antes_da_semantica](#erro_lexico_antes_da_semantica) | erro léxico | — |
| [erro_operadores](#erro_operadores) | rejeitado | SEM004, SEM004, SEM004, SEM004, SEM004, SEM004, SEM004 |
| [erro_retornos](#erro_retornos) | rejeitado | SEM009, SEM009, SEM009, SEM011, SEM011, SEM011, SEM001 |
| [erro_sintatico_antes_da_semantica](#erro_sintatico_antes_da_semantica) | erro sintático | — |
| [erro_vetor_tamanho_e_limites](#erro_vetor_tamanho_e_limites) | rejeitado | SEM016, SEM016, SEM016, SEM016, SEM016 |

## Casos inválidos obrigatórios da especificação (Seção 13.2)

| Caso exigido | Onde está coberto |
|---|---|
| variável não declarada | oficial 12; `erro_cascata_e_multiplos`, `erro_escopos_e_duplicidade` |
| declaração duplicada | oficial 13; `erro_escopos_e_duplicidade` |
| tipos incompatíveis | oficiais 11 e 17; `erro_char_e_conversoes`, `erro_operadores` |
| chamada com número incorreto de argumentos | oficial 16; `erro_chamadas` |
| `break` fora de laço | `erro_break_continue_return_fora` |
| retorno incompatível | oficial 19; `erro_retornos` |
| acesso de vetor com índice não inteiro | oficial 15 |

Os casos léxicos e sintáticos da mesma lista são cobertos pelas suítes das
etapas 1 e 2 (`tests/inputs/`, `tests/parser_inputs/` e os 50 casos oficiais
do parser).

## Casos oficiais da AP 2

Entrada e gabarito de `ref/testes-oficiais/testes-semanticos-20/`. A
explicação de cada caso é a do
[GABARITO.md](../ref/testes-oficiais/testes-semanticos-20/GABARITO.md).

### oficial 01_promocao_numerica

```c
float soma(float a, float b) {
    return a + b;
}
int principal() {
    int n;
    float r;
    n = 3;
    r = soma(n, 2);
    return n;
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

### oficial 02_sombreamento_lexico

```c
int principal() {
    int x;
    x = 4;
    {
        float x;
        x = 2.5;
    }
    x = x + 1;
    return x;
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

### oficial 03_logica_e_comparacoes

```c
bool menor(int a, int b) {
    return a < b;
}
int principal() {
    int x;
    bool ok;
    x = 1;
    ok = menor(x, 3) && !(x == 0);
    if (ok) x = x + 1;
    return x;
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

### oficial 04_vetores_e_parametro_array

```c
float media(int dados[]) {
    float s;
    int i;
    s = 0;
    i = 0;
    while (i < 3) {
        s = s + dados[i];
        i = i + 1;
    }
    return s;
}
int principal() {
    int xs[3];
    float r;
    xs[0] = 1;
    xs[1] = 2;
    xs[2] = 3;
    r = media(xs);
    return 0;
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

### oficial 05_lacos_aninhados

```c
int principal() {
    int i;
    int j;
    int total;
    i = 0;
    total = 0;
    while (i < 3) {
        j = 0;
        while (j < 2) {
            total = total + i + j;
            j = j + 1;
        }
        i = i + 1;
    }
    return total;
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

### oficial 06_recursao_e_retorno

```c
int fatorial(int n) {
    if (n == 0) return 1;
    else return n * fatorial(n - 1);
}
int principal() {
    return fatorial(5);
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

### oficial 07_void_como_comando

```c
void inicializa(int dados[]) {
    dados[0] = 7;
    return;
}
int principal() {
    int v[1];
    inicializa(v);
    return v[0];
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

### oficial 08_retornos_nos_dois_ramos

```c
int sinal(int n) {
    if (n < 0) return -1;
    else return 1;
}
int principal() {
    return sinal(0);
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

### oficial 09_atribuicao_associativa

```c
int principal() {
    int a;
    int b;
    a = b = 3;
    return a + b;
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

### oficial 10_vetor_bool_e_funcoes

```c
bool contem(int dados[], int alvo) {
    int i;
    i = 0;
    while (i < 2) {
        if (dados[i] == alvo) return 1 < 2;
        i = i + 1;
    }
    return 2 < 1;
}
int principal() {
    int valores[2];
    bool achou;
    valores[0] = 4;
    valores[1] = 9;
    achou = contem(valores, 9);
    if (achou) return 1;
    else return 0;
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

### oficial 11_estreitamento_implicito

```c
int principal() {
    int n;
    n = 2.5;
    return 0;
}
```

Saída (stdout):

```text
SEM003 — linha 3, coluna 9: Não é possível atribuir float a int sem conversão permitida (destino “n”; expressão “2.5”).
Análise semântica concluída: 1 erro; programa rejeitado.
```

### oficial 12_identificador_nao_declarado

```c
int principal() {
    int n;
    n = ausente + 1;
    return 0;
}
```

Saída (stdout):

```text
SEM001 — linha 3, coluna 9: Identificador “ausente” não declarado neste escopo.
Análise semântica concluída: 1 erro; programa rejeitado.
```

### oficial 13_declaracao_duplicada

```c
int principal() {
    int medida;
    float medida;
    return 0;
}
```

Saída (stdout):

```text
SEM002 — linha 3, coluna 11: “medida” já declarado neste escopo; declaração anterior na linha 2, coluna 9 (tipo int).
Análise semântica concluída: 1 erro; programa rejeitado.
```

### oficial 14_condicao_nao_booleana

```c
int principal() {
    int n;
    n = 0;
    while (n) n = n + 1;
    return 0;
}
```

Saída (stdout):

```text
SEM005 — linha 4, coluna 12: Condição de while deve ter tipo bool; recebeu int (expressão “n”).
Análise semântica concluída: 1 erro; programa rejeitado.
```

### oficial 15_indice_float

```c
int principal() {
    int dados[3];
    dados[1.0] = 7;
    return 0;
}
```

Saída (stdout):

```text
SEM006 — linha 3, coluna 11: Índice do vetor “dados” deve ser int; recebeu float (expressão “1.0”).
Análise semântica concluída: 1 erro; programa rejeitado.
```

### oficial 16_aridade_incorreta

```c
int combina(int a, float b) {
    return a;
}
int principal() {
    return combina(1);
}
```

Saída (stdout):

```text
SEM007 — linha 5, coluna 12: “combina” espera 2 argumentos, mas recebeu 1.
Análise semântica concluída: 1 erro; programa rejeitado.
```

### oficial 17_tipos_de_argumentos

```c
int combina(int a, float b) {
    return a;
}
int principal() {
    return combina(1 < 2, 3 < 4);
}
```

Saída (stdout):

```text
SEM008 — linha 5, coluna 20: Argumento 1 de “combina”: esperado int, recebido bool (expressão “1 < 2”).
SEM008 — linha 5, coluna 27: Argumento 2 de “combina”: esperado float, recebido bool (expressão “3 < 4”).
Análise semântica concluída: 2 erros; programa rejeitado.
```

### oficial 18_uso_de_void_como_valor

```c
void acao() {
    return;
}
int principal() {
    int n;
    n = acao();
    return 0;
}
```

Saída (stdout):

```text
SEM012 — linha 6, coluna 9: Função “acao” não produz valor (retorno void) e não pode ser usada como expressão de atribuição.
Análise semântica concluída: 1 erro; programa rejeitado.
```

### oficial 19_retorno_e_cobertura

```c
int fracionario() {
    return 2.5;
}
int incompleta(bool condicao) {
    if (condicao) return 1;
}
int principal() {
    return 0;
}
```

Saída (stdout):

```text
SEM009 — linha 2, coluna 12: Retorno float incompatível com o tipo int da função “fracionario”; conversão implícita de float para int não permitida.
SEM011 — linha 4, coluna 1: A função “incompleta” pode terminar sem retornar int; o ramo em que “condicao” é falso alcança o fim do corpo.
Análise semântica concluída: 2 erros; programa rejeitado.
```

### oficial 20_destino_nao_atribuivel

```c
int principal() {
    int n;
    3 = n;
    return 0;
}
```

Saída (stdout):

```text
SEM013 — linha 3, coluna 5: Destino de atribuição não é atribuível; o literal inteiro “3” não designa uma variável ou elemento de vetor.
Análise semântica concluída: 1 erro; programa rejeitado.
```

## Casos próprios

Entradas de `tests/semantic_inputs/`, golden de `tests/semantic_expected/`.

### aceito_programa_completo

**Objetivo.** Programa válido que usa todo o núcleo da linguagem: global com inicializador, funções, vetor, `for`, `while` com `break`/`continue`, promoção `int → float`, `char → int`, `read`, `print` de expressão e de cadeia, `%`.

```c
int limite = 10;
int soma(int a, int b) {
    return a + b;
}
float promove(int n) {
    return n;
}
int main() {
    int valores[3];
    int i;
    int total = 0;
    float x;
    bool ok;
    char letra = 'A';
    for (i = 0; i < 3; i = i + 1) {
        valores[i] = i * 2;
        total = total + valores[i];
    }
    x = promove(total) + 0.5;
    ok = x > 2.0 && !(total == limite);
    while (ok && i < limite) {
        i = i + 1;
        if (i == 7) continue;
        if (i > 8) break;
    }
    total = total + letra;
    read(valores[0]);
    print(soma(total, 1));
    print("fim");
    return total % 3;
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

Código de saída: `0`.

### aceito_recursao_mutua

**Objetivo.** Recursão mútua: `par` chama `impar`, que é definida depois. Só passa porque as assinaturas são coletadas antes dos corpos (passagem A). Também cobre `if`/`else` com `return` nos dois ramos (um deles dentro de bloco).

```c
bool par(int n) {
    if (n == 0) return true;
    return impar(n - 1);
}
bool impar(int n) {
    if (n == 0) return false;
    return par(n - 1);
}
int main() {
    if (par(4)) return 1;
    else {
        return 0;
    }
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

Código de saída: `0`.

### aceito_spec_13_1_programa_valido

**Objetivo.** O programa válido completo da Seção 13.1 da especificação.

```c
int soma(int a, int b) {
    return a + b;
}

int main() {
    int valores[3];
    int i;
    int total = 0;
    for (i = 0; i < 3; i = i + 1) {
        valores[i] = i * 2;
        total = total + valores[i];
    }
    print(soma(total, 1));
    return 0;
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

Código de saída: `0`.

### aceito_spec_19_1_fatorial

**Objetivo.** Programa de referência 19.1 da especificação (fatorial recursivo). O corpo termina com `return` depois de um `if` sem `else`, então não há queda.

```c
int fatorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * fatorial(n - 1);
}

int main() {
    print(fatorial(5));
    return 0;
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

Código de saída: `0`.

### aceito_spec_19_2_entrada_vetor_repeticao

**Objetivo.** Programa de referência 19.2 da especificação: `read` em elemento de vetor dentro de `for`.

```c
int main() {
    int valores[5];
    int i;
    int soma = 0;
    for (i = 0; i < 5; i = i + 1) {
        read(valores[i]);
        soma = soma + valores[i];
    }
    print(soma);
    return 0;
}
```

Saída (stdout):

```text
Análise semântica concluída: 0 erros; programa aceito.
```

Código de saída: `0`.

### erro_break_continue_return_fora

**Objetivo.** SEM010 nas três formas: `return` no topo do arquivo, `break` e `continue` fora de laço. Os mesmos comandos dentro do `while` são aceitos.

```c
int x;
return 1;
void f() {
    break;
    while (true) {
        if (true) continue;
        break;
    }
    continue;
}
int main() {
    return 0;
}
```

Saída (stdout):

```text
SEM010 — linha 2, coluna 1: return só pode ocorrer dentro de uma função.
SEM010 — linha 4, coluna 5: break só pode ocorrer dentro de um laço.
SEM010 — linha 9, coluna 5: continue só pode ocorrer dentro de um laço.
Análise semântica concluída: 3 erros; programa rejeitado.
```

Código de saída: `4`.

### erro_cascata_e_multiplos

**Objetivo.** Vários erros independentes no mesmo programa, em ordem de posição, e a supressão de cascata: `(ausente + 1) * true` gera só o SEM001, sem o SEM004 derivado. `if (n = 1)` mostra que a atribuição tem o tipo do destino.

```c
int main() {
    int n;
    n = ausente + 1 * outro;
    n = ausente(2);
    n = (ausente + 1) * true;
    n = 2.5;
    while (n) n = n - 1;
    if (n = 1) n = 2;
    return 0;
}
```

Saída (stdout):

```text
SEM001 — linha 3, coluna 9: Identificador “ausente” não declarado neste escopo.
SEM001 — linha 3, coluna 23: Identificador “outro” não declarado neste escopo.
SEM001 — linha 4, coluna 9: Identificador “ausente” não declarado neste escopo.
SEM001 — linha 5, coluna 10: Identificador “ausente” não declarado neste escopo.
SEM003 — linha 6, coluna 9: Não é possível atribuir float a int sem conversão permitida (destino “n”; expressão “2.5”).
SEM005 — linha 7, coluna 12: Condição de while deve ter tipo bool; recebeu int (expressão “n”).
SEM005 — linha 8, coluna 9: Condição de if deve ter tipo bool; recebeu int (expressão “n = 1”).
Análise semântica concluída: 7 erros; programa rejeitado.
```

Código de saída: `4`.

### erro_categoria

**Objetivo.** SEM013 e SEM014, uso de um nome fora da sua categoria: chamar variável, indexar escalar, função como valor, atribuir a vetor inteiro e a função, `print` e `read` de vetor inteiro.

```c
int dobro(int n) {
    return n * 2;
}
int main() {
    int x;
    int v[2];
    x = 1;
    x = x(3);
    x = x[0];
    x = dobro;
    v = 3;
    dobro = 4;
    print(v);
    read(v);
    return 0;
}
```

Saída (stdout):

```text
SEM014 — linha 8, coluna 9: “x” não designa uma função; foi declarado como variável na linha 5, coluna 9.
SEM014 — linha 9, coluna 9: “x” não é um vetor e não pode ser indexado; foi declarado como variável na linha 5, coluna 9.
SEM014 — linha 10, coluna 9: A função “dobro” só pode ser usada em chamada.
SEM013 — linha 11, coluna 5: Destino de atribuição não é atribuível; “v” é um vetor, e só seus elementos (“v[i]”) podem receber valores.
SEM013 — linha 12, coluna 5: Destino de atribuição não é atribuível; a função “dobro” não designa uma variável ou elemento de vetor.
SEM014 — linha 13, coluna 11: print não aceita o vetor “v” inteiro; imprima um elemento por vez.
SEM013 — linha 14, coluna 10: Destino de leitura não é atribuível; “v” é um vetor, e só seus elementos (“v[i]”) podem receber valores.
Análise semântica concluída: 7 erros; programa rejeitado.
```

Código de saída: `4`.

### erro_chamadas

**Objetivo.** SEM007 com argumentos a menos e a mais; SEM008 com vetor esperado e escalar recebido, e com `float` para `int`; SEM012 nos contextos de argumento, operando, condição e argumento de `print`; SEM009 no `return` de uma chamada `float` em função `int`.

```c
void acao(int x) {
}
float media(int dados[], int n) {
    return 1.0;
}
int main() {
    int v[3];
    float m;
    int n;
    m = media(v);
    m = media(v, 3, 4);
    m = media(n, 3);
    m = media(v, 2.5);
    acao(acao(1));
    n = acao(1) + 1;
    if (acao(2)) n = 1;
    print(acao(3));
    return media(v, 3);
}
```

Saída (stdout):

```text
SEM007 — linha 10, coluna 9: “media” espera 2 argumentos, mas recebeu 1.
SEM007 — linha 11, coluna 9: “media” espera 2 argumentos, mas recebeu 3.
SEM008 — linha 12, coluna 15: Argumento 1 de “media”: esperado vetor de int, recebido int (expressão “n”).
SEM008 — linha 13, coluna 18: Argumento 2 de “media”: esperado int, recebido float (expressão “2.5”).
SEM012 — linha 14, coluna 10: Função “acao” não produz valor (retorno void) e não pode ser usada como argumento.
SEM012 — linha 15, coluna 9: Função “acao” não produz valor (retorno void) e não pode ser usada como operando.
SEM012 — linha 16, coluna 9: Função “acao” não produz valor (retorno void) e não pode ser usada como condição.
SEM012 — linha 17, coluna 11: Função “acao” não produz valor (retorno void) e não pode ser usada como argumento de print.
SEM009 — linha 18, coluna 12: Retorno float incompatível com o tipo int da função “main”; conversão implícita de float para int não permitida.
Análise semântica concluída: 9 erros; programa rejeitado.
```

Código de saída: `4`.

### erro_char_e_conversoes

**Objetivo.** Conversões: `char → int` é aceita (inicializador, índice, aritmética); `int → char`, `char → float` e `int → bool` não são.

```c
int main() {
    char c = 65;
    int n = 'a';
    float r = 'b';
    bool b = 0;
    int v[2];
    v['x' - 'x'] = c;
    n = c + 1;
    c = n;
    return n;
}
```

Saída (stdout):

```text
SEM003 — linha 2, coluna 14: Não é possível atribuir int a char sem conversão permitida (destino “c”; expressão “65”).
SEM003 — linha 4, coluna 15: Não é possível atribuir char a float sem conversão permitida (destino “r”; expressão “'b'”).
SEM003 — linha 5, coluna 14: Não é possível atribuir int a bool sem conversão permitida (destino “b”; expressão “0”).
SEM003 — linha 9, coluna 9: Não é possível atribuir int a char sem conversão permitida (destino “c”; expressão “n”).
Análise semântica concluída: 4 erros; programa rejeitado.
```

Código de saída: `4`.

### erro_colunas_apos_utf8

**Objetivo.** Colunas contadas em caracteres: acentos numa cadeia, num literal `'é'` e num comentário antes do erro na mesma linha. Python e C têm de apontar a mesma coluna.

```c
int main() {
    int n;
    print("ação"); n = 2.5;
    print('é'); n = ausente;
    /* comentário com acentuação */ n = true;
    return 0;
}
```

Saída (stdout):

```text
SEM003 — linha 3, coluna 24: Não é possível atribuir float a int sem conversão permitida (destino “n”; expressão “2.5”).
SEM001 — linha 4, coluna 21: Identificador “ausente” não declarado neste escopo.
SEM003 — linha 5, coluna 41: Não é possível atribuir bool a int sem conversão permitida (destino “n”; expressão “true”).
Análise semântica concluída: 3 erros; programa rejeitado.
```

Código de saída: `4`.

### erro_destinos

**Objetivo.** SEM013 para cada tipo de destino inválido: literal real, lógico e de caractere, chamada e expressão.

```c
int f() {
    return 1;
}
int main() {
    int n;
    2.5 = n;
    true = n;
    f() = n;
    n + 1 = n;
    'a' = n;
    return 0;
}
```

Saída (stdout):

```text
SEM013 — linha 6, coluna 5: Destino de atribuição não é atribuível; o literal real “2.5” não designa uma variável ou elemento de vetor.
SEM013 — linha 7, coluna 5: Destino de atribuição não é atribuível; o literal lógico “true” não designa uma variável ou elemento de vetor.
SEM013 — linha 8, coluna 5: Destino de atribuição não é atribuível; a chamada “f()” não designa uma variável ou elemento de vetor.
SEM013 — linha 9, coluna 5: Destino de atribuição não é atribuível; a expressão “n + 1” não designa uma variável ou elemento de vetor.
SEM013 — linha 10, coluna 5: Destino de atribuição não é atribuível; o literal de caractere “'a'” não designa uma variável ou elemento de vetor.
Análise semântica concluída: 5 erros; programa rejeitado.
```

Código de saída: `4`.

### erro_divisao_por_zero

**Objetivo.** SEM015: divisor constante zero em `/` e `%`, inteiro e real. `10 / 2` é aceito.

```c
int main() {
    int n;
    float r;
    n = 10 / 0;
    n = 10 % 0;
    r = 1.5 / 0.0;
    n = 10 / 2;
    return n;
}
```

Saída (stdout):

```text
SEM015 — linha 4, coluna 14: Divisão por zero: o divisor é a constante “0”.
SEM015 — linha 5, coluna 14: Resto de divisão por zero: o divisor é a constante “0”.
SEM015 — linha 6, coluna 15: Divisão por zero: o divisor é a constante “0.0”.
Análise semântica concluída: 3 erros; programa rejeitado.
```

Código de saída: `4`.

### erro_escopos_e_duplicidade

**Objetivo.** SEM002 global, entre parâmetros, entre parâmetro e local do corpo, e entre funções; sombreamento válido em bloco interno; nome que sai de escopo (SEM001); global usada antes da declaração (SEM001).

```c
int g;
float g;
int f(int a, int a) {
    int a;
    {
        int a;
        float b;
    }
    return b;
}
void f() {
}
int main() {
    int depois;
    depois = global_tardia;
    return 0;
}
int global_tardia;
```

Saída (stdout):

```text
SEM002 — linha 2, coluna 7: “g” já declarado neste escopo; declaração anterior na linha 1, coluna 5 (tipo int).
SEM002 — linha 3, coluna 18: “a” já declarado neste escopo; declaração anterior na linha 3, coluna 11 (tipo int).
SEM002 — linha 4, coluna 9: “a” já declarado neste escopo; declaração anterior na linha 3, coluna 11 (tipo int).
SEM001 — linha 9, coluna 12: Identificador “b” não declarado neste escopo.
SEM002 — linha 11, coluna 1: “f” já declarado neste escopo; declaração anterior na linha 3, coluna 1 (função que retorna int).
SEM001 — linha 15, coluna 14: Identificador “global_tardia” não declarado neste escopo.
Análise semântica concluída: 6 erros; programa rejeitado.
```

Código de saída: `4`.

### erro_lexico_antes_da_semantica

**Objetivo.** Erro léxico: a análise semântica não roda, a mensagem vai para stderr e o código de saída é 2.

```c
int main() {
    int x = 1 @ 2;
    return x;
}
```

Saída de erro (stderr):

```text
Erro léxico na linha 2, coluna 15: símbolo "@" não reconhecido.
```

Código de saída: `2`.

### erro_operadores

**Objetivo.** SEM004 para cada família de operador: aritmético, `%`, lógico, `!`, `-` unário, relacional e igualdade.

```c
int main() {
    int n;
    float r;
    bool b;
    n = true * 2;
    r = 2.5 % 2;
    b = 1 && b;
    b = !n;
    r = -b;
    b = b < 1;
    b = b == 1;
    return 0;
}
```

Saída (stdout):

```text
SEM004 — linha 5, coluna 14: Operador * requer operandos numéricos; recebeu bool e int.
SEM004 — linha 6, coluna 13: Operador % requer operandos int; recebeu float e int.
SEM004 — linha 7, coluna 11: Operador && requer operandos bool; recebeu int e bool.
SEM004 — linha 8, coluna 9: Operador ! requer operando bool; recebeu int.
SEM004 — linha 9, coluna 9: Operador - requer operando numérico; recebeu bool.
SEM004 — linha 10, coluna 11: Operador < requer operandos numéricos; recebeu bool e int.
SEM004 — linha 11, coluna 11: Operador == requer operandos de tipos compatíveis; recebeu bool e int.
Análise semântica concluída: 7 erros; programa rejeitado.
```

Código de saída: `4`.

### erro_retornos

**Objetivo.** SEM009 nas três formas (valor em `void`, `return;` em não `void`, tipo incompatível) e SEM011 com cada explicação: laço `while`, corpo sem `return`, ramo verdadeiro do `if`. A promoção `int → float` no `return` é aceita.

```c
void nada() {
    return 3;
}
int semvalor() {
    return;
}
bool logico() {
    return 1;
}
float real() {
    return 2;
}
int laco(int n) {
    while (n > 0) {
        return n;
    }
}
int vazio() {
}
int ramos(bool c) {
    if (c) {
        n = 1;
    } else return 2;
}
int main() {
    return 0;
}
```

Saída (stdout):

```text
SEM009 — linha 2, coluna 12: A função “nada” é void e não pode retornar valor (expressão “3”).
SEM009 — linha 5, coluna 5: Retorno sem valor na função “semvalor”, que deve retornar int.
SEM009 — linha 8, coluna 12: Retorno int incompatível com o tipo bool da função “logico”; conversão implícita de int para bool não permitida.
SEM011 — linha 13, coluna 1: A função “laco” pode terminar sem retornar int; o laço while da linha 14 pode terminar e alcançar o fim do corpo.
SEM011 — linha 18, coluna 1: A função “vazio” pode terminar sem retornar int; o fim do corpo é alcançado sem return.
SEM011 — linha 20, coluna 1: A função “ramos” pode terminar sem retornar int; o ramo em que “c” é verdadeiro alcança o fim do corpo.
SEM001 — linha 22, coluna 9: Identificador “n” não declarado neste escopo.
Análise semântica concluída: 7 erros; programa rejeitado.
```

Código de saída: `4`.

### erro_sintatico_antes_da_semantica

**Objetivo.** Erro sintático: a análise semântica não roda (o `y` não declarado não é reportado), a mensagem vai para stderr e o código de saída é 3.

```c
int main() {
    int x = ;
    return y;
}
```

Saída de erro (stderr):

```text
Erro de sintaxe na linha 2, coluna 13: esperado expressão (identificador, literal ou ABRE_PAREN); encontrado PONTO_E_VIRGULA.
```

Código de saída: `3`.

### erro_vetor_tamanho_e_limites

**Objetivo.** SEM016: tamanho de vetor zero e negativo; índice constante fora dos limites (acima e negativo), na escrita e na leitura. `b[2]` num vetor de 3 é aceito.

```c
int main() {
    int a[0];
    int b[3];
    int c[-2];
    int n;
    b[3] = 1;
    b[-1] = 2;
    b[2] = 3;
    n = b[5];
    return 0;
}
```

Saída (stdout):

```text
SEM016 — linha 2, coluna 11: Tamanho do vetor “a” deve ser um literal inteiro positivo; recebeu “0”.
SEM016 — linha 4, coluna 11: Tamanho do vetor “c” deve ser um literal inteiro positivo; recebeu “-2”.
SEM016 — linha 6, coluna 7: Índice “3” fora dos limites do vetor “b” (tamanho 3).
SEM016 — linha 7, coluna 7: Índice “-1” fora dos limites do vetor “b” (tamanho 3).
SEM016 — linha 9, coluna 11: Índice “5” fora dos limites do vetor “b” (tamanho 3).
Análise semântica concluída: 5 erros; programa rejeitado.
```

Código de saída: `4`.
