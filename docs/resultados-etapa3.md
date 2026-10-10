# Resultados da etapa 3 — analisador semântico

Registro da execução das suítes da etapa 3 no estado atual do repositório. A
versão C foi compilada e executada no CI do GitHub (Ubuntu, gcc e Python 3.12,
[`.github/workflows/testes.yml`](../.github/workflows/testes.yml)); a versão
Python também foi executada no Windows 11 com Python 3.14.

## Scripts oficiais da AP 2

```
$ bash ref/scripts/testes_semanticos_py.sh ./minic.py ref/testes-oficiais/testes-semanticos-20
[OK]     ref/testes-oficiais/testes-semanticos-20/01_promocao_numerica.c
...
[OK]     ref/testes-oficiais/testes-semanticos-20/20_destino_nao_atribuivel.c

Resultado: 20/20 aprovados; 0 reprovados.

$ bash ref/scripts/testes_semanticos_c.sh ./minic.c ref/testes-oficiais/testes-semanticos-20
Compilando o analisador com gcc...
[OK]     ref/testes-oficiais/testes-semanticos-20/01_promocao_numerica.c
...
Resultado: 20/20 aprovados; 0 reprovados.
```

O `minic.c` compila sem nenhum aviso com `gcc -Wall -Wextra` (o comando do
script), com `-std=c11 -pedantic` e também com os fontes de `src/c/` compilados
separadamente. O CI usa `-Werror` nas três formas.

## Suíte do projeto (`tests/run_semantic_tests.py`)

```
Python — oficiais: 20/20
Python — próprios: 16/16
C — oficiais: 20/20
C — próprios: 16/16
Equivalência: 103/103 entradas com saída idêntica
Total: 175/175 verificações conformes.
```

A equivalência roda pelo pipeline completo (`minic`) os 20 casos oficiais da
AP 2, os 16 casos próprios da etapa 3, os 50 casos oficiais do parser e os 17
casos próprios da etapa 2, e compara stdout, stderr e código de saída byte a
byte.

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
antigas do lexer C, invisíveis até então porque nenhum teste tinha acentos
antes de um token:

1. **Coluna depois de caractere acentuado.** Dentro de cadeias, literais de
   caractere e comentários, o C contava bytes e o Python contava caracteres. Em
   `print("ação"); n = 2.5;`, o SEM003 saía na coluna 26 no C e na 24 no
   Python. Agora os dois contam caracteres (pendência 1.5 do
   [PENDENCIAS.md](../PENDENCIAS.md)).
2. **Literal `'é'`.** O Python aceitava como um caractere e o C rejeitava com
   "caractere com tamanho inválido" (eram 2 bytes). Agora os dois aceitam.

O caso `tests/semantic_inputs/erro_colunas_apos_utf8.c` cobre as duas.

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
