# Resultados da etapa 3 — analisador semântico

Registro da execução das verificações da etapa 3 no estado atual do
repositório. A versão C é compilada e executada no CI do GitHub
([`.github/workflows/testes.yml`](../.github/workflows/testes.yml)), em dois
ambientes: Ubuntu 22.04 (gcc 11.4, Python 3.10) e Ubuntu 24.04 (gcc 13.3,
Python 3.12, clang 18). A versão Python também foi executada no Windows 11 com
Python 3.14. O resumo por critério de correção está em
[entrega-ap2.md](entrega-ap2.md).

## Última execução completa

**10/10/2026**, na `main` depois do merge da auditoria (PR #2). O CI foi
disparado manualmente
([execução 38053562411](https://github.com/gblsun/minic-compiler/actions/runs/38053562411)),
e a versão Python também rodou localmente. **Tudo passou:**

| Verificação | Python | C |
|---|---|---|
| script oficial da AP 2 (Ubuntu 22.04 e 24.04, e pasta com espaço e acento em `LC_ALL=C`) | 20/20 | 20/20 |
| casos oficiais + próprios (`run_semantic_tests.py`) | 39/39 | 39/39 |
| equivalência byte a byte, programas fixos | 106/106 | (idem) |
| teste diferencial, programas gerados (C com sanitizers) | 900/900 | (idem) |
| AddressSanitizer + UBSan nas entradas de teste | — | 120/120 |
| compilação `-Wall -Wextra -Werror` (gcc 11, gcc 13, clang 18) | — | sem avisos |
| catálogo `docs/testes-semanticos.md` em dia | sim | — |
| etapas 1 e 2 e exercícios da aula 13 | sem regressão | sem regressão |

As seções abaixo detalham cada verificação.

## Scripts oficiais da AP 2

```
$ bash ref/scripts/testes_semanticos_py.sh minic.py ref/testes-oficiais/testes-semanticos-20
[OK]     ref/testes-oficiais/testes-semanticos-20/01_promocao_numerica.c
...
[OK]     ref/testes-oficiais/testes-semanticos-20/20_destino_nao_atribuivel.c

Resultado: 20/20 aprovados; 0 reprovados.

$ bash ref/scripts/testes_semanticos_c.sh minic.c ref/testes-oficiais/testes-semanticos-20
Compilando o analisador com gcc...
[OK]     ref/testes-oficiais/testes-semanticos-20/01_promocao_numerica.c
...
Resultado: 20/20 aprovados; 0 reprovados.
```

Mesmo resultado nos dois ambientes, e também com o repositório copiado para
`/tmp/Avaliação Parcial II/minic compiler` (espaço e acento no caminho, como a
pasta em que o enunciado veio), em locale `C`.

## Suíte do projeto (`tests/run_semantic_tests.py`)

```
Python — oficiais: 20/20
Python — próprios: 19/19
C — oficiais: 20/20
C — próprios: 19/19
Equivalência: 106/106 entradas com saída idêntica
Total: 184/184 verificações conformes.
```

A equivalência roda pelo pipeline completo (`minic`) os 20 casos oficiais da
AP 2, os 19 casos próprios da etapa 3, os 50 casos oficiais do parser e os 17
casos próprios da etapa 2, e compara stdout, stderr e código de saída byte a
byte. O código, o objetivo e a saída de cada caso estão em
[testes-semanticos.md](testes-semanticos.md).

## Teste diferencial (`tests/fuzz_semantico.py`)

```
Programas gerados: 900 — 300 aleatórios, 300 bem tipados (devem ser aceitos), 300 com um erro injetado
Erros injetados: SEM001×20, SEM002×16, SEM003×15, SEM004×14, SEM005×25, SEM006×15, SEM007×14, SEM008×20, SEM009×20, SEM010×17, SEM011×20, SEM012×30, SEM013×22, SEM014×17, SEM015×16, SEM016×19
Resultado (Python × C): 900/900 programas sem falha
```

O lado C é o binário compilado com AddressSanitizer e UBSan. "Sem falha"
significa três coisas:

- nos 300 aleatórios, saída idêntica entre as versões e nenhum crash ou
  traceback;
- os 300 bem tipados são aceitos pelas duas;
- nos 300 com um erro injetado, as duas reportam exatamente o código injetado
  e mais nenhum.

## Compilação e memória

| Verificação | Resultado |
|---|---|
| `gcc -Wall -Wextra -Werror minic.c` (gcc 11 e 13) | sem avisos |
| `gcc -Wall -Wextra -Werror -std=c11 -pedantic minic.c` | sem avisos |
| fontes de `src/c/` compilados separadamente, com `-Werror` | sem avisos |
| `clang -Wall -Wextra -Werror minic.c` (clang 18) | sem avisos |
| 120 entradas de teste do repositório com `-fsanitize=address,undefined` | nenhum erro de memória, vazamento ou comportamento indefinido |

## Suítes das etapas anteriores e exercícios

Nada regrediu com a etapa 3:

| Suíte | Resultado |
|---|---|
| scanner (`tests/run_tests.py` e `src/c/test_scanner_c.sh`) | 9/9 em cada versão |
| parser (`tests/run_parser_tests.py`) | 201/201 |
| scripts oficiais do parser, `LC_ALL=C` | 16 aprovados, 25 erros sintáticos, 0 de execução (o teto do pacote, ver [resultados-etapa2.md](resultados-etapa2.md)) |
| exercícios da aula 13 (`exercicios/aula13/run_tests.py`) | 250/250 |

## Correções feitas no lexer C durante a etapa 3

A equivalência C × Python no pipeline semântico revelou duas divergências
antigas do lexer C. Nenhum teste anterior tinha acentos antes de um token,
então elas não apareciam:

1. **Coluna depois de caractere acentuado.** Dentro de cadeias, literais de
   caractere e comentários, o C contava bytes e o Python contava caracteres. Em
   `print("ação"); n = 2.5;`, o SEM003 saía na coluna 26 no C e na 24 no
   Python. Agora os dois contam caracteres (pendência 1.5 do
   [PENDENCIAS.md](../PENDENCIAS.md)).
2. **Literal `'é'`.** O Python aceitava como um caractere e o C rejeitava com
   "caractere com tamanho inválido" (eram 2 bytes). Agora os dois aceitam.

O caso `tests/semantic_inputs/erro_colunas_apos_utf8.c` cobre as duas.

Na mesma auditoria, o nó `Param` da AST passou a guardar a posição do **nome**
do parâmetro, e não a do tipo. A duplicidade de parâmetro (SEM002) passa a
apontar o nome, como o gabarito faz para variáveis (caso oficial 13). A AST
impressa não mostra posições, então a etapa 2 não muda.

## Diagnósticos dos 10 casos oficiais de rejeição

| Caso | Diagnósticos |
|---|---|
| 11 estreitamento implícito | SEM003 (3:9) |
| 12 identificador não declarado | SEM001 (3:9) |
| 13 declaração duplicada | SEM002 (3:11) |
| 14 condição não booleana | SEM005 (4:12) |
| 15 índice float | SEM006 (3:11) |
| 16 aridade incorreta | SEM007 (5:12) |
| 17 tipos de argumentos | SEM008 (5:20), SEM008 (5:27) |
| 18 uso de void como valor | SEM012 (6:9) |
| 19 retorno e cobertura | SEM009 (2:12), SEM011 (4:1) |
| 20 destino não atribuível | SEM013 (3:5) |

O texto completo de cada mensagem é o do `.gabarito` correspondente, e o
catálogo com as variantes está em [semantica.md](semantica.md#catálogo-de-diagnósticos).
