# Manual do compilador MINIC

Guia de uso do `minic`: como instalar, escrever um programa, compilar,
executar e entender as mensagens. É um dos entregáveis da etapa 4 (Seção 15
da especificação). Como o compilador funciona por dentro está em
[arquitetura.md](arquitetura.md), [ir.md](ir.md) e [backend.md](backend.md).

## 1. Instalação

O compilador existe em duas versões equivalentes, que produzem exatamente a
mesma saída. Basta uma delas.

| Versão | Requisito | Como chamar |
| ------ | --------- | ----------- |
| Python | Python 3.10 ou mais novo, sem bibliotecas externas | `python3 minic.py …` |
| C | um compilador C11 (`gcc` ou `clang`) | `gcc -Wall -Wextra minic.c -o minic`, depois `./minic …` |

```bash
git clone https://github.com/gblsun/minic-compiler.git
cd minic-compiler
gcc -Wall -Wextra minic.c -o minic        # compila sem nenhum aviso
```

No Windows, use `python` em vez de `python3`, e `minic.exe` é o executável que
o `gcc` gera.

## 2. Primeiro programa

Salve como `ola.c`:

```c
int main() {
    print("Olá, MINIC!");
    return 0;
}
```

E execute:

```bash
$ python3 minic.py --run ola.c
Olá, MINIC!
```

`--run` compila o programa para a máquina virtual do compilador (a MVM) e o
executa na hora. Ver [6. Execução](#6-execução).

## 3. A linguagem

A MINIC é um subconjunto de C. Esta seção resume o que dá para usar. A
referência completa está em [especificacao.md](especificacao.md) (léxico),
[gramatica.md](gramatica.md) (sintaxe) e [semantica.md](semantica.md)
(regras), e a fonte normativa é o
[PDF da especificação](../ref/especificacao-completa-minic.pdf).

### Estrutura de um programa

Um programa é uma sequência de declarações globais e de funções. A execução
começa por `main`, que tem de existir para o programa rodar.

```c
int contador = 0;            // variável global
int tabela[10];              // vetor global

int dobro(int x) {           // função com parâmetro
    return x * 2;
}

void registra() {            // função sem retorno
    contador = contador + 1;
}

int main() {
    registra();
    print(dobro(contador));
    return 0;
}
```

### Tipos

| Tipo | Valores | Literais |
| ---- | ------- | -------- |
| `int` | inteiro de 32 bits com sinal | `0`, `42`, `-7` |
| `float` | número real (precisão dupla) | `3.14`, `0.5` |
| `bool` | `true` ou `false` | `true`, `false` |
| `char` | um caractere | `'A'`, `'\n'`, `'é'` |
| `void` | ausência de valor (só como retorno de função) | — |

Vetores são unidimensionais, com tamanho fixo: `int v[10];`. Os índices vão de
0 ao tamanho − 1. Um parâmetro vetor se escreve `int v[]` e é passado por
referência.

Cadeias entre aspas (`"texto"`) só podem ser usadas no `print`.

**Conversões automáticas** são só duas: `int` → `float` e `char` → `int`.
Qualquer outra, como `float` → `int` ou `bool` → `int`, é erro.

### Comandos

```c
int x = 1, y;                 // declaração (com ou sem valor inicial)
x = y = 3;                    // atribuição (é uma expressão)
if (x > 2) { … } else { … }   // condição: tem de ser bool
while (x < 10) { … }
for (i = 0; i < 10; i = i + 1) { … }
break;  continue;             // só dentro de laços
return x;                     // em função void: return;
print(x);                     // imprime um valor e quebra a linha
read(x);                      // lê um valor da entrada
{ int x = 5; … }              // bloco com escopo próprio (pode sombrear)
```

### Expressões e precedência

Da mais forte para a mais fraca:

| Operadores | Observação |
| ---------- | ---------- |
| `()` `[]` chamada | |
| `-` `!` (unários) | `!` exige `bool` |
| `*` `/` `%` | `%` só com inteiros; `/` entre inteiros trunca em direção a zero |
| `+` `-` | |
| `<` `>` `<=` `>=` | resultado `bool` |
| `==` `!=` | |
| `&&` | curto-circuito: o lado direito só é avaliado se precisar |
| `\|\|` | idem |
| `=` | associa à direita |

### Comentários

`// até o fim da linha` e `/* bloco */`. Comentários de bloco não podem ser
aninhados.

## 4. A linha de comando

```text
minic [MODO] [-O0|-O1] [--stats] arquivo.c
```

| Modo | O que faz |
| ---- | --------- |
| *(nenhum)* ou `--check` | Analisa o programa (léxico, sintaxe e semântica) e imprime o relatório: `Análise semântica concluída: 0 erros; programa aceito.` |
| `--tokens` | Imprime os tokens, um por linha: `<IDENT, x>`, `KW_INT`, … |
| `--ast` | Imprime a árvore sintática abstrata, numa linha. |
| `--symbols` | Imprime a tabela de símbolos: categoria, tipo, nível, posição, se foi inicializada e usada, e o deslocamento no registro de ativação. |
| `--ir` | Imprime a representação intermediária (código de três endereços). |
| `--target=asm` | Imprime o código gerado para a máquina de pilha (MVM). |
| `--run` | Gera o código e o executa. A entrada do `read` vem da entrada padrão. |

| Opção | Efeito |
| ----- | ------ |
| `-O0` | sem otimização (o padrão) |
| `-O1` | otimiza a IR antes de mostrar ou gerar código ([ir.md](ir.md#otimizador--o1)) |
| `--stats` | com `--run`: informa em stderr quantas instruções a máquina executou |

Exemplos:

```bash
python3 minic.py programa.c                    # verifica o programa
python3 minic.py --ir -O1 programa.c           # vê a IR otimizada
python3 minic.py --run programa.c < dados.txt  # executa lendo de dados.txt
echo "3 4" | ./minic --run -O1 programa.c      # executa a versão otimizada
./minic --run -O1 --stats programa.c           # quanto o -O1 economizou?
```

Os programas de [`tests/exec_inputs/`](../tests/exec_inputs/) e
[`benchmarks/`](../benchmarks/) servem de exemplo.

## 5. Mensagens de erro

O compilador para na primeira **fase** com erro, mas dentro de uma fase relata
todos os erros que encontrar. Toda mensagem traz linha e coluna.

| Código de saída | Fase | Exemplo de mensagem | Onde sai |
| :-------------: | ---- | ------------------- | -------- |
| 0 | — | — | — |
| 1 | uso do programa | `uso: python minic.py [--check\|--tokens\|--ast\|--symbols\|--ir\|--target=asm\|--run] [-O0\|-O1] [--stats] <arquivo.c>` | stderr |
| 2 | léxica | `Erro léxico na linha 2, coluna 13: símbolo "@" não reconhecido.` | stderr |
| 3 | sintática | `Erro de sintaxe na linha 3, coluna 3: esperado PONTO_E_VIRGULA; encontrado KW_RETURN.` | stderr |
| 4 | semântica | `SEM001 — linha 4, coluna 11: Identificador “total” não declarado neste escopo.` | stdout (relatório) |
| 5 | geração de código | `Erro na geração de código: o programa não define a função main.` | stderr |
| 6 | execução | `Erro de execução em main: divisão por zero.` | stderr |

Os diagnósticos semânticos têm um código, de `SEM001` a `SEM016`. O catálogo,
com a causa de cada um, está em
[semantica.md](semantica.md#catálogo-de-diagnósticos). Os erros de execução
estão em [backend.md](backend.md#erros).

Um exemplo com três erros semânticos, todos relatados de uma vez:

```c
int main() {
    int n = 2.5;      // SEM003: float não converte para int
    bool ok = n;      // SEM003: int não converte para bool
    print(total);     // SEM001: total não foi declarado
    return 0;
}
```

```text
SEM003 — linha 2, coluna 13: Não é possível atribuir float a int sem conversão permitida (destino “n”; expressão “2.5”).
SEM003 — linha 3, coluna 15: Não é possível atribuir int a bool sem conversão permitida (destino “ok”; expressão “n”).
SEM001 — linha 4, coluna 11: Identificador “total” não declarado neste escopo.
Análise semântica concluída: 3 erros; programa rejeitado.
```

## 6. Execução

`--run` executa o programa numa máquina virtual de pilha:

- **`print`** escreve o valor e uma quebra de linha. Inteiros saem em decimal.
  Um `float` sai com os dígitos necessários e com `.0` quando é inteiro (`0.5`,
  `3.0`). Um `bool` sai como `true`/`false`, e um `char` como o próprio
  caractere.
- **`read`** lê o próximo item da entrada, separado por espaços ou quebras de
  linha. O item tem de ser do tipo da variável: `12`, `-3`, `2.5`, `true` ou um
  caractere.
- **Inteiros têm 32 bits**: `2147483647 + 1` dá `-2147483648`, como em C.
- **Variável sem valor inicial** vale zero: `0`, `0.0`, `false` ou o
  caractere nulo.
- **Erros de execução** (código 6) param o programa com uma mensagem que diz
  em que função aconteceu. O que já foi impresso continua na saída. São erros:
  - divisão ou resto por zero;
  - índice fora do vetor;
  - mais de 10 000 chamadas aninhadas;
  - entrada inválida ou esgotada.

```bash
$ echo "10 0" | python3 minic.py --run divide.c
Erro de execução em main: divisão por zero.
$ echo $?
6
```

## 7. Otimização

Com `-O1`, a IR passa por seis otimizações antes da geração de código:

- dobra de constantes;
- propagação de constantes e cópias;
- subexpressões comuns;
- remoção de código inalcançável;
- remoção de código morto;
- otimizações de laço (código invariante e redução de força).

O comportamento do programa é sempre o mesmo, só o trabalho diminui:

```bash
$ python3 minic.py --run --stats benchmarks/ordenacao_bolha.c
16
65424
instruções executadas: 1290929
$ python3 minic.py --run -O1 --stats benchmarks/ordenacao_bolha.c
16
65424
instruções executadas: 748953
```

Os números de todos os benchmarks estão em [benchmarks.md](benchmarks.md).

## 8. Perguntas frequentes

**Por que `python3 minic.py programa.c` não executa o programa?** Sem opção,
o `minic` só verifica o programa e imprime o relatório da análise semântica.
É o contrato da atividade AP 2, que chama o compilador assim. Para executar,
use `--run`.

**O programa precisa de `main`?** Para ser verificado, não: a análise
semântica aceita programas sem `main`, como os da AP 2. Para gerar código e
executar, sim (código 5).

**Por que meu `print(7 / 2)` imprime `3`?** Divisão entre inteiros é inteira,
como em C. Use `7.0 / 2` ou uma variável `float`.

**As versões C e Python dão resultados diferentes?** Não deveriam, em nada:
tokens, AST, mensagens, IR, código gerado, saída e contagem de instruções são
comparados byte a byte nos testes a cada push. Se encontrar uma diferença, é
um defeito; o caminho para reportar está em [PENDENCIAS.md](../PENDENCIAS.md).
