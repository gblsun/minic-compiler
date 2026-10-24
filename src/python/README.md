# src/python/

Implementação em Python do MINIC: analisador léxico (etapa 1), analisador
sintático com AST (etapa 2), analisador semântico (etapa 3), representação
intermediária, otimizador e back-end (etapa 4).

| Arquivo | O que é |
|---|---|
| [lexer.py](lexer.py) | O analisador léxico em si: classe `Lexer`, `Token`, `LexError` e as tabelas de palavras-chave/símbolos/escapes. Não depende de nada fora da biblioteca padrão — pode ser importado por qualquer interface (CLI, testes, Streamlit) sem carregar nada de terminal ou de UI junto. |
| [main.py](main.py) | CLI mínima: `python main.py <arquivo.mc>`. Lê o arquivo, chama `Lexer`, imprime tokens em stdout e erros em stderr, e devolve o exit code definido na especificação (0 = ok, 2 = erro léxico). |
| [minic_ast.py](minic_ast.py) | Os nós da AST e as duas formas de imprimi-la: compacta (S-expression de uma linha, a notação dos testes oficiais) e indentada (`--tree`). O nome não é `ast.py` de propósito: `ast` é um módulo da biblioteca padrão, e um arquivo com esse nome aqui passaria a sombreá-lo. |
| [parser.py](parser.py) | O analisador sintático por descida recursiva — uma função por não terminal da gramática de [`docs/gramatica.md`](../../docs/gramatica.md) — mais a CLI (`python parser.py <arquivo.c>`, com `--tree` e `--tokens`). Consome os tokens de `lexer.py`; se houver erro léxico, não roda. |
| [tabela_simbolos.py](tabela_simbolos.py) | Tabela de símbolos (aula 14): `Simbolo` (nome, categoria, tipo, posição, vetor/tamanho, parâmetros) e `TabelaSimbolos`, uma pilha de escopos com `inserir`, `buscar`, `buscar_no_escopo_atual`, `abrir_escopo` e `fechar_escopo` (Seção 6.2 da especificação). Não sabe nada de AST nem de regras de tipo. |
| [semantico.py](semantico.py) | O analisador semântico (etapa 3): duas passagens sobre a AST (coleta de assinaturas e globais; verificação), síntese de tipos, compatibilidade, contexto de laço e de função, análise de fluxo para retorno em todos os caminhos, e o relatório no formato do gabarito da AP 2. Regras em [`docs/semantica.md`](../../docs/semantica.md). |
| [ir.py](ir.py) | Gerador de código de três endereços: `Instr` (uma instrução, com a tabela das formas no docstring), `Funcao`, `ProgramaIR`, o `Gerador` que percorre a AST já verificada (nomes com sufixo para sombreamento, conversões `itof`/`ctoi` explícitas, vetores com `load`/`store`, curto-circuito quando há chamada) e `formatar`. Decisões em [`docs/ir.md`](../../docs/ir.md). |
| [otimizador.py](otimizador.py) | O `-O1`: blocos básicos, simplificação local, propagação global de constantes e cópias (fluxo de dados forward), subexpressões comuns locais, limpeza do fluxo de controle e código morto por variáveis vivas (backward), repetidos até o ponto fixo. |
| [geracao.py](geracao.py) | O back-end: traduz a IR para o código da máquina de pilha (MVM), monta o registro de ativação de cada função (parâmetros, locais, temporários) e imprime o `--target=asm` e o `--symbols`. Modelo e instruções em [`docs/backend.md`](../../docs/backend.md). |
| [vm.py](vm.py) | A máquina virtual que executa o código gerado (`--run`): pilha de operandos, pilha de quadros (iterativa, sem recursão do Python), inteiros de 32 bits, `read`/`print`, e os erros de execução (código 6). |
| [minic.py](minic.py) | CLI do pipeline completo, com os modos da Seção 11: `--check` (padrão), `--tokens`, `--ast`, `--symbols`, `--ir`, `--target=asm`, `--run`, e `-O0`/`-O1`/`--stats`; cada fase só roda se a anterior não teve erro; códigos de saída 0 a 6. |
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
