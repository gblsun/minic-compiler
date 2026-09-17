# Etapas do projeto — status e entregas

Mapa do projeto no tempo: o que já foi entregue, o que está aberto e o que
ainda vem. A especificação (Seção 15) divide o compilador em quatro etapas; a
disciplina abre uma atividade no Classroom para cada uma.

- **Disciplina**: Compiladores — 202602 CC 6A Manhã (Ciência da Computação, 6º semestre, 2026)
- **Professor**: Alex Torquato Souza Carneiro
- **Grupo** (ordem alfabética): Fellipe Augusto Silva Pereira (2401525),
  Gabriel Muchon Pavanelli (2401895), Paloma Eduarda Soares Leite (2401660),
  Victor Wenzel Martins Gonçalves (2401698)

## Visão geral

| Etapa | Componente | Entregáveis (Seção 15) | Status |
|---|---|---|---|
| 1 | Analisador léxico | tokens, expressões regulares, scanner, erros e testes | ✅ entregue |
| 2 | Parser e AST | gramática, parser, AST, recuperação e testes sintáticos | ✅ concluída |
| 3 | Semântica e IR | tabela de símbolos, escopos, tipos, 3AC e testes | não iniciada |
| 4 | Código e otimização | backend, duas otimizações, benchmarks, manual e apresentação | não iniciada |

## Etapa 1 — analisador léxico ✅

- **Entrega**: 22/08/2026, 23:59.
- **Requisito do enunciado**: "A entrega do analisador léxico deverá ser feita
  mediante um repositório no Github contendo os a documentação, os códigos C,
  Python e os testes com os respectivos resultados." (citação literal)
- **O que foi entregue**: scanner em Python
  ([`src/python/lexer.py`](../src/python/lexer.py)) e em C
  ([`src/c/lexer.c`](../src/c/lexer.c)), com saída idêntica; 9 casos de teste
  com resultado gravado ([`tests/`](../tests/)); documentação léxica
  ([`especificacao.md`](especificacao.md), [`tokens.md`](tokens.md)); e uma
  interface Streamlit opcional.
- **Roteiro e checklist usados**: [`roteiro-etapa1-lexer.md`](roteiro-etapa1-lexer.md).
- **Pendências conhecidas** (não bloquearam a entrega, mas estão registradas):
  o formato de saída e os nomes de token divergem das fixtures oficiais em
  JSONL — ver
  [`ref/testes-oficiais/README.md`](../ref/testes-oficiais/README.md#divergências-entre-estas-fixtures-e-o-nosso-scanner).

## Etapa 2 — analisador sintático e AST ✅

- **Atividade no Classroom**: "AP 1 - analisador sintático", 10 pontos,
  publicada em 9 de setembro de 2026. O nome diz "AP 1", mas é a **etapa 2** do
  projeto — a etapa 1 foi o analisador léxico.
- **Entrega**: **18/09/2026, 23:59**. (O print do Classroom em
  [`ref/enunciados/ap-analisador-sintatico-enunciado.pdf`](../ref/enunciados/ap-analisador-sintatico-enunciado.pdf),
  tirado em 17/09/2026, mostra "Data de entrega: Amanhã, 23:59" — confira no
  Classroom antes de confiar nesta data.)
- **Comandos obrigatórios de invocação** (definidos no enunciado):

  ```bash
  python parser.py codigo.c
  ./parser codigo.c
  ```

- **Material de avaliação**: pacote
  [`ref/testes-oficiais/testes-parser-50/`](../ref/testes-oficiais/testes-parser-50/)
  — 50 programas, 25 aceitos (com AST esperada) e 25 rejeitados.
- **Scripts de teste fornecidos**:

  ```bash
  bash testar_parser_python.sh ./testes-parser-50 ./parser.py
  bash testar_parser_c.sh ./testes-parser-50 ./parser.c
  ```

  Originais em [`ref/scripts/`](../ref/scripts/); as armadilhas de cada um
  (comparação byte a byte, bit de execução, compilação em arquivo único) estão
  em
  [`ref/testes-oficiais/README.md`](../ref/testes-oficiais/README.md#armadilhas-dos-scripts-oficiais).
- **O que foi entregue**: parser por descida recursiva em Python
  ([`src/python/parser.py`](../src/python/parser.py) +
  [`minic_ast.py`](../src/python/minic_ast.py)) e em C
  ([`src/c/parser.c`](../src/c/parser.c), [`ast.c`](../src/c/ast.c),
  [`parser_main.c`](../src/c/parser_main.c)), com os pontos de entrada
  [`parser.py`](../parser.py) e [`parser.c`](../parser.c) na raiz, nos comandos
  exatos que o enunciado pede; AST com impressão compacta e indentada;
  diagnósticos com linha/coluna e recuperação em modo pânico; três suítes de
  teste em [`tests/run_parser_tests.py`](../tests/run_parser_tests.py).
- **Resultado**: 195/195 verificações — 50/50 casos oficiais e 15/15 casos
  próprios em cada implementação, e 65/65 entradas com saída idêntica entre
  Python e C. Registro completo em
  [`resultados-etapa2.md`](resultados-etapa2.md).
- **Documentação da etapa**: [`gramatica.md`](gramatica.md) (EBNF, precedência e
  decisões de desambiguação), [`ast.md`](ast.md) (nós e notação),
  [`roteiro-etapa2-parser.md`](roteiro-etapa2-parser.md) (plano executado, com o
  checklist de conformidade marcado).
- **Base teórica**: aulas 6 a 12 — GLC (aula 6, com
  [roteiro falado em texto](../ref/slides/aula-06-roteiro.txt)), descida
  recursiva (7), ascendente (8, 11), FIRST/FOLLOW (9), LL(1) (10) e integração
  scanner–parser–AST (12), mais o
  [tutorial de conversão tokens → AST](../ref/apostilas/tutorial-conversao-tokens-ast-glc.pdf).
- **Atividade prática da apostila 12** (Partes A, B e C), que é o que o
  professor descreve como o trabalho da etapa: documentar o contrato do token,
  definir os nós da AST e construí-los durante a análise, imprimir a árvore em
  formato canônico e testar entradas válidas, precedência, `if`-`else`, `while`,
  `return`, chamada e pelo menos três entradas inválidas.

## Etapa 3 — semântica e IR

Ainda sem atividade publicada. O que a especificação já fixa:

- tabela de símbolos com nome, categoria (`VARIÁVEL`, `CONSTANTE`, `FUNÇÃO`,
  `PARÂMETRO`, `VETOR`), tipo, escopo, nível léxico, linha/coluna da
  declaração, estado de inicialização/uso e deslocamento (Seção 6.1);
- operações `inserir`, `buscar`, `buscar_no_escopo_atual`, `abrir_escopo`,
  `fechar_escopo` (Seção 6.2);
- escopo léxico com sombreamento e proibição de declaração duplicada no mesmo
  escopo (Seção 6);
- regras semânticas da Seção 7 (identificador não declarado, lado esquerdo
  gravável, condição `bool` na versão estrita, aridade e tipos de argumentos,
  retorno compatível, `break`/`continue` só em laço, índice `int`, divisor
  constante zero rejeitado);
- conversões implícitas permitidas: `int` → `float` e `char` → `int`; as demais
  não (Seção 5);
- IR em código de três endereços, com as operações mínimas da Seção 9.1
  (`ADD`…`RETURN`), e exit code 4 para erro semântico.

## Etapa 4 — geração de código e otimização

- escolha do alvo (assembly de pilha, x86-64, LLVM IR ou VM própria) registrada
  no relatório e mantida estável (Seção 10);
- ao menos uma otimização local (simplificação algébrica, propagação de
  constantes, eliminação de código morto) e uma global (reaching definitions,
  live variables, eliminação de subexpressões comuns), com corretude
  demonstrada por testes antes e depois (Seção 10.1);
- entregáveis extras: benchmarks, manual e apresentação (Seção 15).
