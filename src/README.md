# src/

Código-fonte do compilador, organizado por linguagem de implementação. As duas
versões seguem a mesma especificação e produzem, para o mesmo arquivo de
entrada, os mesmos tokens, a mesma AST, os mesmos erros e o mesmo código de
saída — byte a byte, o que é verificado por script (ver
[`tests/README.md`](../tests/README.md)).

- **[python/](python/)** — o scanner (`lexer.py`) e a sua CLI (`main.py`), o
  parser (`parser.py`) e os nós da AST (`minic_ast.py`), mais a interface
  Streamlit opcional (`app_streamlit.py`).
- **[c/](c/)** — a implementação equivalente em C, portada à mão a partir da
  versão Python: `lexer.c`/`lexer.h` e `main.c` (scanner), `ast.c`/`ast.h`,
  `parser.c`/`parser.h` e `parser_main.c` (parser e AST).

Ver o README dentro de cada pasta para detalhes de cada arquivo, como
compilar/rodar e como o algoritmo funciona.

## Por que a divisão é por linguagem, e não por fase

A Seção 14 da especificação sugere um layout por fase do compilador
(`src/lexer/`, `src/parser/`, `src/ast/`, …). Aqui a divisão é por **linguagem
de implementação**, porque a restrição mais forte deste projeto é manter duas
implementações equivalentes lado a lado: com pastas por fase, cada comparação
Python↔C ficaria espalhada por duas árvores. Dentro de cada linguagem, os
arquivos seguem a nomenclatura por fase (`lexer.py`, `parser.py`,
`minic_ast.py`; `lexer.c`, `parser.c`, `ast.c`). Ver
[`docs/arquitetura.md`](../docs/arquitetura.md#organização-de-pastas).

## Pontos de entrada exigidos pelas atividades

Cada atividade da disciplina define como o programa deve ser chamado, e é por
esses comandos que a entrega é avaliada. Os arquivos de entrada ficam na **raiz**
do repositório e são só casca — a lógica mora aqui:

| Comando exigido | Arquivo na raiz | O que ele faz |
|---|---|---|
| `python parser.py codigo.c` | [`../parser.py`](../parser.py) | ajusta o `sys.path` para `src/python/` e chama o `main()` de [`python/parser.py`](python/parser.py) |
| `./parser codigo.c` | [`../parser.c`](../parser.c) | inclui os quatro fontes de `c/` para compilar como **uma** unidade de tradução, que é como o script do professor compila |

Para desenvolver, a compilação separada é preferível (o compilador confere cada
arquivo isoladamente):

```bash
gcc -Wall -Wextra -std=c11 src/c/lexer.c src/c/ast.c src/c/parser.c \
    src/c/parser_main.c -o src/c/parser
```

O scanner da etapa 1 continua com o seu próprio executável
(`src/c/scanner`, a partir de `lexer.c` + `main.c`).
