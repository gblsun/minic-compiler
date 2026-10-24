# tests/

Testes de regressão do projeto, no formato "golden file" (a saída esperada fica
gravada em disco e é comparada byte a byte com a saída atual). Duas suítes, uma
por etapa:

```text
tests/
├── inputs/            .mc de entrada do SCANNER (etapa 1)
├── expected/          saída esperada de cada .mc (stdout, stderr, exit code)
├── run_tests.py       runner do scanner
├── parser_inputs/     .c de entrada do PARSER (etapa 2)
├── parser_expected/   saída esperada de cada .c (stdout, stderr, exit code)
└── run_parser_tests.py  runner do parser (+ casos oficiais e equivalência)
```

O restante deste arquivo detalha a suíte do scanner; a do parser está na seção
[Suíte do parser](#suíte-do-parser-etapa-2).

## Estrutura

Para cada `tests/inputs/<nome>.mc` existem três arquivos correspondentes em
`tests/expected/`:

- `<nome>.stdout.txt` — tokens que o lexer deve imprimir
- `<nome>.stderr.txt` — erros léxicos esperados (vazio se não houver nenhum)
- `<nome>.exit.txt` — código de saída esperado do processo (`0` ou `2`)

Os nomes seguem a convenção `valido_*.mc` para entradas sem erro léxico e
`erro_*.mc` para entradas que devem disparar erro — só para facilitar achar
um caso específico rapidamente, não é algo que o runner interprete.

## Rodando

```bash
python tests/run_tests.py
```

Compara a saída atual do lexer (rodando `src/python/main.py` de verdade,
como um usuário rodaria) com o que está gravado em `expected/`.

## Adicionando um teste novo

1. Crie `tests/inputs/<nome>.mc` com o caso que você quer cobrir.
2. Rode `python src/python/main.py tests/inputs/<nome>.mc` e confira **à
   mão** que a saída (tokens e/ou erros) está correta.
3. Só então grave o resultado como esperado:

   ```bash
   python tests/run_tests.py --update
   ```

   `--update` regrava a saída esperada para **todos** os arquivos em
   `inputs/`, não só o novo — revise o `git diff` de `tests/expected/` antes
   de commitar, para não acabar congelando uma regressão sem querer.

## Os três runners do scanner (etapa 1)

Três scripts rodam contra os mesmos
`tests/inputs/*.mc` e os mesmos `tests/expected/*.{stdout,stderr,exit}.txt` —
eles não competem entre si, cada um serve um propósito diferente:

| Script | Linguagem testada | Papel |
|---|---|---|
| [run_tests.py](run_tests.py) (aqui) | Python | **Dono** dos arquivos golden em `expected/`: é o único com a flag `--update`, usada para gravar/regravar o esperado depois de conferir a saída à mão. |
| [`src/python/test_scanner_python.sh`](../src/python/test_scanner_python.sh) | Python | Só lê `expected/`, nunca grava. Existe porque o enunciado da disciplina pede um script de teste ao lado do código-fonte do scanner (formato adaptado do original em [`ref/scripts/test_scanner_python.sh`](../ref/scripts/test_scanner_python.sh)). |
| [`src/c/test_scanner_c.sh`](../src/c/test_scanner_c.sh) | C | Compila `lexer.c`/`main.c` e compara com o mesmo `expected/` usado pela versão Python — é a prova em CI/terminal de que as duas implementações produzem os mesmos tokens e os mesmos erros (formato adaptado do original em [`ref/scripts/test_scanner_c.sh`](../ref/scripts/test_scanner_c.sh)). |

Ou seja: para adicionar ou alterar um caso de teste, sempre passe por
`tests/run_tests.py --update` (é o único que grava); os outros dois scripts
servem para *verificar* — em Python e em C — que o golden gravado continua
batendo com a saída de cada scanner.

## Esta suíte e os testes oficiais da disciplina

O que está aqui é a suíte **do projeto**: casos que nós escolhemos, no formato
que o nosso scanner produz, usada no dia a dia. Ela não substitui os pacotes
de teste **oficiais** do professor, que ficam em
[`ref/testes-oficiais/`](../ref/testes-oficiais/) e são o critério de avaliação
externo:

| | `tests/` (aqui) | `ref/testes-oficiais/` |
|---|---|---|
| Origem | escrita pelo grupo | fornecida pelo professor |
| Formato | `.mc` + stdout/stderr/exit gravados | JSONL (scanner) e AST em S-expression (parser) |
| Papel | regressão rápida durante o desenvolvimento | conformidade com a entrega |
| Editável | sim | **não** (material normativo) |

As divergências entre os dois formatos e as armadilhas dos scripts oficiais
estão documentadas em
[`ref/testes-oficiais/README.md`](../ref/testes-oficiais/README.md).

## Suíte do parser (etapa 2)

```bash
python tests/run_parser_tests.py            # tudo
python tests/run_parser_tests.py --python   # só a versão Python
python tests/run_parser_tests.py --c        # só a versão C
python tests/run_parser_tests.py --update   # regrava os golden dos casos próprios
python tests/run_parser_tests.py -v         # detalha caso a caso
```

[`run_parser_tests.py`](run_parser_tests.py) roda **três** suítes:

| Suíte | Fonte dos casos | Critério |
|---|---|---|
| 50 casos oficiais | [`ref/testes-oficiais/testes-parser-50/`](../ref/testes-oficiais/testes-parser-50/) (material do professor) | 01–25: exit 0, stderr vazio e AST igual à esperada (espaços normalizados); 26–50: exit 3, nenhuma AST e diagnóstico "Erro de sintaxe" |
| 17 casos próprios | [`parser_inputs/`](parser_inputs/) | stdout, stderr e exit code idênticos aos golden de [`parser_expected/`](parser_expected/) |
| Equivalência | as 67 entradas das duas suítes acima | a versão Python e a versão C devem devolver **exatamente** o mesmo stdout, stderr e exit code |

Resultado atual: 201/201 — registro em
[`docs/resultados-etapa2.md`](../docs/resultados-etapa2.md).

### Por que os casos próprios existem

Os 50 oficiais não exercitam `for`, `print`, `read`, `break`, `continue` nem
literais de caractere/cadeia, que a especificação lista como obrigatórios. Os
casos em `parser_inputs/` cobrem essas construções, mais declaração múltipla,
parâmetro vetor, `else` pendente, comando vazio, recuperação com vários erros
numa mesma entrada e a interação com erro léxico (exit code 2). A convenção de
nomes é a mesma da etapa 1: `valido_*.c` para entradas aceitas e `erro_*.c`
para as rejeitadas.

### Adicionando um caso próprio

1. Crie `tests/parser_inputs/<nome>.c`.
2. Rode `python parser.py tests/parser_inputs/<nome>.c` e confira **à mão** que
   a AST (ou o diagnóstico) está correta.
3. Grave o golden: `python tests/run_parser_tests.py --update` — como na etapa
   1, quem grava é sempre a versão **Python**, e o `--update` regrava todos os
   casos, então revise o `git diff` de `parser_expected/` antes de commitar.
4. Rode a suíte completa (`python tests/run_parser_tests.py`) para conferir que
   a versão C concorda byte a byte.

### Os scripts oficiais do professor

```bash
bash src/python/test_parser_python.sh
bash src/c/test_parser_c.sh
```

São wrappers que chamam os scripts de [`ref/scripts/`](../ref/scripts/) sem
alterá-los, apontando para os caminhos deste repositório. Eles marcam 16
aprovados e 25 erros sintáticos detectados — o teto do pacote de testes, por
defeitos conhecidos dele (a AST esperada dos 25 casos inválidos é a frase "NÃO
HÁ AST", e o espaçamento dos válidos é inconsistente). O porquê está em
[`docs/resultados-etapa2.md`](../docs/resultados-etapa2.md#scripts-oficiais-da-disciplina)
e em
[`ref/testes-oficiais/README.md`](../ref/testes-oficiais/README.md#armadilhas-dos-scripts-oficiais).

## Suíte do analisador semântico (etapa 3)

```bash
python tests/run_semantic_tests.py            # tudo
python tests/run_semantic_tests.py --python   # só a versão Python
python tests/run_semantic_tests.py --c        # só a versão C
python tests/run_semantic_tests.py --update   # regrava os golden dos casos próprios
python tests/run_semantic_tests.py -v         # detalha caso a caso
```

[`run_semantic_tests.py`](run_semantic_tests.py) roda **três** suítes, todas
comparando **bytes** (o relatório tem CRLF entre as linhas e nenhuma quebra no
fim, como os gabaritos oficiais):

| Suíte | Fonte dos casos | Critério |
|---|---|---|
| 20 casos oficiais | [`ref/testes-oficiais/testes-semanticos-20/`](../ref/testes-oficiais/testes-semanticos-20/) (AP 2) | stdout idêntico ao `.gabarito`, stderr vazio, exit 0 (aceito) ou 4 (rejeitado) |
| 19 casos próprios | [`semantic_inputs/`](semantic_inputs/) | stdout, stderr e exit code idênticos aos golden de [`semantic_expected/`](semantic_expected/) |
| Equivalência | as 39 entradas acima + os 50 oficiais e os 17 próprios do parser (106 programas) | Python e C devolvem exatamente o mesmo stdout, stderr e exit code |

Resultado atual: 184/184 — registro em
[`docs/resultados-etapa3.md`](../docs/resultados-etapa3.md).

Os casos próprios cobrem o que os oficiais não exercitam: operadores (SEM004),
`break`/`continue`/`return` fora de contexto (SEM010), categoria (SEM014),
divisão por zero constante (SEM015), tamanho e limites de vetor (SEM016),
todas as variantes de SEM009, SEM011, SEM012 e SEM013, recursão mútua, `for`,
`print`, `read`, `char`, supressão de cascata, colunas depois de caracteres
acentuados e os erros léxico e sintático antes da semântica. Nomes:
`aceito_*.c` para programas aceitos, `erro_*.c` para os rejeitados.

Para adicionar um caso, o processo é o mesmo do parser: crie
`tests/semantic_inputs/<nome>.c`, confira à mão a saída de
`python minic.py tests/semantic_inputs/<nome>.c`, grave com `--update` (sempre
a partir da versão Python), revise o `git diff` de `semantic_expected/` e rode
a suíte completa para conferir o C.

Os scripts oficiais da AP 2 rodam direto, sem wrapper:

```bash
bash ref/scripts/testes_semanticos_py.sh ./minic.py ref/testes-oficiais/testes-semanticos-20
bash ref/scripts/testes_semanticos_c.sh  ./minic.c  ref/testes-oficiais/testes-semanticos-20
```

### Teste diferencial (programas gerados)

```bash
python tests/fuzz_semantico.py                  # só Python, com os oráculos (600 programas)
python tests/fuzz_semantico.py --c ./minic      # Python × C
python tests/fuzz_semantico.py --c ./minic -n 2000 --semente 7 --salvar /tmp/falhas
```

[`fuzz_semantico.py`](fuzz_semantico.py) gera programas em três modos, um
terço de cada:

| Modo | Como é gerado | O que tem de acontecer |
|---|---|---|
| aleatório | gramática completa, poucos nomes (colisões frequentes), tipos misturados, destinos inválidos | Python e C com stdout, stderr e exit idênticos; nenhum traceback ou crash |
| bem tipado | acompanha escopos, tipos, vetores e assinaturas e só emite o que as regras permitem (promoções, sombreamento em bloco interno, `break` só em laço, índice literal dentro do vetor, `return` no fim de toda função não void) | os dois **aceitam** (pega falso positivo) |
| um erro injetado | um programa bem tipado mais **um** comando com erro conhecido, de SEM001 a SEM016 | os dois reportam **exatamente** aquele código, e só ele (pega falso negativo e cascata) |

A geração é determinística (`--semente`), então qualquer falha é reproduzível.
No CI, o lado C é o binário compilado com AddressSanitizer e UBSan.

O catálogo [`docs/testes-semanticos.md`](../docs/testes-semanticos.md) (cada
teste com o código e a saída esperada) é gerado por
[`gerar_catalogo_semantico.py`](gerar_catalogo_semantico.py). Depois de mudar
um caso ou um golden, rode `python tests/gerar_catalogo_semantico.py`; o CI
falha se o catálogo estiver desatualizado.

## Suíte da IR e do otimizador

```bash
python tests/run_ir_tests.py              # tudo, nas duas implementações
python tests/run_ir_tests.py --python     # só a versão Python
python tests/run_ir_tests.py --c          # só a versão C
python tests/run_ir_tests.py --update     # regrava os golden dos casos próprios
python tests/run_ir_tests.py -v           # detalha caso a caso
```

```text
tests/
├── ir_inputs/       .c próprios (e .in com a entrada de `read`, quando há)
├── ir_expected/     <nome>.O0.ir e <nome>.O1.ir — a IR esperada, byte a byte
├── ir_interp.py     interpretador da IR textual (o oráculo semântico)
├── run_ir_tests.py  runner: oficiais + próprios + equivalência
└── fuzz_ir.py       teste diferencial em programas válidos gerados
```

[`run_ir_tests.py`](run_ir_tests.py) roda três suítes:

1. **Casos oficiais** (`ref/testes-oficiais/testes-ir-30/`). O README do
   pacote diz que os gabaritos não exigem igualdade byte a byte; a suíte
   compara estrutura e execução:
   - **P**: a IR, com temporários, rótulos e sufixos renumerados, é
     comparada com a do gabarito (`idêntica` ou `equivalente`), e as duas são
     **executadas** no interpretador, que tem de imprimir o mesmo;
   - **E**: o código de saída é o da categoria do gabarito;
   - **O**: -O1 executa igual a -O0 e à IR otimizada do gabarito, com menos
     instruções.
2. **Casos próprios** (`ir_inputs/`): IR -O0 e -O1 iguais ao golden, e -O1
   executando igual a -O0.
3. **Equivalência**: `--ir -O0` e `--ir -O1` dão saída idêntica em Python e em
   C em todas as entradas do repositório (as da IR, da semântica e do parser),
   e -O1 executa igual a -O0 em cada programa válido.

[`fuzz_ir.py`](fuzz_ir.py) complementa com volume. Gera programas **válidos**
e que terminam, usando o que costuma quebrar um otimizador: globais alteradas
por funções, `&&`/`||` com chamada à direita, `break`/`continue`,
sombreamento, variáveis chamadas `t1`, conversões e constantes que
transbordam 32 bits. Para cada um, exige IR idêntica nas duas linguagens e o
mesmo comportamento em -O0 e -O1:

```bash
python tests/fuzz_ir.py --c ./src/c/minic -n 1000 --semente 7 --salvar /tmp/falhas
```

O formato da IR, os passes e os resultados estão em [`docs/ir.md`](../docs/ir.md).

## Suíte do back-end (geração de código e execução)

```bash
python tests/run_backend_tests.py              # tudo, nas duas implementações
python tests/run_backend_tests.py --python     # só a versão Python
python tests/run_backend_tests.py --update     # regrava os golden de execução
python tests/run_backend_tests.py -v           # detalha caso a caso
```

```text
tests/
├── exec_inputs/          .c executados com --run (e .in com a entrada de `read`)
├── exec_expected/        <nome>.out, .err e .exit esperados, byte a byte
└── run_backend_tests.py  runner: execução + oráculo + equivalência
```

[`run_backend_tests.py`](run_backend_tests.py) roda três suítes:

1. **Execução** (`exec_inputs/`): cada programa roda com `--run -O0` e
   `--run -O1`; os dois níveis têm de dar a mesma saída, o mesmo erro e o
   mesmo código, iguais ao golden. Inclui:
   - os programas de referência da especificação (13.1, 19.1 e 19.2);
   - o formato do `print` e o `read` de cada tipo;
   - vetores e globais;
   - os erros de execução (código 6) e de geração de código (código 5).
2. **Oráculo**: todo programa válido do repositório é executado na máquina
   virtual e tem de imprimir o mesmo que o interpretador da IR
   ([`ir_interp.py`](ir_interp.py)), que não depende do back-end. Ficam de
   fora os programas que acessam fora do vetor, porque o interpretador não
   confere limites e a VM confere; esse erro é coberto pela suíte de execução.
3. **Equivalência**: C e Python idênticos em `--tokens`, `--ast`, `--symbols`,
   `--target=asm` e `--run` (com e sem `-O1`), em todas as entradas.

O [`fuzz_ir.py`](fuzz_ir.py) também executa cada programa gerado na VM, nas
duas linguagens e nos dois níveis. Os benchmarks ([`../benchmarks/rodar.py`](../benchmarks/rodar.py))
falham se a saída de algum programa mudar com `-O1`.

O modelo de execução, as instruções e as mensagens de erro estão em
[`docs/backend.md`](../docs/backend.md).

## Integração contínua

O workflow [`.github/workflows/testes.yml`](../.github/workflows/testes.yml)
roda a cada push e pull request:

- **job `testes`**, em Ubuntu 22.04 (gcc 11, Python 3.10) e Ubuntu 24.04
  (gcc 13, Python 3.12): compilação de `parser.c` e `minic.c` com `-Werror`,
  as suítes das etapas 1, 2 e 3, da IR e do back-end (com 300 programas do
  `fuzz_ir.py`), os benchmarks, os exercícios da aula 13 e os scripts
  oficiais do professor (o do parser em locale `C`, como na avaliação);
- **job `robustez`**: clang com `-Werror`; todas as entradas de teste do
  repositório pelo binário com AddressSanitizer + UBSan, em todos os modos da
  CLI (inclusive `--run`); os testes diferenciais com 900 programas
  (semântica) e 500 (IR e execução); os scripts oficiais da AP 2 rodando de uma cópia do
  repositório em `/tmp/Avaliação Parcial II/minic compiler` (espaço e acento
  no caminho), em locale `C`; e a conferência de que o catálogo de testes
  está atualizado.

É também a forma de testar a versão C quando a máquina local não consegue
compilar.
