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
| 50 casos oficiais | [`ref/testes-oficiais/testes-parser-50/`](../ref/testes-oficiais/testes-parser-50/) (material do professor) | 01–25: exit 0, stderr vazio e AST igual à esperada (espaços normalizados); 26–50: exit 3, nenhuma AST e diagnóstico "Erro sintático" |
| 15 casos próprios | [`parser_inputs/`](parser_inputs/) | stdout, stderr e exit code idênticos aos golden de [`parser_expected/`](parser_expected/) |
| Equivalência | as 65 entradas das duas suítes acima | a versão Python e a versão C devem devolver **exatamente** o mesmo stdout, stderr e exit code |

Resultado atual: 195/195 — registro em
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
