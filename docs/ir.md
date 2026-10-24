# Representação intermediária e otimização

> Atividade **"Testes para o MINIC (IR)"** (Classroom, 17/10), com o pacote
> [`ref/testes-oficiais/testes-ir-30/`](../ref/testes-oficiais/testes-ir-30/).
> Base teórica: Seção 9 (IR) e 10.1 (otimizações mínimas) da
> [especificação](../ref/especificacao-completa-minic.pdf) e as apostilas das
> aulas [16](../ref/apostilas/apostila-aula-16-sintaxe-semantica-avaliacao-expressoes.pdf),
> [17](../ref/apostilas/apostila-aula-17-ambientes-execucao-geracao-codigo.pdf),
> [18](../ref/apostilas/apostila-aula-18-otimizacao-local-fluxo-dados.pdf) e
> [19](../ref/apostilas/apostila-aula-19-otimizacao-de-lacos-integracao.pdf).

O compilador traduz a AST, depois da análise semântica, para **código de três
endereços** (3AC) e, com `-O1`, otimiza esse código. As duas implementações
imprimem a mesma IR, byte a byte, nos dois níveis.

```text
código-fonte ─► scanner ─► parser ─► semântica ─► IR (-O0) ─► otimizador (-O1)
                                                  ir.py        otimizador.py
                                                  ir.c         otimizador.c
```

## Como rodar

```bash
python3 minic.py --ir programa.c          # IR sem otimização (-O0 é o padrão)
python3 minic.py --ir -O1 programa.c      # IR otimizada

gcc -Wall -Wextra minic.c -o minic
./minic --ir -O1 programa.c
```

Sem `--ir`, o `minic` continua imprimindo o relatório da análise semântica:
é o contrato da AP 2, e o script `testes_semanticos_*.sh` depende dele. Os
nomes das opções são os da Seção 11 da especificação (`--ir`, `-O0`, `-O1`).

| Situação                     | stdout                       | stderr              | Código |
| ---------------------------- | ---------------------------- | ------------------- | :----: |
| programa válido com `--ir`   | a IR, uma instrução por linha (LF) | —             |   0    |
| erro semântico               | relatório da análise semântica | —                 |   4    |
| erro sintático / léxico      | —                            | mensagens de erro   | 3 / 2  |

Exemplo, o caso P10 do pacote oficial:

```text
$ python3 minic.py --ir ref/testes-oficiais/testes-ir-30/testes/01-validos-sem-otimizacao/P10/P10.c
func fatorial(n)
  t1 = n <= 1
  ifFalse t1 goto L_end_if
  return 1
L_end_if:
  t2 = n - 1
  param t2
  t3 = call fatorial, 1
  t4 = n * t3
  return t4
endfunc
func main
  param 5
  t1 = call fatorial, 1
  r = t1
  print r
  return 0
endfunc
```

## Formato da IR

O formato textual é o dos gabaritos do pacote oficial. Cada linha recuada é
uma instrução com no máximo um resultado e dois operandos (Seção 9). Os
rótulos ficam na coluna 0.

| Instrução                  | Significado                                   | Operação da Seção 9.1 |
| -------------------------- | --------------------------------------------- | --------------------- |
| `x = a`                    | cópia                                         | ASSIGN                |
| `x = a op b`               | `+ - * / %`, `< <= > >= == !=`, `and`, `or`   | ADD, SUB, MUL, DIV, MOD |
| `x = neg a` / `x = not a`  | menos unário / negação lógica                 | NEG / NOT             |
| `x = itof a` / `x = ctoi a`| conversão int → float / char → int            | —                     |
| `x = load v[i]`            | lê o elemento `i` do vetor `v`                | LOAD                  |
| `store v[i], a`            | escreve `a` no elemento `i`                   | STORE                 |
| `print a` / `read x`       | E/S                                           | —                     |
| `param a`                  | empilha um argumento                          | PARAM                 |
| `x = call f, n` / `call f, n` | chama `f` com os `n` últimos `param`       | CALL                  |
| `return a` / `return`      | retorno                                       | RETURN                |
| `goto L`                   | desvio                                        | JUMP                  |
| `ifFalse a goto L` / `ifTrue a goto L` | desvio condicional                | JUMP_IF_FALSE / JUMP_IF_TRUE |
| `L:`                       | rótulo                                        | LABEL                 |
| `func f(p1, p2)` … `endfunc` | delimita uma função (sem parênteses quando não há parâmetros) | — |
| `global g` / `global v[10]`  | declara uma global, no topo do arquivo      | —                     |

### Decisões de tradução

- **Temporários e rótulos.** Temporários são `t1`, `t2`, … e rótulos são
  `L_else`, `L_end`, `L_end_if`, `L_test`, `L_step`, `L_sc`. A numeração
  recomeça em cada função, como nos gabaritos (`main` volta a `t1` depois de
  `combinar` em P03). Quando o mesmo rótulo aparece de novo na função, ganha
  um número: `L_end2`, `L_end3`.
- **Nomes de variáveis.** Ficam como no código. Recebem sufixo, `x#1`, `x#2`
  na ordem de declaração, as variáveis que:
  - são declaradas mais de uma vez na mesma função (sombreamento, como em
    P09);
  - coincidem com o nome de uma global;
  - têm a forma de um temporário (uma variável `t1` vira `t1#1`).

  Uma global com forma de temporário vira `t1#g`.
- **Conversões implícitas viram instruções.** `int → float` vira `itof` e
  `char → int` vira `ctoi` (Seção 5). Um literal inteiro em contexto float vira
  direto `2.0`. Sem isso, o otimizador trataria `f / 2`, com `f` float, como
  uma divisão inteira.
- **Vetores.** O endereço é `índice * 1`, já que cada elemento ocupa uma
  posição, seguido de `load`/`store`, como em P07. Com `-O1`, o `* 1`
  desaparece.
- **`&&` e `||`** viram `and`/`or` explícitos, como em P08. Quando o operando
  da direita contém uma chamada, a tradução passa a ser com curto-circuito
  (`ifFalse`/`ifTrue` + rótulo `L_sc`), porque aí avaliar ou não o lado
  direito muda o que o programa faz.
- **Laços.** O `while` usa `L_test`, `ifFalse … goto L_end`, o corpo e
  `goto L_test`. O `for` faz o mesmo, com o passo no fim do corpo. Se houver
  `continue`, o passo ganha o rótulo `L_step`.
- **`if` sem `else`** usa um único rótulo, `L_end_if` (P08).
- **Globais** são declaradas no topo do arquivo. Os inicializadores, e os
  comandos soltos no topo que o parser aceita, rodam numa função própria,
  `@init`, antes de `main`. O nome não colide com nenhuma função do usuário
  porque `@` não é válido em identificador.
- **Declaração sem inicializador** (`int c;`, `int v[3];`) não gera
  instrução.

## Otimizador (`-O1`)

Os passes rodam em sequência, repetidos até nada mais mudar (com teto de 50
rodadas). No fim, os temporários são renumerados em ordem de aparição.

| # | Passe                                          | Tipo                               | O que faz |
| - | ---------------------------------------------- | ---------------------------------- | --------- |
| 1 | Simplificação                                  | local                              | Dobra de constantes inteiras e lógicas (`2 + 3` → `5`, `3 < 5` → `true`), identidades algébricas (`x + 0`, `x * 1`, `x * 0`, `b and true`, …), conversão de literal (`itof 2` → `2.0`, `ctoi 'A'` → `65`), desvio sobre constante (`ifFalse true` some; `ifFalse false` vira `goto`). |
| 2 | Propagação de constantes e cópias              | **global**, fluxo de dados *forward* | Um uso de `x` vira `c` quando, em **todos** os caminhos até ali, a última definição de `x` foi `x = c` e nem `x` nem `c` mudaram depois. É uma análise de *reaching definitions* restrita a cópias, com interseção nas junções e iteração até o ponto fixo sobre o grafo de blocos básicos. |
| 3 | Subexpressões comuns                           | local (bloco básico)               | `t3 = a * b` depois de `t1 = a * b`, sem redefinir `a`, `b` ou `t1` no meio, vira `t3 = t1`. |
| 4 | Limpeza do fluxo de controle                   | global                             | Remove blocos inalcançáveis (código depois de `return`, ramo de `if` constante), `goto` para o rótulo seguinte e rótulos sem uso. |
| 5 | Código morto                                   | **global**, *live variables* (backward) | Remove a atribuição a uma variável local ou temporário que não é lida em nenhum caminho adiante. |

A especificação pede pelo menos uma otimização local e uma global, com
corretude demonstrada por testes antes e depois (Seção 10.1). Aqui há duas
locais (1, 3) e três globais (2, 4, 5). A corretude é verificada executando a
IR -O0 e a -O1 e comparando o que cada uma imprime (ver [Testes](#testes)).

### Cuidados de corretude (aula 18, seção 1.1)

- **Nada com efeito observável é removido nem reordenado.** `print`, `read`,
  `call`, `store` e `load` ficam sempre. Um `load` pode acessar fora do vetor,
  então também não é apagado mesmo que o valor não seja usado. Uma divisão ou
  um resto só é removido se o divisor for uma constante diferente de zero.
- **Globais e chamadas.** Uma função chamada pode ler e escrever qualquer
  global. Por isso uma `call` invalida tudo o que se sabia sobre as globais,
  e as globais são consideradas vivas no fim da função e em toda chamada
  ([`tests/ir_inputs/globais_e_chamadas.c`](../tests/ir_inputs/globais_e_chamadas.c)).
- **Inteiros de 32 bits.** A dobra reproduz o transbordamento em complemento
  de dois e a divisão truncada em direção a zero do C, com o resto levando o
  sinal do dividendo. Não dobra divisão por zero nem `INT_MIN / -1`
  ([`dobra_inteiros.c`](../tests/ir_inputs/dobra_inteiros.c)).
- **`float` não é dobrado.** O texto do resultado dependeria de como cada
  linguagem formata números reais, e a IR das duas implementações precisa
  ser igual byte a byte. Constantes float são propagadas, mas não calculadas.
- **Curto-circuito.** Só some quando o lado esquerdo vira constante, e nesse
  caso o lado direito de fato não seria avaliado
  ([`curto_circuito.c`](../tests/ir_inputs/curto_circuito.c)).

### Antes e depois

O07 (dobra de condição + código inalcançável), de 7 para 2 instruções:

```text
-O0                              -O1
func main                        func main
  t1 = 3 < 5                       print 11
  ifFalse t1 goto L_else           return 0
  r = 11                         endfunc
  goto L_end
L_else:
  r = 22
L_end:
  print r
  return 0
endfunc
```

[`cse_local.c`](../tests/ir_inputs/cse_local.c): os valores vêm de `read`,
então não há o que dobrar, e o que aparece é a eliminação de subexpressão comum
(`a * b` calculado uma vez) junto com a propagação de cópias:

```text
-O0                              -O1
  read a                           read a
  read b                           read b
  t1 = a * b                       t1 = a * b
  t2 = t1 + 1                      t2 = t1 + 1
  x = t2                           t3 = t1 + 2
  t3 = a * b                       print t2
  t4 = t3 + 2                      print t3
  y = t4                           ...
  print x
  print y
  ...
```

A IR -O0 e -O1 de cada caso próprio está gravada em
[`tests/ir_expected/`](../tests/ir_expected/).

## Testes

```bash
python tests/run_ir_tests.py              # tudo, nas duas implementações
python tests/run_ir_tests.py -v           # caso a caso
python tests/fuzz_ir.py --c ./src/c/minic # programas válidos gerados, Python × C
```

O oráculo semântico é o [`tests/ir_interp.py`](../tests/ir_interp.py), um
interpretador da IR textual. Ele executa tanto a IR que o compilador gera
quanto a dos gabaritos, que usa o mesmo formato. Segue a semântica que o
gerador supõe: int de 32 bits, `/` e `%` como em C, e divisão por zero e laço
infinito tratados como erro de execução.

| Suíte                                 | O que exige                                                     | Python | C |
| ------------------------------------- | --------------------------------------------------------------- | :----: | :-: |
| Oficiais P01–P10 (válidos, -O0)       | código 0; a IR executa igual à IR do gabarito; -O1 executa igual | 10/10 | 10/10 |
| Oficiais E01–E10 (inválidos)          | código 2, 3 ou 4, conforme a categoria do gabarito; nenhuma IR  | 10/10 | 10/10 |
| Oficiais O01–O10 (otimização)         | -O1 executa igual a -O0 e à IR otimizada do gabarito, com menos instruções | 10/10 | 10/10 |
| Próprios (11 programas)               | IR -O0 e -O1 iguais ao golden, byte a byte; -O1 executa igual a -O0 | 11/11 | 11/11 |
| Equivalência (147 entradas)           | C e Python idênticos em `--ir -O0` e `--ir -O1`; nos 76 válidos, -O1 executa igual a -O0 | 147/147 | |
| Fuzz (programas válidos gerados)      | idem, em programas bem tipados gerados aleatoriamente           | 1000/1000 (semente 7) | |

**Total do `run_ir_tests.py`: 229/229.** O CI roda tudo isso a cada push e
repete o fuzz com o C compilado com AddressSanitizer e UBSan
([`testes.yml`](../.github/workflows/testes.yml)).

Das 10 IR dos programas válidos, **8 são idênticas ao gabarito** depois de
renumerar temporários, rótulos e sufixos (P01–P05, P07–P09). As outras 2 são
equivalentes (imprimem o mesmo) e diferem em pontos em que o próprio gabarito
foge da tradução direta:

- **P06.** O gabarito emite `i = 1` antes de `produto = 1`, mas no código a
  declaração `int produto = 1;` vem antes do `for (i = 1; …)`. A IR gerada
  segue a ordem do código.
- **P10.** O gabarito traduz o `if` sem `else` como se tivesse um (`goto
  L_end` depois do `return 1`, rótulo `L_rec`). P08, do mesmo pacote, traduz
  `if` sem `else` com um rótulo só, e é o que a IR gerada faz nos dois casos.

As IR otimizadas vão além do alvo de alguns gabaritos, e o próprio pacote diz
que elas só precisam ser "equivalentes". Em O02–O05, O08 e O09, as variáveis
recebem constantes, então a propagação dobra tudo. Por exemplo, O05 espera só
a eliminação de `a * b` repetido e recebe `print 22` / `print 23`. A
eliminação de subexpressão comum com valores desconhecidos aparece em
`cse_local.c`.

### Os scripts oficiais (`test_ir_python.sh` / `test_ir_c.sh`)

Os scripts do pacote rodam `minic <arquivo.c>`, sem opções. Removem de saída e
gabarito as linhas que casam com `IGNORE_RE` (por padrão, as que começam com
`mensagem`, `erro`, `aviso`, …) e comparam o resto **byte a byte** com `cmp`.
O README do mesmo pacote diz o contrário: os gabaritos são "uma IR canônica
de referência, não uma exigência de igualdade byte a byte". Do jeito que
vieram, nenhum compilador passa:

- todo gabarito tem linhas em prosa que nenhum compilador imprime:
  - `Resultado: COMPILAR COM SUCESSO`;
  - `Modo de referência: …`;
  - `Critério: diagnóstico de identificador “fantasma” não declarado; …`;
- os gabaritos de otimização trazem **duas** IR, a -O0 e a -O1, para uma
  única execução do compilador, que não recebe `-O1`. A IR -O0 deles também
  não é completa: O01 omite os temporários e nenhum tem `return 0`.

| Configuração                                                        | Normais | Erros | Otimização |
| ------------------------------------------------------------------- | :-----: | :---: | :--------: |
| scripts como vieram (`minic arquivo.c`)                             |  0/10   | 0/10  |    0/10    |
| `IGNORE_RE` personalizado para descartar a prosa e os diagnósticos, `minic --ir` | 7/10 | 10/10 | 0/10 |

O `IGNORE_RE` da segunda linha foi este, e o próprio script sugere
personalizá-lo:
`^[[:space:]]*(Resultado|Modo|IR |IR:|Categoria|Critério|Observação|Verificar|Alvo|SEM[0-9]|Análise|Erro)`.

Os 7 normais aprovados são os de IR idêntica ao gabarito, com exceção de P09,
que usa `x#outer`/`x#inner` em vez de `x#1`/`x#2`. Fica de fora quem ainda
diverge: P06 e P10 pelos motivos acima, e P09 pelos nomes. Por isso a suíte do
repositório faz a comparação que o README do pacote descreve (estrutura
normalizada + execução) em vez de usar o `cmp` dos scripts.

## Limitações e o que falta da etapa 4

- **Sem movimentação de código invariante (LICM) nem redução de força.** São
  os alvos de O08 e O09, temas da aula 19. Nos dois casos oficiais, a
  propagação de constantes já elimina a expressão invariante (`x * y` vira
  `42`). Em O09, o próprio gabarito diz que a redução de força é opcional.
- **CSE só dentro do bloco básico.** As expressões disponíveis entre blocos
  (*available expressions* global) não foram implementadas.
- **Sem back-end.** A Seção 10 pede um alvo (assembly de pilha, x86-64, LLVM IR
  ou VM própria). O interpretador de `tests/` executa a IR, mas é ferramenta de
  teste, não o alvo do compilador. Também faltam os benchmarks `-O0` × `-O1`,
  o manual e a apresentação (Seção 15).
- **O interpretador não confere limites de vetor**, e lê como 0 uma variável
  nunca escrita. Isso não afeta a comparação -O0 × -O1, porque o otimizador
  não remove nem move acessos a vetor nem supõe nada sobre variáveis não
  inicializadas.
