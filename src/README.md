# src/

Código-fonte do compilador, organizado por linguagem de implementação. As
duas versões implementam a mesma especificação léxica e produzem os mesmos
tokens e os mesmos erros para o mesmo arquivo `.mc` — ver
[`tests/README.md`](../tests/README.md) para como isso é verificado.

- **[python/](python/)** — analisador léxico completo em Python (`lexer.py`),
  a CLI (`main.py`) e a interface Streamlit opcional (`app_streamlit.py`).
- **[c/](c/)** — implementação equivalente em C (`lexer.c`/`lexer.h`,
  `main.c`), portada à mão a partir da versão Python (mesma lógica, mesmas
  tabelas, mesmas mensagens de erro).

Ver o README dentro de cada pasta para detalhes de cada arquivo, como
compilar/rodar e como o algoritmo funciona.

## Por que a divisão é por linguagem, e não por fase

A Seção 14 da especificação sugere um layout por fase do compilador
(`src/lexer/`, `src/parser/`, `src/ast/`, …). Aqui a divisão é por **linguagem
de implementação**, porque a restrição mais forte deste projeto é manter duas
implementações equivalentes lado a lado: com pastas por fase, cada comparação
Python↔C ficaria espalhada por duas árvores. Dentro de cada linguagem, os
arquivos seguem a nomenclatura por fase (`lexer.py`, e na etapa 2 `parser.py`,
`ast.py`). Ver
[`docs/arquitetura.md`](../docs/arquitetura.md#organização-de-pastas).

## O que entra aqui na etapa 2

Nada disso existe ainda; está aqui para que os arquivos apareçam no lugar certo
quando forem escritos (plano completo em
[`docs/roteiro-etapa2-parser.md`](../docs/roteiro-etapa2-parser.md)):

- `python/ast.py` e `python/parser.py` — nós da AST e parser por descida
  recursiva, consumindo os tokens de `lexer.py`;
- `c/ast.c`/`ast.h` e `c/parser.c`/`parser.h` — o port equivalente;
- pontos de entrada `parser.py` e `parser` (executável), exigidos pelo
  enunciado da atividade — ver
  [`docs/arquitetura.md`](../docs/arquitetura.md#interface-de-linha-de-comando).
