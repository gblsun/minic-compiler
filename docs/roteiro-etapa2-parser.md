# Roteiro — Etapa 2: analisador sintático e AST

> Plano de trabalho da atividade "AP 1 - analisador sintático" (etapa 2 do
> projeto), **já executado** — o checklist do fim está marcado e cada decisão
> registrada. Mesmo espírito do [roteiro da etapa 1](roteiro-etapa1-lexer.md):
> sugestão de ordem e checklist, não exigência do enunciado. Prazo e comandos
> obrigatórios em [`etapas.md`](etapas.md); resultados das suítes em
> [`resultados-etapa2.md`](resultados-etapa2.md).

Pré-requisitos de leitura, nesta ordem: [`gramatica.md`](gramatica.md) (o que
reconhecer), [`ast.md`](ast.md) (o que produzir),
[`arquitetura.md`](arquitetura.md#contrato-entre-scanner-e-parser) (o que o scanner
entrega) e
[`ref/testes-oficiais/README.md`](../ref/testes-oficiais/README.md) (como a
entrega vai ser medida).

## 0. Decisões tomadas antes de escrever código

Quatro escolhas travavam o resto do trabalho. Todas foram decididas conforme a
recomendação abaixo, e é o que está implementado:

| Decisão | Opções | Decisão adotada |
|---|---|---|
| Técnica de parsing | descida recursiva (LL, aula 7) vs. tabela LL(1) (aula 10) vs. LR (aulas 8/11) | **descida recursiva**: a gramática do MINIC é quase toda LL(1) com os ajustes de [`gramatica.md`](gramatica.md#notas-de-implementação), o código fica legível, o port para C é direto e o tutorial da disciplina usa exatamente essa técnica |
| Onde mora o parser | dentro de `src/` vs. na raiz | `src/python/parser.py` + `src/c/parser.c`, com **atalhos** `parser.py`/`parser` na raiz (ver seção 5) — mantém a organização e atende ao comando exigido |
| Alvo de atribuição | lookahead extra vs. reinterpretar expressão | **reinterpretar**: `expressao_or()` primeiro, e se vier `=`, validar que o nó é `Id` ou `Index` |
| Comparação de AST nos testes | byte a byte vs. normalizada | **normalizada** (espaços fora de lexemas), pelos defeitos do pacote oficial |

## 1. Contrato do token (Parte A da apostila 12)

Não há nada a implementar aqui — o scanner da etapa 1 já entrega o contrato
completo. O que falta é **usar** e documentar:

- o parser consome a lista de `Token` de `Lexer(source).tokenize()`, não o texto;
- tipos de token: os de [`tokens.md`](tokens.md), nomes com prefixo `KW_` para
  palavras reservadas;
- `line`/`column` de cada token vão para o nó da AST correspondente (a etapa 3
  depende disso);
- `EOF` é o marcador de fim de entrada — o parser aceita quando chega nele;
- se `errors` não estiver vazia, **não parsear**: imprimir os erros léxicos e
  sair com código 2.

## 2. Nós da AST (Parte B)

Criar o módulo de nós antes do parser, seguindo [`ast.md`](ast.md):

- `Program`, `VarDecl`, `Function`, `Param`, `Block`, `ExprStmt`, `If`,
  `While`, `Return`, `Assign`, `Binary`, `Unary`, `Call`, `Index`, `Id`, `Lit`
  (e, na segunda onda, `For`, `Print`, `Read`, `Break`, `Continue`);
- todo nó carrega `line`/`column`;
- um impressor canônico compacto (o formato dos testes) e um indentado (para
  depuração);
- em Python, `@dataclass` como já é feito em `lexer.py`; em C, uma `struct AST`
  com `enum TipoAST` e união de dados, como no
  [tutorial da disciplina](../ref/apostilas/tutorial-conversao-tokens-ast-glc.pdf).

## 3. Parser por descida recursiva

Uma função por não terminal, na ordem da gramática. Esqueleto do cursor igual
ao do lexer (`_peek`/`_advance`), só que sobre tokens:

```text
peek()            token atual, sem consumir
advance()         consome e devolve o token atual
check(tipo)       o token atual é desse tipo?
match(tipo)       se for, consome e devolve verdadeiro
expect(tipo, ctx) se for, consome; senão, erro sintático com posição e contexto
```

Ordem sugerida de implementação — cada item liga a casos oficiais que passam a
funcionar:

1. `primario` e `expressao_posfixa` (chamada e indexação) → casos 14, 15, 17
2. níveis binários, do `*` ao `||`, cada um como laço → casos 03, 12, 18, 24
3. `expressao_unaria` → caso 13
4. `atribuicao` (com a reinterpretação do alvo) → casos 07, 22
5. `declaracao_local`/`declaracao_global` e `declarador` (incluindo `[tamanho]`) → casos 01, 02, 04, 16
6. `bloco` e `item_bloco` → caso 11
7. `comando_if` (com `else` no `if` mais próximo), `comando_while`, `comando_return` → casos 08, 09, 10, 20, 21, 23
8. `declaracao_funcao` e `parametros` → casos 05, 06, 25
9. `programa` como lista de declarações de topo em qualquer ordem → caso 25
10. **segunda onda** (não coberta pelos 50 casos, mas exigida pela
    especificação): `for`, `print`, `read`, `break`, `continue`, literais de
    caractere e cadeia

## 4. Erros sintáticos e recuperação (Parte C)

- formato: `Erro de sintaxe na linha L, coluna C: esperado X; encontrado Y.` —
  o prefixo importa, porque o script oficial procura por ele com `grep` para
  contar os erros detectados (e precisa ser ASCII: em locale `C` o `grep` não
  casa "sintático");
- exit code **3** (Seção 11.1 da especificação) — diferente do 2 do erro léxico;
- os 25 casos de rejeição indicam as mensagens que precisam existir; as pistas
  em `resultado.esperado.txt` dizem o que o professor espera ver apontado;
- recuperação em **modo pânico**: descartar tokens até um sincronizador (`;`,
  `}`, `)` ou palavra reservada que inicie construção) e continuar, para
  reportar mais de um erro por execução sem cascata;
- nos casos rejeitados **não imprimir AST** — o pacote é explícito: "NÃO HÁ
  AST".

## 5. Pontos de entrada exigidos

O enunciado exige `python parser.py codigo.c` e `./parser codigo.c`. Como o
código mora em `src/`, a forma de atender sem duplicar lógica:

- `parser.py` na raiz: só um atalho que ajusta `sys.path` para `src/python/` e
  chama o `main()` de lá. Precisa de `chmod +x parser.py`, porque
  `testar_parser_python.sh` exige o bit de execução.
- `parser` (executável C): o script oficial compila **uma única unidade de
  tradução** (`gcc -Wall -Wextra -std=c11 $PARSER -o …`). Com `lexer.c` +
  `parser.c` + `main.c` isso não funciona direto. Duas saídas: (a) um
  `parser.c` na raiz que faça `#include` dos fontes de `src/c/` — funciona com
  o script oficial sem alteração; ou (b) manter a compilação multi-arquivo e
  adaptar o script, como foi feito na etapa 1 com
  [`src/c/test_scanner_c.sh`](../src/c/test_scanner_c.sh). A opção (a) é a que
  não depende de o professor rodar a nossa versão do script.
- Documentar no README da raiz os dois comandos, do jeito que o enunciado pede,
  e não só os caminhos internos.

## 6. Testes

Duas suítes, com papéis distintos:

1. **Nossa suíte** (`tests/`), no mesmo formato golden da etapa 1: entradas em
   `tests/inputs/` e saídas gravadas em `tests/expected/`, com `run_tests.py`
   como dono dos arquivos golden. Acrescentar casos de AST e de erro sintático,
   e rodar contra as duas implementações.
2. **Os 50 casos oficiais**, em `ref/testes-oficiais/testes-parser-50/`. Vale
   um runner próprio (`tests/run_parser_tests.py`) que:
   - para os casos 01–25, compare a AST produzida com `ast.esperada.txt`
     **normalizando espaços fora de lexemas**;
   - para os casos 26–50, verifique exit code ≠ 0, ausência de AST na saída e
     presença de diagnóstico sintático;
   - trate o caso 24 como exceção conhecida (AST oficial com parêntese
     faltando) e imprima isso no relatório, em vez de esconder;
   - imprima um resumo `N/50`, para virar evidência na entrega.

Os scripts oficiais continuam sendo rodados como conferência, com a ressalva já
documentada de que eles marcam os 25 casos inválidos como `FALHOU` por
construção.

## 7. Documentação da entrega

O enunciado da etapa 1 pediu "documentação, os códigos C, Python e os testes
com os respectivos resultados"; nada indica que mudou. Então, antes de entregar:

- [x] `README.md` da raiz mostra como rodar o parser nas duas linguagens, com
      os comandos exatos do enunciado
- [x] [`gramatica.md`](gramatica.md) reflete a gramática efetivamente
      implementada (inclusive as decisões de desambiguação)
- [x] [`ast.md`](ast.md) reflete os nós e o formato de impressão implementados
- [x] resultado da execução dos 50 casos registrado (não só o esperado)
- [x] `src/python/README.md` e `src/c/README.md` descrevem os arquivos novos
- [x] `tests/README.md` explica o runner novo e o que ele cobre

## Checklist de conformidade

**Gramática** (Seção 4 da especificação)

- [x] declarações globais, locais e vetores (`tipo id [tamanho]`)
- [x] funções com/sem parâmetros, `void`, parâmetro vetor (`tipo id[]`)
- [x] blocos aninhados
- [x] `if`, `if`-`else` com `else` ligado ao `if` mais próximo
- [x] `while`
- [x] `for` (não coberto pelos 50 casos, mas obrigatório) — testado em `tests/parser_inputs/valido_for_completo.c` e `valido_for_partes_vazias.c`
- [x] `return` com e sem expressão
- [x] `break`, `continue` — `valido_for_completo.c`
- [x] `print`, `read` — `valido_print_read.c` (e `read` só aceita variável ou elemento de vetor: `erro_read_alvo_invalido.c`)
- [x] atribuição associativa à direita, inclusive em cadeia
- [x] todos os níveis de precedência da Seção 4.3, com associatividade correta
- [x] unários `-` e `!`
- [x] chamada e indexação encadeáveis — `f(2)[0] + g()(1)` é aceito pelas duas implementações com a mesma AST (`v[i][j]` é erro semântico, não sintático)
- [x] literais: inteiro, real, booleano, caractere e cadeia — `valido_literais.c`, com escapes preservados na impressão

**AST**

- [x] um nó por construção, com `line`/`column`
- [x] parênteses não geram nó
- [x] `NULL` explícito em ramo ausente (`If` sem `else`, `return` vazio)
- [x] impressão canônica compacta estável entre execuções
- [x] impressão indentada para depuração

**Erros**

- [x] `Erro de sintaxe na linha L, coluna C: …` com token/lexema encontrado
- [x] exit code 3
- [x] mais de um erro por execução (modo pânico com sincronização)
- [x] nenhuma AST impressa quando a entrada é rejeitada

**Equivalência Python/C**

- [x] mesma AST e mesmos diagnósticos para as mesmas entradas — 65/65 entradas com saída byte a byte idêntica
- [x] verificado por script (`tests/run_parser_tests.py`, suíte de equivalência), contra os mesmos arquivos esperados

**Testes**

- [x] 25 casos válidos oficiais produzindo a AST esperada (com a exceção
      documentada do caso 24) — 25/25 em Python e em C
- [x] 25 casos inválidos oficiais rejeitados com diagnóstico e exit code 3 — 25/25 em Python e em C; tabela dos diagnósticos em [`resultados-etapa2.md`](resultados-etapa2.md#cobertura-dos-diagnósticos-dos-25-casos-inválidos)
- [x] casos próprios cobrindo `for`, `print`, `read`, `break`, `continue` e
      literais de caractere/cadeia, que os oficiais não exercitam — 15 casos
      em `tests/parser_inputs/`
- [x] resultados gravados em `tests/parser_expected/` (golden de stdout, stderr e exit code de cada caso próprio)

## O que foi construído (mapa final)

| Arquivo | Papel |
|---|---|
| [`../parser.py`](../parser.py) | ponto de entrada `python parser.py codigo.c` (atalho para `src/python/parser.py`), com bit de execução para o script oficial |
| [`../parser.c`](../parser.c) | ponto de entrada `./parser codigo.c`; inclui os fontes de `src/c/` para compilar como unidade única, que é como o script oficial compila |
| [`../src/python/minic_ast.py`](../src/python/minic_ast.py) | nós da AST + impressão compacta e indentada |
| [`../src/python/parser.py`](../src/python/parser.py) | parser por descida recursiva + CLI |
| [`../src/c/ast.h`](../src/c/ast.h) / [`ast.c`](../src/c/ast.c) | a mesma AST em C (struct única + enum de tipo de nó) |
| [`../src/c/parser.h`](../src/c/parser.h) / [`parser.c`](../src/c/parser.c) | o mesmo parser em C (setjmp/longjmp no lugar das exceções) |
| [`../src/c/parser_main.c`](../src/c/parser_main.c) | CLI em C |
| [`../tests/run_parser_tests.py`](../tests/run_parser_tests.py) | as três suítes: oficiais, próprias e equivalência |
| [`../src/python/test_parser_python.sh`](../src/python/test_parser_python.sh) / [`../src/c/test_parser_c.sh`](../src/c/test_parser_c.sh) | wrappers que rodam os scripts oficiais do professor com os caminhos deste repositório |

Duas divergências entre a gramática da especificação e os casos oficiais foram
encontradas durante a implementação e estão registradas em
[`gramatica.md`](gramatica.md#divergências-entre-a-especificação-e-os-casos-oficiais):
comandos soltos no nível do programa e declaração global com lista de
declaradores/vetor.
