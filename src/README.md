# src/

Código-fonte do compilador, organizado por linguagem de implementação:

- **[python/](python/)** — analisador léxico completo em Python (`lexer.py`),
  a CLI (`main.py`) e a interface Streamlit opcional (`app_streamlit.py`).
  Ver o README dentro da pasta para detalhes de cada arquivo.
- **c/** — implementação equivalente em C, ainda não iniciada. Ver
  [sugestao_roteiro.md](../sugestao_roteiro.md) na raiz do repositório para a
  estrutura sugerida (`lexer.c`, `lexer.h`, `main.c`) — a entrega exige as
  duas linguagens produzindo os mesmos tokens e os mesmos erros para os
  mesmos arquivos `.mc` de teste.
