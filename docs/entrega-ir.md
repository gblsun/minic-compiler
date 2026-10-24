# Entrega — Testes para o MINIC (IR)

Página única para avaliar a atividade **"Testes para o MINIC (IR)"**
(Classroom, 17/10, anexo `minic-ir-test-cases.zip`). Explica como rodar, quais
foram os resultados e o que o pacote de testes permite ou não verificar. A
documentação técnica (formato da IR, decisões, passes do otimizador) está em
[ir.md](ir.md).

## Como rodar

Na raiz do repositório, sem instalar nada além de `python3` (3.10+) e `gcc`:

```bash
python3 minic.py --ir programa.c          # IR de três endereços (-O0)
python3 minic.py --ir -O1 programa.c      # IR otimizada

gcc -Wall -Wextra minic.c -o minic        # mesma coisa em C, sem nenhum aviso
./minic --ir -O1 programa.c
```

As opções são as da Seção 11 da especificação. Sem `--ir`, o `minic` imprime o
relatório da análise semântica, e os comandos da AP 2 continuam funcionando
como antes (20/20).

Os 30 casos do pacote estão versionados em
[`ref/testes-oficiais/testes-ir-30/`](../ref/testes-oficiais/testes-ir-30/),
e a suíte que os avalia roda com um comando:

```bash
python3 tests/run_ir_tests.py
```

```text
== IR — casos oficiais (Python) ==
  30/30 aprovados (P: 8 com IR idêntica ao gabarito, 2 equivalentes)
== IR — casos próprios (Python) ==
  11/11 aprovados
== IR — casos oficiais (C) ==
  30/30 aprovados (P: 8 com IR idêntica ao gabarito, 2 equivalentes)
== IR — casos próprios (C) ==
  11/11 aprovados
== IR — equivalência C × Python e -O0 × -O1 ==
  147/147 aprovados (76 programas válidos executados nos dois níveis)

Total: 229/229
```

Com `-v`, o resultado sai caso a caso, por exemplo
`ok    O07: 7 → 2 instruções; imprime 11`.

## Resultado por grupo do pacote

| Grupo | O que o gabarito pede | Como é verificado | Python | C |
|---|---|---|:-:|:-:|
| `01-validos-sem-otimizacao` (P01–P10) | IR canônica em `-O0` | IR normalizada (temporários, rótulos e sufixos renumerados) comparada à do gabarito, e as duas **executadas**: têm de imprimir o mesmo | 10/10 | 10/10 |
| `02-erros` (E01–E10) | rejeitar, com a categoria certa | código de saída 4 (semântico), 3 (sintático) ou 2 (léxico), como o gabarito diz, e nenhuma IR | 10/10 | 10/10 |
| `03-otimizacoes` (O01–O10) | IR `-O1` equivalente à de referência | `-O1` executa igual a `-O0` e à IR otimizada do gabarito, com menos instruções | 10/10 | 10/10 |

Dos 10 válidos, **8 têm IR idêntica** ao gabarito. Os outros 2 imprimem o
mesmo e diferem em pontos em que o próprio gabarito foge da tradução direta:

- **P06**: o gabarito põe `i = 1` antes de `produto = 1`, invertendo a ordem
  do código.
- **P10**: o gabarito traduz o `if` sem `else` como se tivesse um, enquanto
  P08 traduz o mesmo construto com um rótulo só.

Detalhes em [ir.md](ir.md#testes).

## Por que não pelos scripts do pacote

O pacote traz `test_ir_python.sh` e `test_ir_c.sh` (versionados em
[`ref/scripts/`](../ref/scripts/)). Eles comparam o gabarito inteiro **byte a
byte** com a saída, depois de remover só as linhas de mensagem. Só que todo
gabarito tem linhas em prosa que nenhum compilador imprime, como
`Resultado: REJEITAR` e `Critério: …`. E os casos de otimização trazem duas IR
(-O0 e -O1) para uma única execução. Por isso:

| Configuração | Normais | Erros | Otimização |
|---|:-:|:-:|:-:|
| scripts como vieram | 0/10 | 0/10 | 0/10 |
| filtro `IGNORE_RE` personalizado (como o script sugere) + `--ir` | 7/10 | 10/10 | 0/10 |

O 0/30 vale para **qualquer** compilador, não é uma falha deste. O README do
próprio pacote diz que os gabaritos são "uma IR canônica de referência, não
uma exigência de igualdade byte a byte", e pede comparação de estrutura e
semântica. É isso que `tests/run_ir_tests.py` faz. A análise completa e o
filtro usado estão em
[ir.md](ir.md#os-scripts-oficiais-test_ir_pythonsh--test_ir_csh).

## O que foi entregue

| Item | Onde |
|---|---|
| Gerador de IR (código de três endereços, Seção 9) | [`src/python/ir.py`](../src/python/ir.py), [`src/c/ir.c`](../src/c/ir.c) |
| Otimizador `-O1`: 2 passes locais (simplificação, subexpressões comuns) e 3 globais (propagação de constantes e cópias, limpeza do fluxo, código morto por variáveis vivas) | [`src/python/otimizador.py`](../src/python/otimizador.py), [`src/c/otimizador.c`](../src/c/otimizador.c) |
| CLI com `--ir`, `-O0`, `-O1` | [`src/python/minic.py`](../src/python/minic.py), [`src/c/minic_main.c`](../src/c/minic_main.c) |
| Interpretador da IR, usado para provar que `-O1` preserva o comportamento | [`tests/ir_interp.py`](../tests/ir_interp.py) |
| Suíte: 30 oficiais + 11 próprios (golden -O0/-O1) + equivalência | [`tests/run_ir_tests.py`](../tests/run_ir_tests.py), [`tests/ir_inputs/`](../tests/ir_inputs/), [`tests/ir_expected/`](../tests/ir_expected/) |
| Teste diferencial em programas válidos gerados (1000/1000) | [`tests/fuzz_ir.py`](../tests/fuzz_ir.py) |
| CI: tudo acima a cada push, mais o C com AddressSanitizer/UBSan | [`.github/workflows/testes.yml`](../.github/workflows/testes.yml) |

As duas implementações imprimem a mesma IR byte a byte, em `-O0` e em `-O1`.
Isso foi conferido em 147 entradas do repositório e em 1000 programas gerados.

## Depois desta entrega

A etapa 4 foi implementada sobre esta IR, fora do escopo do pacote de testes:

- o back-end: código para uma máquina virtual de pilha e sua execução (`--target=asm`, `--run`), descrito em [backend.md](backend.md);
- as otimizações de laço da aula 19 (código invariante e redução de força). Com elas, O09 sai na forma que o gabarito descreve;
- os [benchmarks](benchmarks.md), o [manual](manual.md) e a [apresentação](apresentacao.md).

Os números desta página continuam valendo. A suíte da IR passou de 229 para 235 verificações, com dois casos próprios de laço. Ver [etapas.md](etapas.md#etapa-4--geração-de-código-e-otimização-).
