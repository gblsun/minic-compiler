# src/python/

Implementação em Python do analisador léxico do MINIC.

| Arquivo | O que é |
|---|---|
| [lexer.py](lexer.py) | O analisador léxico em si: classe `Lexer`, `Token`, `LexError` e as tabelas de palavras-chave/símbolos/escapes. Não depende de nada fora da biblioteca padrão — pode ser importado por qualquer interface (CLI, testes, Streamlit) sem carregar nada de terminal ou de UI junto. |
| [main.py](main.py) | CLI mínima: `python main.py <arquivo.mc>`. Lê o arquivo, chama `Lexer`, imprime tokens em stdout e erros em stderr, e devolve o exit code definido na especificação (0 = ok, 2 = erro léxico). |
| [app_streamlit.py](app_streamlit.py) | Interface web opcional (não faz parte da entrega obrigatória) para testar o lexer interativamente. Reaproveita o mesmo `Lexer` de `lexer.py` — nenhuma regra léxica é duplicada aqui. |

## Rodando

Ver o [README.md](../../README.md) na raiz do repositório para o passo a
passo completo (CLI, suíte de testes e interface Streamlit).

## Por que sem `re`

O reconhecimento de tokens em `lexer.py` é feito na mão, caractere a
caractere, em vez de usar expressões regulares. Isso é proposital: o
objetivo da disciplina é entender o funcionamento interno de um scanner
(cursor, lookahead, maximal munch), não só produzir a saída certa.
