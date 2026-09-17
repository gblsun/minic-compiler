# src/c/

Implementação em C do analisador léxico do MINIC — mesma lógica de
[`src/python/lexer.py`](../python/lexer.py), portada à mão (sem `re`/`lex`/`flex`)
para produzir exatamente os mesmos tokens e as mesmas mensagens de erro.

| Arquivo | O que é |
|---|---|
| [lexer.h](lexer.h) | Interface pública: `TokenType` (prefixo `TOK_`, para não colidir com macros do C como `EOF` de `<stdio.h>`), `Token`, `LexError`, `Lexer`, e as assinaturas de `lexer_init`/`lexer_tokenize`/`lexer_free`/`token_to_string`/`lexerror_to_string`. |
| [lexer.c](lexer.c) | Implementação: as mesmas tabelas do `lexer.py` (`KEYWORDS`, `TWO_CHAR_SYMBOLS`, `ONE_CHAR_SYMBOLS`, escapes) e a mesma máquina de reconhecimento caractere a caractere (cursor com linha/coluna, maximal munch, modo pânico). |
| [main.c](main.c) | CLI mínima: `./scanner <arquivo.mc>`. Lê o arquivo, roda o `Lexer` e imprime tokens em stdout / erros em stderr, com o mesmo contrato de exit code de `main.py` (0 = ok, 2 = erro léxico, 1 = erro de uso). |
| [test_scanner_c.sh](test_scanner_c.sh) | Script de teste (formato pedido pela disciplina — ver o original em [`../../ref/scripts/test_scanner_c.sh`](../../ref/scripts/test_scanner_c.sh), adaptado aqui para compilar este scanner e comparar com `tests/expected/`; ver [`tests/README.md`](../../tests/README.md) para como os três runners de teste do projeto se encaixam). |

## Compilando e rodando

```bash
gcc -Wall -Wextra -std=c11 src/c/lexer.c src/c/main.c -o src/c/scanner
./src/c/scanner tests/inputs/valido_soma.mc
```

Saída idêntica à da versão Python (mesmo formato de token, mesmas mensagens de
erro, mesmo exit code) — ver o [README.md](../../README.md) da raiz para o
contrato completo de stdout/stderr/exit code.

## Rodando os testes

```bash
bash src/c/test_scanner_c.sh
```

Compila o scanner com `gcc -Wall -Wextra -std=c11`, roda o binário sobre cada
`.mc` de `tests/inputs/` e compara com `tests/expected/` (os mesmos arquivos
golden usados pela versão Python) — é assim que se verifica na prática o
requisito de que as duas implementações produzam os mesmos tokens e os mesmos
erros para os mesmos arquivos de entrada.

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

## Por que sem `re`/`flex`

Mesma motivação da versão Python (ver [`../python/README.md`](../python/README.md)):
o objetivo da disciplina é implementar o reconhecimento token a token na mão
(cursor, lookahead, maximal munch), não gerar um scanner a partir de uma
ferramenta como `flex`.
