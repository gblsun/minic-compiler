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
| 3 | Semântica e IR | tabela de símbolos, escopos, tipos, 3AC e testes | ✅ analisador semântico (AP 2) e IR de três endereços concluídos |
| 4 | Código e otimização | backend, duas otimizações, benchmarks, manual e apresentação | ✅ implementada: back-end (máquina de pilha + VM), `-O1` com laços, benchmarks, manual e apresentação |

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
- **Resultado**: 201/201 verificações — 50/50 casos oficiais e 17/17 casos
  próprios em cada implementação, e 67/67 entradas com saída idêntica entre
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

## Etapa 3 — analisador semântico ✅

Atividade **"AP 2 - Analisador Semântico"**
([enunciado](../ref/enunciados/ap-analisador-semantico-enunciado.md),
[orientações de entrega](../ref/enunciados/orientacoes-entrega-analisador-semantico.pdf)).

Comandos exigidos:

```bash
./testes_semanticos_py.sh minic.py [diretorio]
./testes_semanticos_c.sh minic.c [diretorio]
```

O script Python roda `python3 minic.py <teste>`. O script C compila
`gcc -Wall -Wextra minic.c` (uma unidade de tradução) e roda o executável.
Os dois comparam stdout+stderr com o `.gabarito` usando `diff` exato.

Critérios de correção (orientações): desacoplamento entre os componentes,
equivalência entre as versões C e Python, lógica de implementação e casos de
teste. A entrega é o repositório no GitHub com scanner, parser e analisador
semântico em arquivos separados, mais os testes com os resultados.

O que foi entregue:

- [`minic.py`](../minic.py) e [`minic.c`](../minic.c) na raiz, como os scripts
  chamam;
- tabela de símbolos e analisador semântico em módulos próprios, nas duas
  linguagens (`src/python/tabela_simbolos.py`, `src/python/semantico.py`,
  `src/c/tabela_simbolos.c`, `src/c/semantico.c`);
- regras, decisões e catálogo de mensagens em [semantica.md](semantica.md);
- 20/20 casos oficiais nas duas versões; 19 casos próprios com o resultado
  de cada um ([testes-semanticos.md](testes-semanticos.md)); equivalência
  C × Python em 106 programas fixos e 900 gerados
  ([resultados-etapa3.md](resultados-etapa3.md));
- a correspondência entre cada critério de correção e a evidência no
  repositório: [entrega-ap2.md](entrega-ap2.md).

### Representação intermediária ✅

Atividade **"Testes para o MINIC (IR)"** (Classroom, 17/10), com o pacote
`minic-ir-test-cases.zip`, em
[`ref/testes-oficiais/testes-ir-30/`](../ref/testes-oficiais/testes-ir-30/):
10 programas válidos (IR -O0), 10 inválidos e 10 de otimização (IR -O1).
O Classroom não mostra prazo; confira lá.

```bash
python3 minic.py --ir [-O0|-O1] programa.c
./minic --ir [-O0|-O1] programa.c
```

O que foi feito, detalhado em [ir.md](ir.md):

- gerador de código de três endereços ([`ir.py`](../src/python/ir.py),
  [`ir.c`](../src/c/ir.c)) no formato dos gabaritos;
- otimizador -O1 ([`otimizador.py`](../src/python/otimizador.py),
  [`otimizador.c`](../src/c/otimizador.c)): dobra de constantes e identidades
  algébricas, propagação global de constantes e cópias, subexpressões comuns
  locais, limpeza do fluxo de controle e eliminação de código morto por
  variáveis vivas;
- 30/30 casos oficiais nas duas versões. Dos 10 válidos, 8 têm IR idêntica ao
  gabarito e 2 são equivalentes. A comparação é estrutural + execução, como
  pede o README do pacote;
- 11 casos próprios com golden -O0/-O1, equivalência C × Python em 147
  entradas e em 1000 programas válidos gerados, e -O1 executando igual a -O0
  em todos eles ([`tests/run_ir_tests.py`](../tests/run_ir_tests.py),
  [`tests/fuzz_ir.py`](../tests/fuzz_ir.py)).

Os scripts oficiais do pacote (`test_ir_*.sh`) comparam o gabarito inteiro,
com a prosa, byte a byte, então nenhum compilador passa por eles do jeito que
vieram. Com o `IGNORE_RE` personalizado, chegam a 17/30. A análise está em
[ir.md](ir.md#os-scripts-oficiais-test_ir_pythonsh--test_ir_csh).

## Etapa 4 — geração de código e otimização ✅

A atividade da etapa 4 ainda não foi aberta no Classroom. O que está aqui
segue a Seção 15 da especificação ("Backend, duas otimizações, benchmarks,
manual e apresentação") e a apostila da aula 19 ("gerador de código,
otimizador, testes, relatório e manual"). Quando o enunciado sair, confira se
ele pede algo a mais, por exemplo um comando de invocação próprio.

| Entregável | Onde | Situação |
|---|---|---|
| **Back-end**: alvo registrado e estável (Seção 10) | máquina virtual de pilha própria (MVM): [`geracao.py`](../src/python/geracao.py)/[`.c`](../src/c/geracao.c) geram o código, [`vm.py`](../src/python/vm.py)/[`.c`](../src/c/vm.c) executam; [backend.md](backend.md) | ✅ |
| **Otimizações** (Seção 10.1): uma local e uma global, com testes antes e depois | `-O1` com seis passes, quatro deles globais, inclusive as otimizações de laço da aula 19 (código invariante e redução de força); [ir.md](ir.md#otimizador--o1) | ✅ |
| **Benchmarks** | [`benchmarks/`](../benchmarks/) e o relatório [benchmarks.md](benchmarks.md): 8 programas, de 0% a 42% menos instruções executadas, 7,8% no total | ✅ |
| **Manual** | [manual.md](manual.md) | ✅ |
| **Apresentação** | [apresentacao.md](apresentacao.md) (slides em Markdown/Marp) | ✅ |
| CLI da Seção 11 | `--tokens`, `--ast`, `--symbols`, `--check`, `--ir`, `--target=asm`, `--run`, `-O0`/`-O1`; códigos de saída 5 e 6 | ✅ |
| Tabela de símbolos com deslocamento (Seção 6.1) | `--symbols` | ✅ |

```bash
python3 minic.py --run [-O1] programa.c < entrada.txt
./minic --target=asm -O1 programa.c
python3 tests/run_backend_tests.py      # execução, VM × interpretador, C × Python
python3 benchmarks/rodar.py             # -O0 × -O1
```

Sem opção de modo, `minic programa.c` continua imprimindo o relatório da AP 2,
porque é assim que o script daquela atividade o chama. A Seção 11 diz que
`minic programa.mc` "compila o programa"; aqui compilar e executar é
`--run`, e só gerar o código é `--target=asm`. A decisão está registrada no
[PENDENCIAS.md](../PENDENCIAS.md).
