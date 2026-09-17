# ref/testes-oficiais/

Pacotes de teste **oficiais** distribuídos pelo professor, sem edição. Eles
não são a suíte que roda no dia a dia deste repositório (essa é a de
[`tests/`](../../tests/)) — são o critério externo contra o qual a entrega vai
ser avaliada, e por isso valem uma leitura atenta antes de cada etapa.

```text
ref/testes-oficiais/
├── testes-scanner-minic/    etapa 1 — tokens em JSONL (6 casos inválidos + 5 programas C)
├── testes-parser-50/        etapa 2 — 50 programas (25 aceitos + 25 rejeitados) com AST esperada
└── testes-parser-50.tar.gz  o pacote original, como veio do Classroom
```

O `.tar.gz` fica versionado junto do conteúdo extraído de propósito: o
extraído é o que se lê e se usa no dia a dia, e o compactado é a prova de que
nada foi alterado (`tar -tzf testes-parser-50.tar.gz` lista o original).

---

## testes-parser-50/ — etapa 2 (parser e AST)

Anexo da atividade "AP 1 - analisador sintático". 50 programas completos, em
diretórios numerados dentro de `casos/`:

| Arquivo em cada caso | Conteúdo |
|---|---|
| `codigo.c` | o programa de entrada (uma linha, na maioria dos casos) |
| `ast.esperada.txt` | a AST esperada em notação S-expression compacta — ou a frase `NÃO HÁ AST: o parser deve rejeitar a entrada.` |
| `resultado.esperado.txt` | `ACEITO`/`REJEITADO` e, nos rejeitados, a pista do diagnóstico esperado |

Na raiz do pacote: `README.md` (formato e notação), `INDICE.txt` (tabela
ID/status/título/diretório dos 50 casos), `EXECUCAO.txt` (os comandos
obrigatórios) e `manifesto.json` (os mesmos metadados em JSON).

- **Casos 01–25: ACEITO.** Devem produzir exatamente a AST de `ast.esperada.txt`.
- **Casos 26–50: REJEITADO.** Devem terminar com código de saída diferente de
  zero e diagnóstico sintático. A pista (`pista: esperado PONTO_E_VIRGULA`,
  `pista: token KW_ELSE inesperado`, `pista: esperado expressão`) indica o
  ponto de falha, mas o texto da mensagem "pode variar entre implementações",
  conforme o README do pacote.

A notação da AST, os nós usados e as armadilhas dela estão documentados em
[`docs/ast.md`](../../docs/ast.md). O subconjunto da gramática que esses 50
casos realmente exercitam está em
[`docs/gramatica.md`](../../docs/gramatica.md#o-que-os-50-casos-oficiais-cobrem)
— resumindo: **nenhum** dos 50 casos usa `for`, `print`, `read`, `break`,
`continue` ou literal de caractere/cadeia, embora todos sejam obrigatórios pela
especificação.

### Defeitos conhecidos do pacote

Encontrados ao conferir o material; não são erros de digitação deste README:

1. **Caso 24 (`24_la_o_com_express_o_complexa`) tem AST inválida.** Falta um
   `)`: o `While` fecha o `Block` da função antes do `Return`, deixando o
   `Return(Id(i))` fora do corpo. Contagem de parênteses:
   `(` = n, `)` = n−1. Nenhum parser correto reproduz esse arquivo byte a byte.
2. **O espaçamento da notação é inconsistente entre casos.** Há
   `VarDecl(int x = Lit(int,42))` (caso 02) e `VarDecl(int i=Lit(int,0))`
   (caso 10); `Binary(+, Lit(int,2), …)` (caso 03) e `Binary(+,Id(i),…)`
   (caso 10). Um impressor canônico — que é o que a apostila da aula 12 pede —
   produz **um** espaçamento só, então não bate byte a byte com os dois estilos.
3. **`ferramentas/` vem vazia** no `.tar.gz` (o git não versiona diretório
   vazio, então ela não aparece aqui).
4. Os nomes dos diretórios perderam os acentos na geração do pacote
   (`01_declara_o_inteira_simples` em vez de `01_declaracao_inteira_simples`) —
   inclusive o `README.md` do pacote cita o nome "correto", que não existe.
   Os scripts oficiais usam `find`, então isso não quebra nada.

Consequência prática: **comparar a AST byte a byte contra `ast.esperada.txt`
não é um critério viável.** A comparação precisa normalizar espaços em branco
fora de lexemas, e o caso 24 precisa de exceção explícita e documentada.

É o que [`tests/run_parser_tests.py`](../../tests/run_parser_tests.py) faz: os
50 casos passam nas duas implementações (50/50 cada), com o caso 24 comparado
contra a AST corrigida e isso dito no relatório. Já os scripts oficiais do
professor, que comparam byte a byte, param em 16 aprovados — o teto do pacote.
Os dois resultados, e como lê-los, estão em
[`docs/resultados-etapa2.md`](../../docs/resultados-etapa2.md).

---

## testes-scanner-minic/ — etapa 1 (scanner)

Fixtures do scanner em JSONL. Um token por linha:

```json
{"token":"INT_LIT","lexeme":"12","attribute":12,"line":1,"column":9}
```

Cada caso tem `<entrada>.expected.jsonl` com a sequência completa de tokens
(sempre terminada em `EOF`) e, nos inválidos, `<entrada>.errors.jsonl` com os
diagnósticos mínimos (`{"error":"UNKNOWN_SYMBOL","lexeme":"@","line":1,"column":7}`).
`check_fixtures.py` valida a forma das próprias fixtures (campos obrigatórios,
tipos de linha/coluna, presença de `EOF`) — não testa scanner nenhum:

```bash
python ref/testes-oficiais/testes-scanner-minic/check_fixtures.py
```

Conteúdo:

- `casos-invalidos/` — `i01` símbolo desconhecido, `i02` comentário de bloco
  não terminado, `i03` caractere não terminado, `i04` cadeia não terminada,
  `i05` real malformado, `i06` identificador iniciado por dígito.
- `casos-programas-c/` — cinco programas completos (`c01_fibonacci`,
  `c02_primos`, `c03_media_vetor`, `c04_menu_interativo`,
  `c05_controle_temperatura`) com a sequência integral de tokens esperada.

### O que falta no pacote

O `MANIFESTO.md` lista sete casos válidos (`v01_declaracoes` a
`v07_literais_opcionais`) e um sétimo inválido (`i07_operador_logico_incompleto`)
que **não vieram no download**: não existe `casos-validos/` e o `i06` veio sem o
`.minic` (só os dois `.jsonl`). Se precisar desses casos, é preciso rebaixar o
pacote no Classroom ou pedir ao professor. Os cinco programas em
`casos-programas-c/` cobrem, na prática, o papel dos casos válidos.

### Divergências entre estas fixtures e o nosso scanner

Nossas duas implementações (Python e C) seguem a notação do material de aula
(`<TOKEN, lexema>` em texto), e não o JSONL destas fixtures. Além do formato,
os **nomes** de token divergem:

| Conceito | Fixtures oficiais | Nosso scanner ([`docs/tokens.md`](../../docs/tokens.md)) |
|---|---|---|
| Palavra reservada | `INT`, `FLOAT`, `IF`, `WHILE`, `RETURN`, `PRINT`, `VOID`, … | `KW_INT`, `KW_FLOAT`, `KW_IF`, … (prefixo `KW_`) |
| Literal inteiro | `INT_LIT` (com `attribute` numérico) | `INT` |
| Literal real | `FLOAT_LIT` | `FLOAT` |
| Literal de caractere / cadeia | `CHAR_LIT` / `STRING_LIT` | `CHAR` / `STRING` |
| Saída | um objeto JSON por linha | `<TOKEN, lexema>` / `TOKEN` |

E uma divergência de comportamento, no caso `i05` (`valor = 12.;`):

- **Fixture oficial**: `INT_LIT(12)`, depois um token `DOT`, depois
  `SEMICOLON`, mais o diagnóstico `MALFORMED_REAL_LITERAL`.
- **Nosso scanner**: `<INT, 12>`, `SEMICOLON` e o erro
  `símbolo "." não reconhecido` (linha 1, coluna 11) — sem token para o ponto.

As duas rejeitam a entrada e apontam a mesma posição; o que difere é o
inventário de tokens. Note que `.` **não** é um token previsto na Seção 3.4 da
especificação, então `DOT` é uma invenção do pacote de fixtures. Nada disso foi
alterado no scanner: a etapa 1 já foi entregue com a notação do material de
aula, e a decisão está registrada em
[`docs/tokens.md`](../../docs/tokens.md). Se algum dia for preciso rodar estas
fixtures de verdade, o caminho é adicionar um modo de saída `--jsonl` ao
`main.py`/`main.c` com um mapa de nomes, sem tocar nas regras léxicas.

---

## Armadilhas dos scripts oficiais

Os quatro scripts em [`ref/scripts/`](../scripts/) foram lidos linha por linha;
o que importa saber antes de usá-los:

**`testar_parser_python.sh` / `testar_parser_c.sh`**

- Comparam **stdout + stderr concatenados** com um arquivo "esperado"
  descoberto por tentativa. Para este pacote, o candidato que casa é o último
  da lista: `casos/NN/ast.esperada.txt`. Ou seja, para os casos **26–50** o
  script espera literalmente a frase `NÃO HÁ AST: o parser deve rejeitar a
  entrada.` na saída do parser — logo, esses 25 casos **sempre** aparecem como
  `FALHOU`, por construção. O máximo alcançável é `25 OK / 25 FALHOU`, com
  exit code 1 e a mensagem "há casos que precisam ser corrigidos".
- A normalização é mínima (remove `\r`, espaços à direita e linhas em branco
  iniciais). Espaço interno **conta**, e é onde o pacote é inconsistente.
- O resumo traz um contador separado, `Erros sintáticos`, alimentado por um
  `grep -Eiq 'erro[ _-]*(sintático|de sintaxe)|syntax error|parse error'` em
  stdout/stderr. É o número que de fato mostra que os casos inválidos foram
  detectados — vale garantir que nossas mensagens casem com esse padrão.
- `testar_parser_python.sh` exige `[[ -x "$PARSER" ]]`: o `parser.py` precisa
  do **bit de execução** (`chmod +x parser.py`). Em Windows isso costuma vir
  desligado; no Git Bash, `chmod +x` funciona e o git guarda o bit.
- `testar_parser_c.sh` compila com `gcc -Wall -Wextra -std=c11 $PARSER -o …`,
  isto é, **uma única unidade de tradução**. Um parser em C espalhado por
  vários `.c` não compila por esse script sem adaptação (foi exatamente o que
  aconteceu com o scanner, que é `lexer.c` + `main.c`).

**`test_scanner_python.sh` / `test_scanner_c.sh`**

- Comparam a saída do scanner com `*.expected.jsonl` — formato que o nosso
  scanner não produz (ver divergências acima). As versões adaptadas em
  `src/python/` e `src/c/` comparam com `tests/expected/` em vez disso.
- Ambos declaram contadores `PASSED`/`FAILED`/`SKIPPED` que nunca são usados
  (o corpo usa `PASS`/`FAIL`/`WARN`, que por sua vez nunca são inicializados —
  funciona só porque o bash trata vazio como 0 em `$(( ))`), e
  `test_scanner_c.sh` chama uma função `fail` que não existe: se o `gcc`
  falhar, a mensagem que aparece é `fail: command not found`.
