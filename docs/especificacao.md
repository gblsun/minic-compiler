# Especificação léxica — MINIC

> Resumo operacional da Seção 3 ("Especificação léxica") de
> [`ref/especificacao-completa-minic.pdf`](../ref/especificacao-completa-minic.pdf) (v1.0),
> mais os pontos das Seções 11.1, 12 e 13.2 relevantes para o lexer. Em caso de dúvida ou
> divergência, o PDF é a fonte normativa — este arquivo é só um guia rápido para implementar
> e revisar o scanner.

## 3.1 Convenções gerais

- MINIC diferencia maiúsculas de minúsculas.
- Espaços, tabulações e quebras de linha são ignorados fora de literais.
- O lexer deve preservar **linha e coluna** de cada token, para diagnósticos.

## 3.2 Comentários

- Comentário de linha: `//` até o fim da linha.
- Comentário de bloco: `/* ... */`.
- Comentários **não podem ser aninhados**.

## 3.3 Palavras reservadas

| Palavra | Finalidade |
|---|---|
| `int` | Inteiro |
| `float` | Real |
| `bool` | Booleano |
| `char` | Caractere |
| `void` | Sem retorno |
| `if` / `else` | Condicional |
| `while` / `for` | Repetição |
| `return` | Retorno |
| `break` / `continue` | Controle de laços |
| `true` / `false` | Literais booleanos |
| `print` / `read` | Saída e entrada |

Palavras reservadas têm prioridade sobre identificadores: `if` nunca vira `IDENT`.

## 3.4 Tokens obrigatórios

| Categoria | Expressão ou exemplos |
|---|---|
| Identificador | `[A-Za-z_][A-Za-z0-9_]*` |
| Inteiro | `[0-9]+` |
| Real | `[0-9]+\.[0-9]+` |
| Caractere | `'a'`, `'\n'` |
| Cadeia | `"texto"` |
| Aritméticos | `+ - * / %` |
| Relacionais | `== != < > <= >=` |
| Lógicos | `&& \|\| !` |
| Atribuição | `=` |
| Delimitadores | `( ) [ ] { } ; ,` |

## 3.5 Identificadores, literais e escapes

- Identificadores começam por letra ou `_` e continuam com letras, dígitos ou `_`.
- Inteiros são uma sequência de dígitos; reais têm parte inteira **e** decimal (`[0-9]+\.[0-9]+` —
  não há forma abreviada como `.5` ou `5.`).
- Caracteres usam aspas simples e devem conter exatamente um caractere (ou uma sequência de
  escape); cadeias usam aspas duplas.

| Escape | Significado |
|---|---|
| `\n` | Nova linha |
| `\t` | Tabulação |
| `\\` | Barra invertida |
| `\'` | Aspas simples |
| `\"` | Aspas duplas |

## Erros léxicos (Seções 11.1, 12 e 13.2)

- Código de saída da ferramenta para erro léxico: **2** (0 = sucesso, 1 = uso inválido, 3 =
  sintático, 4 = semântico, 5 = geração de código, 6 = execução).
- Todo diagnóstico informa categoria, linha, coluna e o lexema/token relevante. Formato adotado
  (Seção 12):

  ```
  Erro léxico na linha 3, coluna 12: símbolo "@" não reconhecido.
  ```

- Casos inválidos obrigatórios de origem léxica (Seção 13.2): símbolo não reconhecido, literal
  (string ou char) não terminado, caractere com tamanho inválido (ex.: `'ab'`).
- Recuperação: modo pânico — ao encontrar um símbolo não reconhecido, o erro é reportado e o
  caractere é descartado; o scanner continua daí, permitindo relatar mais de um erro por execução
  sem mascarar a causa original.

Ver também [`docs/tokens.md`](tokens.md) para a tabela operacional token → regex → ação do
lexer usada na implementação, e o formato de saída dos tokens.
