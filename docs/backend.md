# Back-end: geração de código e máquina virtual

> Etapa 4 da especificação (Seção 10 e Seção 15). Base teórica: apostila da
> [aula 17](../ref/apostilas/apostila-aula-17-ambientes-execucao-geracao-codigo.pdf)
> (ambientes de execução, registros de ativação, máquina de pilha).
> Implementação: [`geracao.py`](../src/python/geracao.py) /
> [`geracao.c`](../src/c/geracao.c) (gerador de código) e
> [`vm.py`](../src/python/vm.py) / [`vm.c`](../src/c/vm.c) (máquina virtual).

O compilador gera código para uma **máquina virtual de pilha própria**, a
**MVM**, e a executa. As duas implementações geram o mesmo código e produzem a
mesma saída, os mesmos erros e a mesma contagem de instruções, byte a byte.

```text
AST ─► IR (3AC) ─► otimizador (-O1) ─► geracao: código da MVM ─► vm: execução
                                         --target=asm             --run
```

## Escolha do alvo

A Seção 10 permite assembly de máquina de pilha, x86-64, LLVM IR ou código para
uma máquina virtual própria, desde que a escolha seja registrada e mantida
estável. A escolha foi a **máquina virtual de pilha própria**:

- **É o alvo da aula 17.** O próprio material traduz expressões, decisões,
  laços e chamadas "para IR e para uma máquina de pilha didática".
- **Mantém as duas implementações equivalentes.** x86-64 ou LLVM dependeriam
  de ferramentas externas (montador, `llc`) e de plataforma. A MVM roda igual
  em qualquer sistema, nas versões C e Python, e a comparação byte a byte que
  vale para as outras fases vale também aqui.
- **Permite verificar o código gerado.** Como a MVM executa o programa, os
  testes conferem a **saída** do programa compilado, e não só o texto do código.

## Como usar

```bash
python3 minic.py --target=asm programa.c        # código gerado (-O0)
python3 minic.py --target=asm -O1 programa.c    # código gerado a partir da IR otimizada
python3 minic.py --run programa.c < entrada.txt # gera e executa
python3 minic.py --run -O1 --stats programa.c   # + instruções executadas (em stderr)
python3 minic.py --symbols programa.c           # tabela de símbolos com deslocamentos

./minic --run -O1 programa.c                    # o mesmo, na versão em C
```

| Situação | Saída | Código |
| -------- | ----- | :----: |
| execução normal | o que o programa imprime, em stdout | 0 |
| erro na geração de código | `Erro na geração de código: …` em stderr | 5 |
| erro durante a execução | o que já foi impresso, em stdout, e `Erro de execução em <função>: …` em stderr | 6 |

## Modelo de execução

### Memória

| Área | O que guarda | Como é acessada |
| ---- | ------------ | --------------- |
| **Estática** | variáveis e vetores globais | pelo nome (`GLOAD g`, `GALOAD g`) |
| **Pilha de quadros** | um **registro de ativação** por chamada em andamento | pelo deslocamento no quadro (`LOAD n`) |
| **Pilha de operandos** | valores intermediários das expressões e argumentos de chamada | implícita (as instruções empilham e desempilham) |

As globais são inicializadas pela função `@init` da IR, se existir, que roda
antes de `main`. Uma variável não inicializada vale o zero do seu tipo: `0`,
`0.0`, `false` ou o caractere nulo.

### Registro de ativação

Cada chamada ganha um quadro com uma posição (*slot*) por nome usado na função,
nesta ordem:

1. os **parâmetros**, na ordem da declaração;
2. as **variáveis locais**, na ordem de declaração, inclusive as de blocos
   internos. Um nome sombreado tem posição própria (`x#1`, `x#2`);
3. os **temporários** da IR, na ordem em que aparecem.

O índice do slot é o **deslocamento** da Seção 6.1, e é o que `--symbols` mostra:

```text
$ python3 minic.py --symbols tests/exec_inputs/globais_vetores.c
escopo global (nível 0):
  contador: variável int — linha 2, coluna 5; inicializada, usada
  dados: vetor int[5] — linha 3, coluna 5; não inicializada, usada
  preencher: função void — linha 5, coluna 1; usada
  somar: função int — linha 13, coluna 1; usada
  main: função int — linha 22, coluna 1; não usada
…
função somar (registro de ativação: 9 posições):
  v: parâmetro vetor int[], nível 1 — linha 13, coluna 15; inicializada, usada; deslocamento 0
  n: parâmetro int, nível 1 — linha 13, coluna 24; inicializada, usada; deslocamento 1
  i: variável int, nível 1 — linha 14, coluna 9; não inicializada, usada; deslocamento 2
  s: variável int, nível 1 — linha 15, coluna 9; inicializada, usada; deslocamento 3
  t1: temporário — deslocamento 4
  …
```

A tabela mostra o que a Seção 6.1 pede para cada nome: categoria, tipo, nível
léxico, linha e coluna, estado de inicialização e uso, e deslocamento.

**Vetores locais** são alocados no quadro na entrada da função, com o tamanho
declarado, e liberados no retorno.

### Convenção de chamada

1. O chamador empilha os argumentos, da esquerda para a direita. Vetores vão
   **por referência**.
2. `CALL f n` cria o quadro de `f`, tira os `n` valores do topo da pilha e os
   coloca nos slots 0 … n−1, que são os parâmetros.
3. `RET` encerra o quadro e deixa **exatamente um valor** na pilha do chamador.
   Uma função `void`, ou uma que chega ao fim sem `return`, devolve 0, que o
   chamador descarta com `POP` (a análise semântica já garante que uma função
   não `void` retorna em todos os caminhos).
4. A recursão é limitada a 10 000 chamadas aninhadas. Passar disso é erro de
   execução, e não um *crash*: a VM guarda os quadros numa pilha própria, não
   na pilha do processo.

## Conjunto de instruções

| Instrução | Efeito na pilha | Descrição |
| --------- | --------------- | --------- |
| `PUSH lit` | → v | empilha um literal: `42`, `-3`, `2.5`, `true`, `'a'`, `"texto"` |
| `LOAD n` / `STORE n` | → v / v → | lê/escreve o slot `n` do quadro atual |
| `GLOAD g` / `GSTORE g` | → v / v → | lê/escreve a global `g` |
| `ALOAD n` / `GALOAD g` | i → v | lê o elemento `i` do vetor do slot `n` / global `g` |
| `ASTORE n` / `GASTORE g` | i v → | escreve `v` no elemento `i` |
| `ADD` `SUB` `MUL` `DIV` `MOD` | a b → r | aritmética; int de 32 bits ou float |
| `NEG` | a → r | menos unário |
| `LT` `LE` `GT` `GE` `EQ` `NE` | a b → bool | comparação |
| `AND` `OR` / `NOT` | a b → bool / a → bool | lógica |
| `ITOF` / `CTOI` | a → r | int → float / char → código do caractere |
| `JMP L` | — | desvio |
| `JZ L` / `JNZ L` | c → | desvia se `c` é falso / verdadeiro |
| `CALL f n` | a₁ … aₙ → r | chama `f` com `n` argumentos |
| `RET` | v → | retorna `v` ao chamador |
| `POP` | v → | descarta o topo |
| `PRINT` | v → | imprime `v` e uma quebra de linha |
| `READ tipo` | → v | lê um `int`, `float`, `bool` ou `char` da entrada |

## Tradução da IR

Cada instrução de três endereços vira "empilha os operandos, opera, guarda o
resultado". Um operando é empilhado com `PUSH` se for literal, com `LOAD` se for
local e com `GLOAD` se for global.

| IR | MVM |
| -- | --- |
| `x = a` | `push a` · `store x` |
| `x = a op b` | `push a` · `push b` · `OP` · `store x` |
| `x = op a` | `push a` · `OP` · `store x` |
| `x = load v[i]` | `push i` · `ALOAD v` · `store x` |
| `store v[i], a` | `push i` · `push a` · `ASTORE v` |
| `param a` | `push a` |
| `x = call f, n` / `call f, n` | `CALL f n` · `store x` / `CALL f n` · `POP` |
| `return a` / `return` | `push a` · `RET` / `PUSH 0` · `RET` |
| `ifFalse a goto L` / `ifTrue a goto L` | `push a` · `JZ L` / `push a` · `JNZ L` |
| `print a` / `read x` | `push a` · `PRINT` / `READ tipo` · `store x` |

O fatorial recursivo (programa 19.1 da especificação), com `--target=asm`:

```text
; MINIC — código para a máquina de pilha (MVM)
.func fatorial 1
.slot 0 n int
.slot 1 t1
.slot 2 t2
.slot 3 t3
.slot 4 t4
  LOAD 0  ; n
  PUSH 1
  LE
  STORE 1  ; t1
  LOAD 1  ; t1
  JZ L_end_if
  PUSH 1
  RET
L_end_if:
  LOAD 0  ; n
  PUSH 1
  SUB
  STORE 2  ; t2
  LOAD 2  ; t2
  CALL fatorial 1
  STORE 3  ; t3
  LOAD 0  ; n
  LOAD 3  ; t3
  MUL
  STORE 4  ; t4
  LOAD 4  ; t4
  RET
.endfunc
.func main 0
.slot 0 t1
  PUSH 5
  CALL fatorial 1
  STORE 0  ; t1
  LOAD 0  ; t1
  PRINT
  PUSH 0
  RET
.endfunc
```

`.func f n` abre uma função com `n` parâmetros, e cada `.slot` declara uma
posição do quadro, com o tipo quando ele é conhecido. `.global g tipo` declara
uma global. O texto depois de `;` é só comentário: o nome do slot.

## Semântica de execução

| Ponto | Regra |
| ----- | ----- |
| `int` | 32 bits. `+`, `-`, `*` e o menos unário transbordam em complemento de dois; `/` trunca em direção a zero e `%` tem o sinal do dividendo, como em C. `INT_MIN / -1` dá `INT_MIN` e `INT_MIN % -1` dá `0`, em vez do comportamento indefinido do C. |
| `float` | `double` (IEEE 754). |
| `print` de float | o menor número de dígitos que representa o valor de volta (`%g` com precisão crescente, até 17), com `.0` quando fica sem ponto: `0.5`, `3.0`, `0.30000000000000004`, `1.5e+15`. |
| `print` de bool e char | `true` / `false`; o próprio caractere. Uma cadeia é impressa sem as aspas. |
| `read` | lê o próximo item separado por espaço da entrada padrão. `int` aceita `-?[0-9]+`, dentro dos 32 bits; `float` aceita `-?[0-9]+(.[0-9]+)?`; `bool` aceita `true` ou `false`; `char` aceita exatamente um caractere (UTF-8). |
| `CTOI` | código Unicode do caractere: `'a'` → 97, `'é'` → 233. |

### Erros

| Código | Mensagem (stderr) | Quando |
| :----: | ----------------- | ------ |
| 5 | `Erro na geração de código: o programa não define a função main.` | programa válido para a semântica, mas sem `main` para executar |
| 6 | `Erro de execução em f: divisão por zero.` / `resto de divisão por zero.` | divisor zero (int ou float) |
| 6 | `Erro de execução em f: índice 4 fora dos limites do vetor “v” (tamanho 4).` | acesso fora do vetor |
| 6 | `Erro de execução em f: estouro da pilha de chamadas (mais de 10000 chamadas aninhadas).` | recursão sem fim |
| 6 | `Erro de execução em f: entrada “doze” inválida para read de int.` | entrada que não é do tipo esperado |
| 6 | `Erro de execução em f: entrada esgotada: read de int sem valor.` | `read` sem mais nada na entrada |

Tudo o que o programa imprimiu antes do erro continua em stdout. Cada erro
tem um programa de teste em [`tests/exec_inputs/`](../tests/exec_inputs/).

## Duas implementações, uma saída

- **Strings internadas.** O C guarda todos os nomes e operandos num *pool*
  único da IR, então comparar dois nomes é comparar ponteiros. O Python usa
  `str`.
- **VM iterativa nas duas.** Os quadros ficam numa pilha da própria VM, e não
  na recursão da linguagem hospedeira. Assim o limite de 10 000 chamadas é o
  mesmo nas duas, e a recursão profunda não derruba nem o Python nem o C.
- **Erros no C.** Saem por `setjmp`/`longjmp`, e todo o estado da execução
  (pilha, quadros, vetores) fica numa estrutura que é liberada num lugar só,
  com erro ou sem. O CI confere isso com AddressSanitizer.
- **Formato do float.** As duas versões usam o mesmo algoritmo (`%.*g` com
  precisão crescente até o valor voltar exato). No C, o expoente é
  normalizado para dois dígitos, porque o `printf` antigo do Windows usa três
  (`1e+020`).
- **Contagem de instruções** (`--stats`): é a mesma nas duas, porque executam o
  mesmo código. É a métrica dos [benchmarks](benchmarks.md).

## Testes

```bash
python3 tests/run_backend_tests.py      # execução + oráculo + equivalência C × Python
python3 tests/fuzz_ir.py --c ./minic    # inclui executar cada programa gerado na VM
python3 benchmarks/rodar.py             # -O0 × -O1, e a saída tem de ser a mesma
```

| Suíte | O que exige | Resultado |
| ----- | ----------- | :-------: |
| Execução (14 programas) | saída, erro e código iguais ao golden, em `-O0` e em `-O1`, nas duas versões | 14/14 + 14/14 |
| Oráculo (59 programas válidos do repositório) | a VM imprime o mesmo que o interpretador da IR ([`tests/ir_interp.py`](../tests/ir_interp.py)), que não depende do back-end | 59/59 |
| Equivalência (163 entradas × 8 modos) | C e Python idênticos em `--tokens`, `--ast`, `--symbols`, `--target=asm` e `--run`, com e sem `-O1` | 163/163 |
| Fuzz (programas válidos gerados) | a VM em C e em Python, nos dois níveis, imprime o mesmo que o interpretador | 300/300 por rodada no CI |

## Limitações

- **Tradução direta da IR, sem otimizações de janela.** Cada temporário vira
  `STORE` + `LOAD`. Um *peephole* ou uma alocação de registradores
  removeriam boa parte disso.
- **A MVM é interpretada.** Não há geração de código nativo. É o que a Seção
  10 permite com "máquina virtual própria".
- **Sem coleta de lixo**, e não há necessidade: só os vetores são alocados
  dinamicamente, e os locais são liberados no retorno da função.
