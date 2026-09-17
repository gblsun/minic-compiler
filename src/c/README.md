# src/c/

Implementação em C do MINIC — analisador léxico (etapa 1) e analisador sintático
com AST (etapa 2) —, portada à mão a partir das versões Python
([`lexer.py`](../python/lexer.py), [`parser.py`](../python/parser.py),
[`minic_ast.py`](../python/minic_ast.py)), sem `re`/`lex`/`flex` nem gerador de
parser, para produzir exatamente os mesmos tokens, a mesma AST e as mesmas
mensagens de erro.

| Arquivo | O que é |
|---|---|
| [lexer.h](lexer.h) | Interface pública: `TokenType` (prefixo `TOK_`, para não colidir com macros do C como `EOF` de `<stdio.h>`), `Token`, `LexError`, `Lexer`, e as assinaturas de `lexer_init`/`lexer_tokenize`/`lexer_free`/`token_to_string`/`lexerror_to_string`. |
| [lexer.c](lexer.c) | Implementação: as mesmas tabelas do `lexer.py` (`KEYWORDS`, `TWO_CHAR_SYMBOLS`, `ONE_CHAR_SYMBOLS`, escapes) e a mesma máquina de reconhecimento caractere a caractere (cursor com linha/coluna, maximal munch, modo pânico). |
| [main.c](main.c) | CLI mínima: `./scanner <arquivo.mc>`. Lê o arquivo, roda o `Lexer` e imprime tokens em stdout / erros em stderr, com o mesmo contrato de exit code de `main.py` (0 = ok, 2 = erro léxico, 1 = erro de uso). |
| [ast.h](ast.h) | Interface da AST: `AstKind` (prefixo `AST_`), a struct `Ast` e as funções de construção, liberação e impressão. O cabeçalho traz a tabela de "qual campo significa o quê em cada tipo de nó" — o contrato que substitui os campos nomeados das `@dataclass` do Python. |
| [ast.c](ast.c) | Implementação da AST: criação de nós, lista de filhos que dobra de tamanho, liberação e as duas impressões (compacta e indentada), com as mesmas regras de espaçamento de `minic_ast.py`. |
| [parser.h](parser.h) | Interface do parser: `ParseError`, a struct `Parser` (tokens, erros, lista de nós e o ponto de recuperação) e `parser_init`/`parser_parse`/`parser_free`/`parseerror_to_string`. |
| [parser.c](parser.c) | O parser por descida recursiva: uma função por não terminal, na mesma ordem de `parser.py`. |
| [parser_main.c](parser_main.c) | CLI do parser: `./parser <arquivo.c>`, com `--tree` e `--tokens`, e os exit codes 0/1/2/3 da Seção 11.1. |
| [test_parser_c.sh](test_parser_c.sh) | Roda o script de teste do parser fornecido pela disciplina ([`../../ref/scripts/testar_parser_c.sh`](../../ref/scripts/testar_parser_c.sh)) com os caminhos deste repositório; o cabeçalho explica como ler o resultado. |
| [test_scanner_c.sh](test_scanner_c.sh) | Script de teste (formato pedido pela disciplina — ver o original em [`../../ref/scripts/test_scanner_c.sh`](../../ref/scripts/test_scanner_c.sh), adaptado aqui para compilar este scanner e comparar com `tests/expected/`; ver [`tests/README.md`](../../tests/README.md) para como os três runners de teste do projeto se encaixam). |

## Compilando e rodando

Scanner (etapa 1):

```bash
gcc -Wall -Wextra -std=c11 src/c/lexer.c src/c/main.c -o src/c/scanner
./src/c/scanner tests/inputs/valido_soma.mc
```

Parser (etapa 2) — do jeito que o enunciado pede, com o `parser.c` da raiz, que
inclui estes fontes para compilar como **uma** unidade de tradução:

```bash
gcc -Wall -Wextra -std=c11 parser.c -o parser
./parser ref/testes-oficiais/testes-parser-50/casos/09_if_com_else/codigo.c
```

Ou, para desenvolver, compilando cada arquivo separadamente:

```bash
gcc -Wall -Wextra -std=c11 src/c/lexer.c src/c/ast.c src/c/parser.c \
    src/c/parser_main.c -o src/c/parser
```

As duas formas compilam sem nenhum aviso com `-Wall -Wextra`.

Saída idêntica à da versão Python (mesmo formato de token, mesmas mensagens de
erro, mesmo exit code) — ver o [README.md](../../README.md) da raiz para o
contrato completo de stdout/stderr/exit code.

## Rodando os testes

```bash
bash src/c/test_scanner_c.sh            # etapa 1: scanner
python tests/run_parser_tests.py --c    # etapa 2: parser (50 oficiais + 15 próprios)
bash src/c/test_parser_c.sh             # etapa 2: script oficial da disciplina
```

`test_scanner_c.sh` compila o scanner (`lexer.c` + `main.c`) com
`gcc -Wall -Wextra -std=c11`, roda o binário sobre cada `.mc` de
`tests/inputs/` e compara com `tests/expected/` — os mesmos arquivos golden
usados pela versão Python.

`run_parser_tests.py` compila `parser.c` exatamente como o script do professor
(unidade única, mesmas flags) e, quando roda as duas implementações, confere
também que a saída é **idêntica byte a byte** à da versão Python nas 65
entradas — é assim que o requisito de equivalência entre as duas
implementações é verificado na prática. Resultados registrados em
[`docs/resultados-etapa2.md`](../../docs/resultados-etapa2.md).

## Como o algoritmo funciona (e por que é igual ao Python)

A estrutura do scanner é a mesma nas duas linguagens — só muda a forma de
representar cada peça, porque C não tem coleta de lixo, listas dinâmicas nem
dicionários prontos:

| Conceito | Em Python (`lexer.py`) | Em C (`lexer.c`/`lexer.h`) |
|---|---|---|
| Estado do lexer | atributos de `self` (`pos`, `line`, `column`, `tokens`, `errors`) | campos da struct `Lexer` |
| Token | `@dataclass Token` | `struct Token`, com `TokenValue` (union) no lugar do campo `value: object` do Python |
| Lista de tokens/erros | `list` que cresce com `.append` | array que dobra de tamanho a cada `realloc` quando enche (`tokens_push`/`errors_push`) — o equivalente manual de uma lista dinâmica |
| Palavras reservadas / símbolos | `dict` (`KEYWORDS`, `TWO_CHAR_SYMBOLS`, `ONE_CHAR_SYMBOLS`) | array de structs (`KeywordEntry`, `TwoCharEntry`, `OneCharEntry`) percorrido com busca linear (`strcmp`/comparação de char) |
| Lexema (string do token) | `str` imutável, gerenciada pelo GC | `char *` alocado com `malloc`/`strbuf_*`; o `Token` é dono da string e ela é liberada em `lexer_free` |
| Formatação de erro (`f"..."`) | f-string | `format_string()` (um `vsnprintf` de duas passadas: mede o tamanho, aloca, formata) |

A lógica de reconhecimento em si — cursor com `_peek`/`_advance` mantendo
linha/coluna, roteamento pelo primeiro caractere
(`_scan_token`/`lexer_scan_token`), maximal munch nos operadores de dois
caracteres, keyword-antes-de-identificador, `.` só vira `FLOAT` se seguido de
dígito, modo pânico nos erros — é a mesma função por função nas duas
implementações; os comentários de `lexer.c` remetem aos trechos equivalentes
de `lexer.py` sempre que a tradução não é direta (ex.: por que `read_escape`
sempre consome um caractere mesmo em erro, para não travar em loop).

## Gerenciamento de memória

Diferente da versão Python (onde o GC cuida de tudo), aqui cada alocação tem
um dono explícito:

- `Token.lexeme` e `LexError.message` são `malloc`ados e liberados em
  `lexer_free` — quem usa os tokens/erros **depois** de chamar `lexer_free`
  está lendo memória já liberada.
- O `char *source` passado para `lexer_init` **não** é copiado nem liberado
  pelo lexer — continua sendo responsabilidade de quem o alocou (em
  `main.c`, o próprio `read_file`/`free(source)`).
- `token_to_string`/`lexerror_to_string` alocam uma nova string a cada
  chamada; quem chama é dono dela (`free()` depois de imprimir, como faz
  `main.c`).

## Por que sem `flex`/`bison`

Mesma motivação da versão Python (ver [`../python/README.md`](../python/README.md)):
o objetivo da disciplina é implementar o reconhecimento token a token na mão
(cursor, lookahead, maximal munch) e a análise sintática por descida recursiva,
não gerar scanner e parser a partir de ferramentas como `flex` e `bison`.

## Como o parser foi portado (e onde C obrigou a mudar de mecânica)

A estrutura é função por função a mesma de `parser.py`; três peças não têm
equivalente direto em C e por isso mudaram de forma:

| Conceito | Em Python (`parser.py` / `minic_ast.py`) | Em C (`parser.c` / `ast.c`) |
|---|---|---|
| Tipos de nó | uma `@dataclass` por nó, com campos nomeados (`If.cond`, `If.then`) | um `enum AstKind` + a struct única `Ast`, com filhos fixos `a`/`b`/`c`/`d` e a lista `items`; a tabela em [ast.h](ast.h) diz o que cada campo é em cada nó |
| Abortar a análise | `raise ParseError`, capturado com `try/except` no laço de itens | `setjmp`/`longjmp`: a mensagem fica em `parser->pending` e o salto leva ao ponto de recuperação. Um laço aninhado (o do bloco) salva e repõe o `jmp_buf` anterior, porque em C o ponto de retomada não se empilha sozinho como um `try` |
| Dono da memória | o coletor de lixo recolhe as subárvores abandonadas por um erro | o `Parser` guarda **todos** os nós que criou e libera um a um em `parser_free` (com `ast_free_node`, que é raso — os filhos também estão na lista). Consequência: usar a AST depois de `parser_free` é ler memória liberada, igual ao contrato do lexer |
| Lista de declaradores (`int a, b;`) | devolve um nó ou uma lista, e quem chama achata | recebe o nó de destino e empurra cada `VarDecl` direto nele — sem união de tipos nem retorno de lista |
| Montar strings de saída | f-strings e `"".join(...)` | um buffer `Buf` que dobra de tamanho (`buf_add`), para não fazer `malloc`+`strcat` quadrático |
| Mensagem de erro formatada | f-string | `parser_format()` (dois `vsnprintf`: mede, aloca, formata). O nome tem prefixo porque `lexer.c` já tem um `format_string` e os dois convivem na mesma unidade de tradução quando o `parser.c` da raiz inclui os dois fontes |

O laço genérico dos níveis binários também sobreviveu à tradução: em Python é
`_binario_esquerda(proximo_nivel, *operadores)`, em C é
`binario_esquerda(p, ponteiro_de_funcao, ops, n_ops)` — mesma ideia, com
ponteiro de função no lugar do parâmetro de função de primeira classe.
