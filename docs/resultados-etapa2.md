# Resultados da etapa 2 (analisador sintático)

Registro da execução real das suítes, para a entrega — o enunciado pede "os
testes com os respectivos resultados". Os números abaixo são reproduzíveis com
os comandos indicados; as saídas gravadas de cada caso próprio ficam em
[`tests/parser_expected/`](../tests/parser_expected/).

Ambiente da execução: Windows 11, Python 3.14, gcc (MinGW-w64) com
`-Wall -Wextra -std=c11` (compilação sem nenhum aviso).

## Resumo

| Suíte | Python | C |
|---|---|---|
| 50 casos oficiais (`ref/testes-oficiais/testes-parser-50/`) | 50/50 | 50/50 |
| 15 casos próprios (`tests/parser_inputs/`) | 15/15 | 15/15 |
| Equivalência Python ↔ C (65 entradas, byte a byte) | 65/65 | — |
| **Total** | **195/195** | |

```bash
python tests/run_parser_tests.py
```

```text
Python — oficiais: 50/50
Python — próprios: 15/15
C — oficiais: 50/50
C — próprios: 15/15
Equivalência: 65/65 entradas com saída idêntica
Total: 195/195 verificações conformes.
```

Critério de cada suíte:

- **Oficiais 01–25 (ACEITO)**: código de saída 0, AST em stdout, stderr vazio e
  AST igual à de `ast.esperada.txt` (comparação normalizando espaços fora de
  lexemas — ver [ast.md](ast.md#por-que-não-dá-para-comparar-byte-a-byte)).
  O caso 24 é comparado contra a AST oficial **corrigida**, porque o arquivo do
  pacote tem um `)` faltando; isso aparece explicitamente no relatório do
  runner.
- **Oficiais 26–50 (REJEITADO)**: código de saída 3, nenhuma AST impressa e
  pelo menos um diagnóstico no formato `Erro sintático na linha L, coluna C: …`.
- **Próprios**: comparação byte a byte de stdout, stderr e código de saída com
  os golden gravados.
- **Equivalência**: as duas implementações têm de devolver exatamente o mesmo
  stdout, stderr e código de saída para cada uma das 65 entradas.

## Scripts oficiais da disciplina

```bash
bash src/python/test_parser_python.sh   # embrulha ref/scripts/testar_parser_python.sh
bash src/c/test_parser_c.sh             # embrulha ref/scripts/testar_parser_c.sh
```

As duas implementações produzem **o mesmo resumo**:

```text
Total de casos:       50
Aprovados:            16
Reprovados:           34
Erros sintáticos:     25
Erros de execução:    0
Esperados ausentes:   0
```

Leitura desses números — 16/50 é o **máximo alcançável** neste pacote, e não
falha do parser:

- **25 reprovados são impossíveis por construção.** Para os casos 26–50 o
  script compara a saída do parser com `ast.esperada.txt`, que contém a frase
  "NÃO HÁ AST: o parser deve rejeitar a entrada." — nenhum parser imprime isso.
  O que mostra que esses casos foram tratados corretamente é a linha
  **`Erros sintáticos: 25`**: todos os 25 foram detectados e diagnosticados.
- **9 reprovados diferem só em espaço em branco.** Os arquivos oficiais dos
  casos válidos usam dois estilos de espaçamento para o mesmo construto
  (`VarDecl(int x = Lit(int,42))` no caso 02, `VarDecl(int i=Lit(int,0))` no
  caso 10), então nenhum impressor determinístico casa com todos. A regra
  canônica adotada casa exatamente com 16 e difere dos outros 9 apenas em
  espaços — o que a comparação normalizada de `run_parser_tests.py` confirma
  (50/50).
- **`Erros de execução: 0`** confirma que nenhum caso quebrou o parser
  (nenhuma exceção, nenhum crash, nenhum código de saída inesperado).

Detalhamento dos defeitos do pacote em
[`ref/testes-oficiais/README.md`](../ref/testes-oficiais/README.md#defeitos-conhecidos-do-pacote).

> Se o contador "Erros sintáticos" aparecer como 0, o locale do shell está em
> `C`: o `grep` do script oficial procura por "sintático" e só casa com o "á"
> em locale UTF-8. Os wrappers em `src/` já exportam `LC_ALL=C.UTF-8`.

## Etapa 1 continua verde

A etapa 2 reaproveita o lexer sem alterá-lo; as três suítes da etapa 1 seguem
passando:

```bash
python tests/run_tests.py                 # 9/9 testes passaram
bash src/python/test_scanner_python.sh    # 9 OK, 0 falha(s)
bash src/c/test_scanner_c.sh              # 9 OK, 0 falha(s)
```

## Exemplos de saída

AST compacta (caso oficial 25, "programa integrado"):

```bash
python parser.py ref/testes-oficiais/testes-parser-50/casos/25_programa_integrado/codigo.c
```

```text
Program(Function(int soma(int a,int b) Block(Return(Binary(+,Id(a),Id(b))))), Function(int main() Block(VarDecl(int v size=Lit(int,3)), ExprStmt(Assign(Index(Id(v),Lit(int,0)),Call(Id(soma),Lit(int,1),Lit(int,2)))), While(Binary(<,Index(Id(v),Lit(int,0)),Lit(int,5)),Block(ExprStmt(Assign(Index(Id(v),Lit(int,0)),Binary(+,Index(Id(v),Lit(int,0)),Lit(int,1)))))), Return(Index(Id(v),Lit(int,0))))))
```

AST indentada (`--tree`), útil para conferir a estrutura a olho:

```bash
python parser.py --tree tests/parser_inputs/valido_print_read.c
```

```text
Program
  Function returnType=int name=main
    Block
      VarDecl type=int name=valores
        size
          Lit type=int value=5
      VarDecl type=int name=i
        Lit type=int value=0
      Read
        Index
          Id name=valores
          Id name=i
      ...
```

Diagnóstico de erro sintático, com recuperação (vários erros numa execução —
`tests/parser_inputs/erro_multiplos.c`):

```text
Erro sintático na linha 3, coluna 5: esperado PONTO_E_VIRGULA; encontrado KW_INT.
Erro sintático na linha 3, coluna 13: esperado expressão (identificador, literal ou ABRE_PAREN); encontrado PONTO_E_VIRGULA.
Erro sintático na linha 4, coluna 15: esperado FECHA_PAREN; encontrado ABRE_CHAVE.
```

Código de saída 3 (erro sintático), 2 (erro léxico — o parser não roda) e 0
(aceito), conforme a Seção 11.1 da especificação.

## Cobertura dos diagnósticos dos 25 casos inválidos

Mensagem emitida em cada caso oficial de rejeição (a pista do pacote está entre
parênteses quando difere da nossa formulação):

| Caso | Diagnóstico emitido |
|---|---|
| 26 | esperado PONTO_E_VIRGULA; encontrado fim do arquivo |
| 27 | esperado PONTO_E_VIRGULA; encontrado KW_RETURN |
| 28 | esperado FECHA_PAREN; encontrado ABRE_CHAVE |
| 29 | esperado ABRE_CHAVE; encontrado KW_RETURN |
| 30 | esperado FECHA_CHAVE; encontrado fim do arquivo |
| 31 | esperado KW_\<tipo\> ou FECHA_PAREN; encontrado ABRE_CHAVE |
| 32 | esperado KW_\<tipo\> ou FECHA_PAREN; encontrado IDENT |
| 33 | esperado IDENT; encontrado VIRGULA |
| 34 | esperado KW_\<tipo\> ou FECHA_PAREN; encontrado VIRGULA |
| 35 | esperado expressão (identificador, literal ou ABRE_PAREN); encontrado PONTO_E_VIRGULA |
| 36 | esperado expressão (…); encontrado PONTO_E_VIRGULA |
| 37 | esperado expressão (…); encontrado SLASH |
| 38 | esperado expressão (…); encontrado ATRIBUICAO |
| 39 | esperado FECHA_PAREN; encontrado PONTO_E_VIRGULA |
| 40 | esperado expressão (…); encontrado FECHA_PAREN |
| 41 | lado esquerdo da atribuição não é atribuível (pista do pacote: esperado FECHA_COLCHETE) |
| 42 | esperado expressão (…); encontrado FECHA_COLCHETE |
| 43 | esperado início de statement; encontrado FECHA_CHAVE |
| 44 | token inesperado no início de statement; encontrado KW_ELSE |
| 45 | esperado PONTO_E_VIRGULA; encontrado FECHA_CHAVE |
| 46 | esperado IDENT; encontrado PONTO_E_VIRGULA |
| 47 | esperado IDENT; encontrado PONTO_E_VIRGULA |
| 48 | esperado ABRE_CHAVE; encontrado PONTO_E_VIRGULA |
| 49 | token inesperado no nível do programa; encontrado FECHA_CHAVE |
| 50 | esperado expressão (…); encontrado FECHA_PAREN |

Os rótulos (`PONTO_E_VIRGULA`, `FECHA_PAREN`, `ABRE_CHAVE`, …) são de propósito
os mesmos que o pacote oficial usa nas pistas, para a conferência ser direta.

Sobre o caso 41 (`a[1 = 2;`): a nossa mensagem aponta o `=` na coluna 28 e diz
que `1` não é um destino de atribuição válido. A pista do pacote esperava
`FECHA_COLCHETE`, mas a gramática **permite** atribuição dentro de um índice
(`expressao ::= atribuicao`), então o primeiro problema real da entrada é o
alvo inválido, e é ele que o diagnóstico reporta — na posição exata.
