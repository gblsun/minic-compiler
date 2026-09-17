# tests/

Testes de regressão do analisador léxico, no formato "golden file" (a saída
esperada fica gravada em disco e é comparada byte a byte com a saída atual).

```
tests/
├── inputs/     arquivos .mc de entrada
├── expected/   saída esperada para cada entrada (stdout, stderr, exit code)
└── run_tests.py
```

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

## Os três runners de teste do projeto

Este repositório tem três scripts que rodam contra os mesmos
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

## Etapa 2 (parser): o que ainda não existe aqui

Nada de teste sintático está implementado. O plano — runner próprio para os 50
casos oficiais (`tests/run_parser_tests.py`), comparação de AST com
normalização de espaços e casos próprios para as construções que os oficiais
não cobrem — está em
[`docs/roteiro-etapa2-parser.md`](../docs/roteiro-etapa2-parser.md#6-testes).
