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
