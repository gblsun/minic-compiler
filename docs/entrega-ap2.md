# Entrega da AP 2 — analisador semântico

Página única para avaliar a atividade **"AP 2 - Analisador Semântico"**: como
rodar, o resultado, e cada critério de correção das
[orientações de entrega](../ref/enunciados/orientacoes-entrega-analisador-semantico.pdf)
com a evidência correspondente no repositório.

## Como rodar (comandos do enunciado)

Da raiz do repositório, com os scripts e o pacote de testes da AP 2 (cópias
versionadas em [`ref/scripts/`](../ref/scripts/) e
[`ref/testes-oficiais/testes-semanticos-20/`](../ref/testes-oficiais/testes-semanticos-20/)):

```bash
bash ref/scripts/testes_semanticos_py.sh minic.py ref/testes-oficiais/testes-semanticos-20
bash ref/scripts/testes_semanticos_c.sh  minic.c  ref/testes-oficiais/testes-semanticos-20
```

```text
[OK]     ref/testes-oficiais/testes-semanticos-20/01_promocao_numerica.c
...
[OK]     ref/testes-oficiais/testes-semanticos-20/20_destino_nao_atribuivel.c

Resultado: 20/20 aprovados; 0 reprovados.
```

**20/20 nas duas implementações.** Não é preciso compilar nada antes: o script
C compila o `minic.c` sozinho (`gcc -Wall -Wextra minic.c`, sem nenhum aviso),
e o Python só precisa de `python3` (3.10 ou mais novo), sem nenhuma
dependência externa. Um arquivo isolado também roda direto:

```bash
python3 minic.py programa.c
gcc -Wall -Wextra minic.c -o minic && ./minic programa.c
```

## Critério 1 — desacoplamento entre os componentes

Cada componente fica no seu arquivo e conhece só a interface do anterior:

```text
código-fonte ─► scanner ─► tokens ─► parser ─► AST ─► analisador semântico ─► relatório
                lexer.py             parser.py        semantico.py
                lexer.c              parser.c         semantico.c
                                     minic_ast.py      └─ usa a tabela de símbolos:
                                     ast.c                tabela_simbolos.py / .c
```

| Componente | Python | C | Depende de |
|---|---|---|---|
| scanner | [`lexer.py`](../src/python/lexer.py) | [`lexer.c`](../src/c/lexer.c) / [`.h`](../src/c/lexer.h) | nada |
| AST | [`minic_ast.py`](../src/python/minic_ast.py) | [`ast.c`](../src/c/ast.c) / [`.h`](../src/c/ast.h) | nada |
| parser | [`parser.py`](../src/python/parser.py) | [`parser.c`](../src/c/parser.c) / [`.h`](../src/c/parser.h) | tokens do scanner; produz a AST |
| tabela de símbolos | [`tabela_simbolos.py`](../src/python/tabela_simbolos.py) | [`tabela_simbolos.c`](../src/c/tabela_simbolos.c) / [`.h`](../src/c/tabela_simbolos.h) | nada (só guarda declarações) |
| analisador semântico | [`semantico.py`](../src/python/semantico.py) | [`semantico.c`](../src/c/semantico.c) / [`.h`](../src/c/semantico.h) | **só** a AST e a tabela de símbolos; não importa o scanner nem o parser |
| CLI do pipeline | [`minic.py`](../src/python/minic.py) | [`minic_main.c`](../src/c/minic_main.c) | encadeia as fases |

- **Cada fase tem a sua CLI e roda sozinha**: `main.py`/`main.c` (só tokens),
  `parser.py`/`parser.c` (só AST) e `minic.py`/`minic.c` (até a semântica).
  As suítes das etapas 1 e 2 continuam passando sem mudança.
- **Os arquivos da raiz são só casca.** O `minic.py` carrega
  `src/python/minic.py`; o `minic.c` inclui os fontes de `src/c/` numa unidade
  de tradução, porque o script oficial compila um único arquivo. Para
  desenvolver, os `.c` compilam também separadamente, e o CI confere as duas
  formas.
- **A tabela de símbolos não sabe nada de tipos nem de AST**, e o analisador
  não manipula escopos diretamente: usa as operações da Seção 6.2 da
  especificação (`inserir`, `buscar`, `buscar_no_escopo_atual`,
  `abrir_escopo`, `fechar_escopo`).
- **O contrato entre parser e semântica é a AST**, a mesma da etapa 2. A única
  mudança no parser é uma opção (`destino_generico`) que o pipeline semântico
  liga, para `3 = n;` chegar à semântica como SEM013. Sem a opção, o parser se
  comporta exatamente como na etapa 2.

## Critério 2 — equivalência entre as versões C e Python

As duas versões têm a mesma estrutura, função por função, na mesma ordem
(`_coletar_globais` ↔ `sem_coletar_globais`, `_indexacao` ↔ `sem_indexacao`,
e assim por diante), com as mesmas mensagens e a mesma ordem de visita. A
equivalência é **verificada**, não só declarada:

| Verificação | Resultado |
|---|---|
| saída idêntica byte a byte (stdout, stderr e código de saída) nos 106 programas fixos: 20 oficiais da AP 2, 19 próprios, 50 oficiais do parser e 17 próprios do parser | **106/106** |
| saída idêntica em 900 programas gerados ([teste diferencial](../tests/README.md#teste-diferencial-programas-gerados)) | **900/900** |
| scripts oficiais da AP 2 | **20/20** em Python e **20/20** em C |
| ambientes: Ubuntu 22.04 (gcc 11, Python 3.10) e Ubuntu 24.04 (gcc 13, Python 3.12) | iguais nos dois |

A auditoria de equivalência encontrou e corrigiu duas diferenças antigas do
lexer C, que contava colunas em bytes depois de caracteres acentuados e
rejeitava `'é'`. Elas estão registradas em
[resultados-etapa3.md](resultados-etapa3.md#correções-feitas-no-lexer-c-durante-a-etapa-3).

## Critério 3 — lógica de implementação

A lógica está descrita em [semantica.md](semantica.md). Em resumo:

- **Duas passagens** (seção 2 da apostila da aula 15). A passagem A coleta
  assinaturas e globais (o que permite recursão direta e mútua); a passagem B
  verifica tudo na ordem do código.
- **Tabela de símbolos com pilha de escopos** (aula 14). Inclui sombreamento
  em bloco interno, duplicidade no mesmo escopo, parâmetros no mesmo escopo
  do corpo e globais visíveis só depois da declaração.
- **Síntese de tipos** pela matriz da seção 5 da apostila, com um ponto único
  de compatibilidade (`compativel`), usado em atribuição, inicialização,
  argumento, retorno e índice. As únicas conversões implícitas são
  `int → float` e `char → int`.
- **Análise de fluxo** para o retorno em todos os caminhos (seção 10 da
  apostila). Laço nunca garante retorno, e a mensagem explica **por onde** o
  controle chega ao fim.
- **Supressão de cascata** com o tipo sentinela `ERRO`: um erro não gera
  diagnósticos derivados, e as partes independentes continuam sendo
  verificadas.
- **Diagnósticos acionáveis**: código estável, linha e coluna do início da
  construção, nome ou operador, tipos esperado e recebido, a expressão
  reescrita e a posição da declaração anterior. Saem ordenados por posição.

Como "um código que passa em todos os testes não necessariamente está
correto", a lógica foi conferida além dos casos fixos:

| Verificação | O que garante | Resultado |
|---|---|---|
| 300 programas **bem tipados** gerados (escopos, sombreamento, promoções, vetores, chamadas, recursão, laços) | nenhum falso positivo | 300/300 aceitos |
| 300 programas com **exatamente um erro injetado** (de SEM001 a SEM016) | nenhum falso negativo nem cascata: sai exatamente o código injetado, e só ele | 300/300 |
| todas as 120 entradas de teste do repositório com o C compilado com **AddressSanitizer + UBSan** (inclui detecção de vazamento) | nenhum acesso inválido, vazamento ou comportamento indefinido | 120/120 |
| compilação com `gcc -Wall -Wextra -Werror` (gcc 11 e 13, também com `-std=c11 -pedantic`) e `clang -Wall -Wextra -Werror` | código C sem avisos | sem avisos |

## Critério 4 — casos de teste

- **20 casos oficiais** da AP 2: 10 aceitos e 10 rejeitados.
- **19 casos próprios**, que cobrem todos os códigos do catálogo, inclusive os
  que os oficiais não exercitam (SEM004, SEM010, SEM014, SEM015, SEM016).
  Também cobrem todas as variantes de SEM009, SEM011, SEM012 e SEM013,
  `for`, `print`, `read`, `char`, recursão mútua, supressão de cascata,
  colunas depois de acentos, erros léxico e sintático antes da semântica e os
  três programas de referência da especificação (13.1, 19.1 e 19.2).
- **Todos os casos inválidos obrigatórios** da Seção 13.2 da especificação têm
  teste (tabela em [testes-semanticos.md](testes-semanticos.md#casos-inválidos-obrigatórios-da-especificação-seção-132)).
- **Cada teste com o seu resultado** (objetivo, código e saída esperada):
  [testes-semanticos.md](testes-semanticos.md). O catálogo é gerado a partir
  dos próprios arquivos de teste, e o CI confere que está atualizado.
- **Teste diferencial** com 900 programas gerados (três modos).
- **CI** ([`.github/workflows/testes.yml`](../.github/workflows/testes.yml)):
  roda tudo isso a cada push, mais as suítes das etapas 1 e 2 e os scripts
  oficiais numa pasta com espaço e acento no nome, em locale `C`.

```bash
python3 tests/run_semantic_tests.py           # 184/184: oficiais + próprios nas duas versões + equivalência
python3 tests/fuzz_semantico.py --c ./minic   # teste diferencial (padrão: 600 programas)
```

## Decisões em que o pacote oficial prevaleceu sobre a especificação

Documentadas e testadas, como a apostila da aula 15 pede ("a equipe deve
documentar e testar a decisão, sem deixar comportamento implícito"):

| Ponto | Especificação | O que foi feito | Por quê |
|---|---|---|---|
| função `main` | Seção 2: obrigatória | não é exigida | os 10 programas válidos da AP 2 não têm `main` e o gabarito os aceita |
| `3 = n;` | gramática: destino é `localizavel` (erro de sintaxe) | erro semântico SEM013 | caso oficial 20; a apostila prevê que "o parser pode aceitar a forma geral" |
| `a = b = 3` | apostila: atribuição "não produz valor" | a atribuição tem o tipo do destino | caso oficial 09 |
| formato da saída | — | CRLF entre linhas, sem quebra no fim | é o conteúdo dos `.gabarito`, e o script compara com `diff` exato |

As demais decisões (conversões, escopos, posição dos diagnósticos, códigos
SEM014–SEM016) estão em [semantica.md](semantica.md#decisões-de-projeto).
