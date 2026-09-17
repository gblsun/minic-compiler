# Gramática sintática — MINIC

> Transcrição da Seção 4 ("Gramática sintática (EBNF)") de
> [`ref/especificacao-completa-minic.pdf`](../ref/especificacao-completa-minic.pdf) (v1.0),
> mais as notas de implementação necessárias para escrever o parser. Em caso de dúvida ou
> divergência, o PDF é a fonte normativa — este arquivo é o guia de trabalho da **etapa 2**.

Notação: símbolos entre aspas são terminais; `?` indica opcionalidade; `*`
indica repetição (zero ou mais); `+` indica uma ou mais ocorrências.

## Estrutura do programa

```ebnf
programa           ::= declaracao_global* declaracao_funcao* funcao_main ;
declaracao_global  ::= tipo identificador inicializacao? ";" ;
declaracao_funcao  ::= tipo_retorno identificador "(" parametros? ")" bloco ;
tipo_retorno       ::= tipo | "void" ;
parametros         ::= parametro ("," parametro)* ;
parametro          ::= tipo identificador | tipo identificador "[" "]" ;
tipo               ::= "int" | "float" | "bool" | "char" ;
bloco              ::= "{" item_bloco* "}" ;
item_bloco         ::= declaracao_local | comando ;
declaracao_local   ::= tipo declarador ("," declarador)* ";" ;
declarador         ::= identificador inicializacao? | identificador "[" tamanho "]" ;
inicializacao      ::= "=" expressao ;
```

## Comandos (Seção 4.1)

```ebnf
comando            ::= comando_expressao | comando_bloco | comando_if
                     | comando_while | comando_for | comando_return
                     | comando_break | comando_continue
                     | comando_print | comando_read ;
comando_expressao  ::= expressao? ";" ;
comando_if         ::= "if" "(" expressao ")" comando ("else" comando)? ;
comando_while      ::= "while" "(" expressao ")" comando ;
comando_for        ::= "for" "(" expressao? ";" expressao? ";" expressao? ")" comando ;
comando_return     ::= "return" expressao? ";" ;
comando_break      ::= "break" ";" ;
comando_continue   ::= "continue" ";" ;
comando_print      ::= "print" "(" argumento_print ")" ";" ;
comando_read       ::= "read" "(" localizavel ")" ";" ;
```

## Expressões (Seção 4.2)

```ebnf
expressao              ::= atribuicao ;
atribuicao             ::= localizavel "=" atribuicao | expressao_or ;
expressao_or           ::= expressao_and | expressao_or "||" expressao_and ;
expressao_and          ::= expressao_igualdade | expressao_and "&&" expressao_igualdade ;
expressao_igualdade    ::= expressao_relacional
                         | expressao_igualdade ("==" | "!=") expressao_relacional ;
expressao_relacional   ::= expressao_aditiva
                         | expressao_relacional op_rel expressao_aditiva ;
op_rel                 ::= "<" | ">" | "<=" | ">=" ;
expressao_aditiva      ::= expressao_multiplicativa
                         | expressao_aditiva ("+" | "-") expressao_multiplicativa ;
expressao_multiplicativa ::= expressao_unaria
                         | expressao_multiplicativa ("*" | "/" | "%") expressao_unaria ;
expressao_unaria       ::= ("-" | "!") expressao_unaria | expressao_posfixa ;
expressao_posfixa      ::= primario
                         | expressao_posfixa "[" expressao "]"
                         | expressao_posfixa "(" argumentos? ")" ;
primario               ::= identificador | literal_inteiro | literal_real
                         | literal_booleano | literal_caractere | "(" expressao ")" ;
```

## Precedência e associatividade (Seção 4.3)

Do mais forte para o mais fraco:

| Nível | Operadores | Associatividade |
|---|---|---|
| 1 | `()` `[]` chamada | Esquerda |
| 2 | `-` `!` (unários) | Direita |
| 3 | `*` `/` `%` | Esquerda |
| 4 | `+` `-` | Esquerda |
| 5 | `<` `>` `<=` `>=` | Esquerda |
| 6 | `==` `!=` | Esquerda |
| 7 | `&&` | Esquerda |
| 8 | `\|\|` | Esquerda |
| 9 | `=` | Direita |

## Não terminais que a especificação não define

Três símbolos aparecem no lado direito das produções sem ter produção própria
no PDF. Precisam ser fixados por nós — e a decisão registrada aqui:

| Símbolo | Onde aparece | Leitura adotada |
|---|---|---|
| `tamanho` | `declarador` (`identificador "[" tamanho "]"`) | `literal_inteiro`. Tamanho de vetor é constante em tempo de compilação; é também o que os casos oficiais usam (`int dados[10];`). |
| `localizavel` | `atribuicao`, `comando_read` | O que pode aparecer à esquerda de `=`: `identificador` ou `identificador "[" expressao "]"` (um *lvalue*). Ver a nota sobre alvo de atribuição abaixo. |
| `argumentos` | `expressao_posfixa` (chamada) | `expressao ("," expressao)*`. |
| `argumento_print` | `comando_print` | `expressao` (a especificação não menciona `print` de múltiplos argumentos nem formatação). |

## Notas de implementação

### Recursão à esquerda

As produções de expressão são escritas com **recursão à esquerda**
(`expressao_aditiva ::= … | expressao_aditiva ("+" | "-") expressao_multiplicativa`),
que é a forma correta para expressar associatividade à esquerda, mas que um
parser descendente não consegue executar literalmente — ele entraria em recursão
infinita. A transformação padrão (apostila da aula 07) é para laço:

```text
expressao_aditiva:
    no = expressao_multiplicativa()
    enquanto token atual é "+" ou "-":
        op = consome()
        direita = expressao_multiplicativa()
        no = Binary(op, no, direita)
    devolve no
```

O laço constrói a árvore associando à esquerda — `a - b - c` vira
`Binary(-, Binary(-, a, b), c)` —, que é exatamente o que a tabela de
precedência exige. Para os níveis associativos à **direita** (unários e `=`) a
recursão é direta, sem laço: `atribuicao ::= localizavel "=" atribuicao`.

### Alvo de atribuição

`atribuicao ::= localizavel "=" atribuicao | expressao_or` não é LL(1): ao ver
um identificador, o parser não sabe se está diante de uma atribuição ou do
início de uma expressão. Duas saídas usuais:

1. **Lookahead extra** — olhar o token seguinte ao `localizavel` (e, no caso de
   `v[i]`, até depois do `]`) para decidir.
2. **Analisar como expressão e reinterpretar** — chamar `expressao_or()` e, se o
   próximo token for `=`, verificar se o nó já construído é um *lvalue* válido
   (`Id` ou `Index`); se não for, é erro sintático. É a abordagem mais simples e
   produz um diagnóstico direto no caso oficial 38 (`atribuição sem destino`).

**Implementado com a opção 2** (`_atribuicao`/`parse_atribuicao`): se o nó à
esquerda do `=` não é `Id` nem `Index`, o erro sai na posição do próprio `=`
(`lado esquerdo da atribuição não é atribuível`). Mesma verificação vale para o
alvo de `read`, que a especificação também exige `localizavel`.

### `else` pendente (*dangling else*)

`comando_if ::= "if" "(" expressao ")" comando ("else" comando)?` é ambígua
para `if (a) if (b) x; else y;`. Convenção adotada (a usual, e a que o caso
oficial 23 assume): **o `else` liga ao `if` mais próximo**. Num parser
descendente isso sai de graça — basta consumir o `else` assim que ele aparecer,
dentro da chamada do `if` mais interno.

### Declaração local vs. comando

`item_bloco ::= declaracao_local | comando` também exige decisão por lookahead:
se o token atual é um dos tipos (`int`, `float`, `bool`, `char`), é declaração;
qualquer outra coisa é comando. Um caso de erro oficial (47,
`declaração local inválida`) exercita justamente esse ponto.

### Declaração global vs. função

`programa ::= declaracao_global* declaracao_funcao* funcao_main` sugere que as
globais vêm todas antes das funções, mas as duas começam igual
(`tipo identificador`) — o que decide é o token após o identificador: `(` é
função, qualquer outra coisa é variável. Vale implementar como **uma lista de
declarações de topo em qualquer ordem** (o caso oficial 25 tem `soma` antes de
`main`, e os casos 01–04 têm programa só com globais, sem `main` nenhum, ainda
assim marcados como `ACEITO`).

> Atenção: a Seção 2 da especificação diz que "todo programa MINIC deve conter
> exatamente uma função `main`", mas **13 dos 25** casos oficiais aceitos não
> têm `main` (01–04, 06, 12, 13, 16, 18–22). Isso é regra **semântica** (etapa
> 3), não sintática: o parser da etapa 2 não deve rejeitar um programa por falta
> de `main`.

## Divergências entre a especificação e os casos oficiais

Três pontos em que a gramática do PDF e o pacote de 50 casos não concordam.
Nos três, a implementação segue os **casos oficiais** (é por eles que a entrega
é avaliada), e a decisão está registrada aqui e em comentário no código:

| # | A gramática diz | Os casos oficiais exigem | Implementado |
|---|---|---|---|
| 1 | `programa ::= declaracao_global* declaracao_funcao* funcao_main` — só declarações no topo | o caso 22 (`int a; int b; a = b = 3;`) é **ACEITO**, com `ExprStmt` no nível do programa | topo aceita declaração **ou** comando, em qualquer ordem |
| 2 | `declaracao_global ::= tipo identificador inicializacao? ";"` — uma variável por vez, sem vetor | o caso 16 (`int dados[10];`) é **ACEITO** como global | global usa a mesma regra da local (`declarador ("," declarador)*`), o que também aceita `int a, b = 2, v[3];` |
| 3 | "todo programa deve conter exatamente uma função `main`" (Seção 2) | 13 dos 25 casos aceitos não têm `main` | o parser não exige `main`; isso passa a ser verificação semântica da etapa 3 |

O que **não** foi relaxado: função sem corpo (`int f();`, caso 48) continua
sendo erro, porque a MINIC não tem protótipo; e declaração dentro de um comando
sem bloco (`if (x) int y;`) também, porque `comando` não deriva
`declaracao_local`.

### Tokens do scanner usados pelo parser

O parser consome os tokens do nosso scanner, cujos nomes estão em
[`tokens.md`](tokens.md): palavras reservadas com prefixo `KW_` (`KW_INT`,
`KW_IF`, …), `IDENT`, `INT`, `FLOAT`, `CHAR`, `STRING`, operadores (`PLUS`,
`MINUS`, `STAR`, `SLASH`, `PERCENT`, `EQ`, `NEQ`, `LT`, `GT`, `LE`, `GE`,
`AND`, `OR`, `NOT`, `ASSIGN`), delimitadores (`LPAREN`, `RPAREN`, `LBRACKET`,
`RBRACKET`, `LBRACE`, `RBRACE`, `SEMICOLON`, `COMMA`) e `EOF`.

O `EOF` é gerado pelo `Lexer` e hoje é filtrado na impressão pelo
`main.py`/`main.c` — mas ele existe na lista de tokens e é o marcador de fim de
entrada que o parser precisa. Ver
[`arquitetura.md`](arquitetura.md#contrato-entre-scanner-e-parser).

## O que os 50 casos oficiais cobrem

Levantado a partir de
[`ref/testes-oficiais/testes-parser-50/`](../ref/testes-oficiais/testes-parser-50/)
(contagem por `grep` nos 50 `codigo.c`):

| Construção | Aparece nos casos? |
|---|---|
| declaração global (com e sem inicializador) | sim (01–04, 12, 13, 16, 18, 19, 22) |
| vetor com tamanho e indexação | sim (16, 17, 25) |
| função com e sem parâmetros, `void` | sim (05, 06, 20, 25) |
| bloco aninhado | sim (11, 23) |
| `if` / `if`-`else` / `if` aninhado | sim (08, 09, 21, 23) |
| `while` | sim (10, 24, 25) |
| `return` com e sem expressão | sim (05–09, 20, 21, 25) |
| atribuição, inclusive em cadeia (`a = b = 3`) | sim (07, 22) |
| chamada com e sem argumentos | sim (14, 15, 25) |
| operadores aritméticos, relacionais, lógicos, unários | sim (03, 12, 13, 18, 24) |
| `float` e `bool` | sim (04, 12, 13, 19) |
| **`for`** | **não** |
| **`print` / `read`** | **não** |
| **`break` / `continue`** | **não** |
| **literal de caractere / cadeia, tipo `char`** | **não** |

Ou seja: os 50 casos exercitam pouco mais da metade da gramática. Implementar
só o que eles cobrem passaria nos testes oficiais, mas deixaria de fora
construções que a especificação lista como obrigatórias (Seção 18) e que voltam
a aparecer nas etapas 3 e 4.

**O parser implementa a gramática inteira**, incluindo as quatro linhas
marcadas como "não" acima. Como os casos oficiais não as exercitam, elas são
cobertas pelos casos próprios em
[`tests/parser_inputs/`](../tests/parser_inputs/): `for` completo e com as três
partes vazias, `break`/`continue`, `print`/`read`, literais de caractere e de
cadeia (com escapes), declaração múltipla, parâmetro vetor, `else` pendente e
comando vazio.

Os 25 casos de rejeição (26–50) mapeiam quais diagnósticos precisam existir:
ponto e vírgula ausente (26, 27, 45), parêntese/chave ausente (28–31, 39, 41,
49), problema em lista de parâmetros (32–34), operando ou expressão ausente
(35–37, 42, 50), atribuição sem destino (38), vírgula sem argumento (40),
corpo ausente (43, 48), token fora de contexto (44, 46, 47).
