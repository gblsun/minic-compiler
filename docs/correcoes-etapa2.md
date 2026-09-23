# Correções da etapa 2 (resposta à avaliação do parser)

Resposta, item por item, à avaliação da atividade "AP 1 — analisador
sintático". Para cada apontamento: o que foi reproduzido, a causa encontrada, o
que mudou no código e como conferir. Os resultados completos das suítes estão
em [resultados-etapa2.md](resultados-etapa2.md).

## Resumo

| Apontamento da avaliação | Causa | Situação |
|---|---|---|
| C: 0/50, "integração não executável", falta `./scanner` e instruções de compilação | o `parser.c` da raiz, que o script oficial compila, tinha sido substituído por um `main.c` | **corrigido**: `parser.c` restaurado, instruções no README |
| C: `popen()` com o nome do arquivo no comando (espaços, injeção, buffer de 1024) | esse código não existe no repositório (ver abaixo) | **não se aplica**: o parser C chama o lexer no mesmo processo |
| Python: 0 erros sintáticos e 25 erros de execução | `grep` do script oficial não casa "sintático" em locale `C` | **corrigido**: diagnóstico passou a ser "Erro de sintaxe" |
| Python: falhas no tratamento de erros léxicos | lexer aceitava letras e dígitos não-ASCII; traceback com UTF-8 inválido | **corrigido** |
| Python: dependência frágil do diretório de execução | import por nome (`from parser import main`) via `sys.path` | **corrigido**: import por caminho de arquivo |
| Parser robusto | aninhamento profundo derrubava os dois parsers | **corrigido**: limite de 200 níveis com diagnóstico |
| Python: 9 reprovados | defeitos do próprio pacote de testes (espaçamento e caso 24) | **explicado**: não há saída que case com todos |

Resultado do script oficial depois das correções, **igual nas duas
implementações** e em qualquer locale (`C`, `POSIX` ou `C.UTF-8`):

```text
Total de casos:       50
Aprovados:            16
Reprovados:           34
Erros sintáticos:     25
Erros de execução:    0
Esperados ausentes:   0
```

No critério da avaliação, em que os 25 casos rejeitados contam como aprovados,
isso corresponde a **41/50 nas duas implementações**, e os 9 restantes são os
casos explicados na última seção.

---

## 1. C: 0/50 e "parser espera um executável ./scanner"

**Causa.** O script oficial `testar_parser_c.sh` compila **um único arquivo**:

```bash
gcc -Wall -Wextra -std=c11 ./parser.c -o ./parser
```

O repositório tinha um `parser.c` na raiz que inclui os fontes de `src/c/` (o
lexer da etapa 1, a AST, o parser e a CLI), justamente para essa linha
funcionar. Um commit posterior trocou esse arquivo por um `main.c`. Sem o
`parser.c`, o `gcc` falhava e os 50 casos viravam "erro de execução".

**Correção.** O `parser.c` da raiz foi restaurado, e o `main.c` foi removido
(ele duplicava `src/c/parser_main.c`). A compilação e a execução estão
documentadas no [README](../README.md#rodar-o-parser-comandos-do-enunciado):

```bash
gcc -Wall -Wextra -std=c11 parser.c -o parser
./parser codigo.c
```

Compila sem nenhum aviso com `-Wall -Wextra -std=c11` e também com
`-pedantic`, no gcc 14 em Linux.

## 2. `popen()` e o nome do arquivo no comando

A avaliação descreve um parser C que chama `popen()` com o nome do arquivo
montado num buffer de 1024 caracteres. **Esse código não existe neste
repositório**, nem no estado atual nem em nenhum commit do histórico
(`git log --all -S popen` não encontra nada).

O parser C não executa nenhum programa externo. O `parser.c` junta lexer e
parser numa única unidade de tradução, o arquivo é lido com `fopen` e os
tokens passam por memória (`lexer_tokenize` → `parser_parse`). Por isso:

- nome de arquivo com espaço ou caractere especial funciona (testado com
  `"meu teste.c"`);
- não existe shell envolvido, então não há como injetar comando;
- o caminho vem direto de `argv`, sem buffer de tamanho fixo;
- não depende do diretório atual nem de um `./scanner` compilado à parte.

A hipótese mais provável é que o apontamento tenha vindo da falha de
compilação do item 1, que deixava o C sem executável.

## 3. Python: "Erros sintáticos: 0" e "Erros de execução: 25"

**Causa.** O script oficial decide se houve erro sintático com:

```bash
grep -Eiq 'erro[[:space:]_-]*(sint[aá]tico|de sintaxe)|syntax[[:space:]_-]*error|parse[[:space:]_-]*error'
```

Em locale `C`/`POSIX`, o `[aá]` é lido byte a byte, e o "á" em UTF-8 ocupa dois
bytes. A mensagem antiga, "Erro sintático", **não casa**. Os 25 casos eram
rejeitados corretamente (exit 3), mas o script os contava como erro de
execução. Reprodução:

```bash
printf 'Erro sintático na linha 1\n' | LC_ALL=C grep -Eiq 'erro[[:space:]_-]*(sint[aá]tico|de sintaxe)' && echo casa || echo "não casa"
# não casa
```

**Correção.** Os diagnósticos agora começam com **"Erro de sintaxe"**, só
ASCII, nas duas implementações (`src/python/parser.py`, `src/c/parser.c`):

```text
Erro de sintaxe na linha 2, coluna 1: esperado FECHA_CHAVE; encontrado fim do arquivo.
```

## 4. Tratamento de erros léxicos

| Problema | Antes | Agora |
|---|---|---|
| Identificador com letra não-ASCII (`int ação;`) | o Python aceitava (`str.isalpha()` é Unicode); o C rejeitava | os dois rejeitam, com erro léxico e exit 2 |
| Dígito não-ASCII (`int x = ٣;`) | o Python aceitava e gerava `Lit(int,٣)` | erro léxico nos dois |
| Caractere não-ASCII na mensagem do C | um erro por byte, com bytes soltos (UTF-8 inválido na saída) | um erro por caractere, com o caractere inteiro, igual ao Python |
| Arquivo com UTF-8 inválido | o Python abortava com `UnicodeDecodeError` (traceback) | erro léxico `símbolo "\xFF" não reconhecido` nos dois |

A especificação (Seção 3.5) define identificador como `[A-Za-z_][A-Za-z0-9_]*`
e inteiro como `[0-9]+`. O lexer Python agora usa checagens ASCII explícitas
(`_eh_letra` e `_eh_digito`, em `src/python/lexer.py`).

## 5. Dependência do diretório de execução (Python)

**Antes.** O `parser.py` da raiz punha `src/python/` no `sys.path` e fazia
`from parser import main`, um import **por nome**. Esse nome colide com o
próprio arquivo e, em Python ≤ 3.9, com o módulo embutido `parser`.

**Agora.**

- o `parser.py` da raiz carrega `src/python/parser.py` **pelo caminho do
  arquivo** (`importlib.util.spec_from_file_location`), calculado a partir de
  `__file__`;
- `src/python/parser.py` põe o próprio diretório no `sys.path` antes de
  importar `lexer` e `minic_ast`.

Com isso, o parser funciona chamado de qualquer diretório:

```bash
cd /tmp && python /caminho/do/repo/parser.py /caminho/do/caso/codigo.c
```

## 6. Robustez: aninhamento profundo

A descida recursiva gasta pilha a cada nível. Uma entrada com ~100 parênteses
aninhados derrubava o Python com `RecursionError` (traceback), e uma com
milhares derrubava o C com estouro de pilha.

**Correção.** Os dois parsers contam o aninhamento (atribuições, unários e
comandos dentro de comandos) com o mesmo limite, `LIMITE_ANINHAMENTO = 200`.
Acima dele, a análise para com um único diagnóstico e exit 3:

```text
Erro de sintaxe na linha 1, coluna 209: aninhamento acima do limite de 200 níveis; encontrado ABRE_PAREN.
```

## 7. Os 9 casos reprovados que sobram

Os casos **02, 03, 04, 06, 07, 08, 09, 10 e 24** continuam aparecendo como
reprovados no script oficial. Não é erro do parser, e sim do pacote de testes:

- **Espaçamento inconsistente.** O mesmo construto aparece com dois estilos:
  `VarDecl(int x = Lit(int,42))` no caso 02 e `VarDecl(int i=Lit(int,0))` no
  caso 10; `Binary(+, Lit(int,2), …)` no caso 03 e `Binary(+,Id(i),…)` no caso
  10. Um impressor determinístico produz um estilo só, então não pode casar
  byte a byte com os dois.
- **Caso 24 com AST inválida.** Falta um `)` no `ast.esperada.txt`, e o
  `Return(Id(i))` fica fora do corpo da função. Nenhum parser correto gera
  esse texto.

Comparando a estrutura (espaços normalizados, caso 24 corrigido), os 50 casos
passam nas duas implementações. O detalhamento está em
[`ref/testes-oficiais/README.md`](../ref/testes-oficiais/README.md#defeitos-conhecidos-do-pacote).

## Como conferir

```bash
# script oficial, no locale em que a avaliação rodou
LC_ALL=C bash ref/scripts/testar_parser_python.sh ref/testes-oficiais/testes-parser-50 ./parser.py
LC_ALL=C bash ref/scripts/testar_parser_c.sh      ref/testes-oficiais/testes-parser-50 ./parser.c

# suíte completa: 50 oficiais + 17 próprios em cada implementação + equivalência
python3 tests/run_parser_tests.py        # Total: 201/201 verificações conformes.
```

Casos de regressão novos, em [`tests/parser_inputs/`](../tests/parser_inputs/):

- `erro_identificador_nao_ascii.c`: identificador e dígito fora do ASCII (erro
  léxico, exit 2);
- `erro_aninhamento_profundo.c`: 250 parênteses aninhados (erro de sintaxe,
  exit 3, sem traceback).
