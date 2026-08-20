# Tabela de tokens — MINIC

Tabela operacional usada pela implementação do lexer (Python em [`src/python/lexer.py`](../src/python/lexer.py),
e futuramente C). Baseada na Seção 3 de `ref/especificacao-completa-minic.pdf` — ver
[`docs/especificacao.md`](especificacao.md) para o resumo normativo.

## Formato de saída

Segue a notação do material de aula: `<TOKEN, lexema>` para tokens com atributo relevante
(identificadores e literais), e apenas o nome do token para os que não precisam de atributo
(palavras reservadas, operadores, delimitadores) — o próprio nome do token já identifica o
lexema.

```
<IDENT, total>
KW_INT
ASSIGN
<INT, 2>
PLUS
<INT, 3>
STAR
<INT, 4>
SEMICOLON
```

## Palavras reservadas → token

| Lexema | Token |
|---|---|
| `int` | `KW_INT` |
| `float` | `KW_FLOAT` |
| `bool` | `KW_BOOL` |
| `char` | `KW_CHAR` |
| `void` | `KW_VOID` |
| `if` | `KW_IF` |
| `else` | `KW_ELSE` |
| `while` | `KW_WHILE` |
| `for` | `KW_FOR` |
| `return` | `KW_RETURN` |
| `break` | `KW_BREAK` |
| `continue` | `KW_CONTINUE` |
| `true` | `KW_TRUE` |
| `false` | `KW_FALSE` |
| `print` | `KW_PRINT` |
| `read` | `KW_READ` |

Prefixo `KW_` evita colisão com os tokens de literal (`int` a palavra reservada vs. `INT` o
token de literal inteiro).

## Identificadores e literais (tokens com atributo)

| Token | Regex sugerida | Exemplos | Ação do lexer |
|---|---|---|---|
| `IDENT` | `[A-Za-z_][A-Za-z0-9_]*` | `contador`, `_x`, `soma2` | Verificar antes se é palavra reservada; senão, `IDENT` |
| `INT` | `[0-9]+` | `0`, `42`, `2026` | Converter para inteiro |
| `FLOAT` | `[0-9]+\.[0-9]+` | `3.14`, `0.5` | Só consome o `.` se seguido de dígito; senão o número fica `INT` |
| `CHAR` | `'([^\\']\|\\[ntr\\'"])'` | `'a'`, `'\n'` | Exatamente 1 caractere (ou 1 escape) entre aspas simples |
| `STRING` | `"([^\\"]\|\\[ntr\\'"])*"` | `"MINIC"`, `"a\nb"` | Decodificar escapes; parar em `"` fechando ou erro se EOF/quebra de linha antes |

## Operadores e delimitadores (sem atributo)

| Token | Lexema | Token | Lexema |
|---|---|---|---|
| `PLUS` | `+` | `EQ` | `==` |
| `MINUS` | `-` | `NEQ` | `!=` |
| `STAR` | `*` | `LE` | `<=` |
| `SLASH` | `/` | `GE` | `>=` |
| `PERCENT` | `%` | `LT` | `<` |
| `AND` | `&&` | `GT` | `>` |
| `OR` | `\|\|` | `NOT` | `!` |
| `ASSIGN` | `=` | `LPAREN` / `RPAREN` | `(` / `)` |
| `LBRACKET` / `RBRACKET` | `[` / `]` | `LBRACE` / `RBRACE` | `{` / `}` |
| `SEMICOLON` | `;` | `COMMA` | `,` |

Ignorados (não geram token): espaços/tabs/`\r`/`\n`, comentário de linha `//...`, comentário de
bloco `/* ... */` (sem aninhamento).

## Prioridade das regras

1. **Operadores de dois caracteres antes dos de um caractere**: `==` antes de `=`, `<=` antes de
   `<`, `!=` antes de `!`, `>=` antes de `>`. Implementado testando o par de caracteres primeiro
   (maximal munch); se não bater com nenhum operador de dois caracteres, cai para o de um.
   `&&` e `||` só existem como par — um `&` ou `|` isolado é símbolo não reconhecido (MINIC não
   define operadores bit a bit de um caractere).
2. **Palavras reservadas antes de identificador**: o scanner sempre lê o lexema completo de
   letras/dígitos/`_` e só depois consulta a tabela de palavras reservadas; se não encontrar,
   classifica como `IDENT`.
3. **Real vs. inteiro**: dígitos são consumidos; o `.` só é consumido (virando `FLOAT`) se o
   caractere seguinte também for dígito — evita tratar `campo.metodo` (fora do escopo de MINIC,
   mas guarda a regra) ou um `.` isolado como parte do número.

## Erros léxicos e recuperação

Formato (Seção 12): `Erro léxico na linha <L>, coluna <C>: <mensagem>.`

| Situação | Mensagem |
|---|---|
| Símbolo não reconhecido | `símbolo "<c>" não reconhecido` |
| Cadeia não terminada (EOF/quebra de linha antes do `"` de fechamento) | `cadeia não terminada` |
| Literal de caractere não terminado | `literal de caractere não terminado` |
| Caractere com tamanho inválido (ex.: `'ab'`) | `caractere com tamanho inválido` |
| Comentário de bloco não terminado | `comentário de bloco não terminado` |
| Sequência de escape inválida | `sequência de escape inválida "\<c>"` |

**Modo pânico**: ao encontrar um caractere que não inicia nenhum token válido, o erro é
registrado na posição atual e o caractere é descartado; o scanner recomeça a busca por um token
válido a partir do próximo caractere. Isso permite reportar mais de um erro léxico por execução
em vez de abortar no primeiro.
