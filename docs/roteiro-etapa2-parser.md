# Roteiro — Etapa 2: analisador sintático e AST

> Plano de trabalho para a atividade "AP 1 - analisador sintático" (etapa 2 do
> projeto). Mesmo espírito do [roteiro da etapa 1](roteiro-etapa1-lexer.md):
> sugestão de ordem e checklist, não exigência do enunciado. Prazo e comandos
> obrigatórios em [`etapas.md`](etapas.md).

Pré-requisitos de leitura, nesta ordem: [`gramatica.md`](gramatica.md) (o que
reconhecer), [`ast.md`](ast.md) (o que produzir),
[`arquitetura.md`](arquitetura.md#contrato-entre-scanner-e-parser) (o que o scanner
entrega) e
[`ref/testes-oficiais/README.md`](../ref/testes-oficiais/README.md) (como a
entrega vai ser medida).

## 0. Decisões a tomar antes de escrever código

Quatro escolhas travam o resto do trabalho. As recomendações abaixo valem para
o prazo curto desta etapa:

| Decisão | Opções | Recomendação |
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

- formato: `Erro sintático na linha L, coluna C: esperado X; encontrado Y.` —
  o prefixo "Erro sintático" importa, porque o script oficial procura por ele
  com `grep` para contar os erros detectados;
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

- [ ] `README.md` da raiz mostra como rodar o parser nas duas linguagens, com
      os comandos exatos do enunciado
- [ ] [`gramatica.md`](gramatica.md) reflete a gramática efetivamente
      implementada (inclusive as decisões de desambiguação)
- [ ] [`ast.md`](ast.md) reflete os nós e o formato de impressão implementados
- [ ] resultado da execução dos 50 casos registrado (não só o esperado)
- [ ] `src/python/README.md` e `src/c/README.md` descrevem os arquivos novos
- [ ] `tests/README.md` explica o runner novo e o que ele cobre

## Checklist de conformidade

**Gramática** (Seção 4 da especificação)

- [ ] declarações globais, locais e vetores (`tipo id [tamanho]`)
- [ ] funções com/sem parâmetros, `void`, parâmetro vetor (`tipo id[]`)
- [ ] blocos aninhados
- [ ] `if`, `if`-`else` com `else` ligado ao `if` mais próximo
- [ ] `while`
- [ ] `for` (não coberto pelos 50 casos, mas obrigatório)
- [ ] `return` com e sem expressão
- [ ] `break`, `continue`
- [ ] `print`, `read`
- [ ] atribuição associativa à direita, inclusive em cadeia
- [ ] todos os níveis de precedência da Seção 4.3, com associatividade correta
- [ ] unários `-` e `!`
- [ ] chamada e indexação encadeáveis (`f(x)[0]`, `v[i][j]` é erro semântico, não sintático)
- [ ] literais: inteiro, real, booleano, caractere

**AST**

- [ ] um nó por construção, com `line`/`column`
- [ ] parênteses não geram nó
- [ ] `NULL` explícito em ramo ausente (`If` sem `else`, `return` vazio)
- [ ] impressão canônica compacta estável entre execuções
- [ ] impressão indentada para depuração

**Erros**

- [ ] `Erro sintático na linha L, coluna C: …` com token/lexema encontrado
- [ ] exit code 3
- [ ] mais de um erro por execução (modo pânico com sincronização)
- [ ] nenhuma AST impressa quando a entrada é rejeitada

**Equivalência Python/C**

- [ ] mesma AST e mesmos diagnósticos para as mesmas entradas
- [ ] verificado por script, contra os mesmos arquivos esperados

**Testes**

- [ ] 25 casos válidos oficiais produzindo a AST esperada (com a exceção
      documentada do caso 24)
- [ ] 25 casos inválidos oficiais rejeitados com diagnóstico e exit code ≠ 0
- [ ] casos próprios cobrindo `for`, `print`, `read`, `break`, `continue` e
      literais de caractere/cadeia, que os oficiais não exercitam
- [ ] resultados gravados em `tests/expected/`
