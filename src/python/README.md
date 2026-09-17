# src/python/

Implementação em Python do MINIC: analisador léxico (etapa 1) e analisador
sintático com AST (etapa 2).

| Arquivo | O que é |
|---|---|
| [lexer.py](lexer.py) | O analisador léxico em si: classe `Lexer`, `Token`, `LexError` e as tabelas de palavras-chave/símbolos/escapes. Não depende de nada fora da biblioteca padrão — pode ser importado por qualquer interface (CLI, testes, Streamlit) sem carregar nada de terminal ou de UI junto. |
| [main.py](main.py) | CLI mínima: `python main.py <arquivo.mc>`. Lê o arquivo, chama `Lexer`, imprime tokens em stdout e erros em stderr, e devolve o exit code definido na especificação (0 = ok, 2 = erro léxico). |
| [minic_ast.py](minic_ast.py) | Os nós da AST e as duas formas de imprimi-la: compacta (S-expression de uma linha, a notação dos testes oficiais) e indentada (`--tree`). O nome não é `ast.py` de propósito: `ast` é um módulo da biblioteca padrão, e um arquivo com esse nome aqui passaria a sombreá-lo. |
| [parser.py](parser.py) | O analisador sintático por descida recursiva — uma função por não terminal da gramática de [`docs/gramatica.md`](../../docs/gramatica.md) — mais a CLI (`python parser.py <arquivo.c>`, com `--tree` e `--tokens`). Consome os tokens de `lexer.py`; se houver erro léxico, não roda. |
| [test_parser_python.sh](test_parser_python.sh) | Roda o script de teste do parser fornecido pela disciplina ([`../../ref/scripts/testar_parser_python.sh`](../../ref/scripts/testar_parser_python.sh)) com os caminhos deste repositório. O cabeçalho do arquivo explica como ler o resultado (16 aprovados / 25 erros sintáticos é o teto do pacote). |
| [app_streamlit.py](app_streamlit.py) | Interface web opcional (não faz parte da entrega obrigatória) para testar o lexer interativamente. Reaproveita o mesmo `Lexer` de `lexer.py` — nenhuma regra léxica é duplicada aqui. |
| [test_scanner_python.sh](test_scanner_python.sh) | Script de teste (formato pedido pela disciplina — ver o original em [`../../ref/scripts/test_scanner_python.sh`](../../ref/scripts/test_scanner_python.sh), adaptado aqui para rodar `main.py` e comparar com `tests/expected/`; ver [`tests/README.md`](../../tests/README.md) para como os três runners de teste do projeto se encaixam). |

## Rodando

Ver o [README.md](../../README.md) na raiz do repositório para o passo a
passo completo (CLI, suíte de testes e interface Streamlit).

```bash
bash src/python/test_scanner_python.sh
```

roda a mesma suíte de `tests/inputs/`/`tests/expected/` usada por
`tests/run_tests.py`, no formato de script pedido pela disciplina.

## Por que sem `re` e sem gerador de parser

O reconhecimento de tokens em `lexer.py` é feito na mão, caractere a
caractere, em vez de usar expressões regulares; e `parser.py` é descida
recursiva escrita à mão, em vez de yacc/bison/ANTLR. Isso é proposital: o
objetivo da disciplina é entender o funcionamento interno de um scanner
(cursor, lookahead, maximal munch) e de um parser (FIRST de cada produção,
recursão à esquerda virando laço, desambiguação do `else` pendente), não só
produzir a saída certa.

## Como o parser reporta erro

Cada função "desiste" levantando `ParseError`; os laços de item de bloco e de
item de topo capturam, registram o diagnóstico e sincronizam em modo pânico
(`;`, `}` ou início de construção nova). É isso que permite reportar mais de um
erro por execução — ver `tests/parser_inputs/erro_multiplos.c`, que produz três
diagnósticos numa só passada. O código de saída é 3 para erro sintático, 2 para
erro léxico (Seção 11.1 da especificação).
