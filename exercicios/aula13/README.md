# Aula 13 — exercícios práticos de tradução dirigida por sintaxe

Resolução dos 10 exercícios de
[`ref/exercicios/exercicios-aula-13-enunciados.pdf`](../../ref/exercicios/exercicios-aula-13-enunciados.pdf),
cada um em **Python e em C**, com regras semânticas equivalentes e a mesma
saída byte a byte nas duas linguagens.

| # | Pasta | Tema | Casos |
|---|---|---|---|
| 01 | [ex01_calculadora](ex01_calculadora/) | atributo sintetizado `val`, avaliação com rastreamento | 16 |
| 02 | [ex02_grafo_dependencias](ex02_grafo_dependencias/) | grafo de dependências, ordenação topológica (Kahn), ciclos | 11 |
| 03 | [ex03_ast_s_atribuida](ex03_ast_s_atribuida/) | AST com SDD S-atribuída, impressão prefixa e avaliação | 12 |
| 04 | [ex04_tipo_herdado](ex04_tipo_herdado/) | atributo herdado `L.inh`, tabela de símbolos | 11 |
| 05 | [ex05_escopos](ex05_escopos/) | escopo como contexto herdado, blocos, sombreamento | 9 |
| 06 | [ex06_expressoes_minic](ex06_expressoes_minic/) | AST de expressões MINIC com precedência | 13 |
| 07 | [ex07_comandos](ex07_comandos/) | AST de comandos e fluxo de controle | 13 |
| 08 | [ex08_tipos](ex08_tipos/) | tipos como atributos sintetizados | 10 |
| 09 | [ex09_erros](ex09_erros/) | diagnósticos, recuperação, ações com pré-condições | 12 |
| 10 | [ex10_pipeline](ex10_pipeline/) | pipeline completo scanner → parser → atributos → AST | 13 |

**Resultado:** 120 casos × 2 linguagens + 10 compilações = **250/250
verificações**. O C compila sem nenhum aviso com `gcc -Wall -Wextra -std=c11`
(gcc 14, Linux), e os 120 casos também rodaram com AddressSanitizer e UBSan
sem nenhum problema de memória, vazamento ou comportamento indefinido.

## Como rodar

Tudo a partir da raiz do repositório.

```bash
python exercicios/aula13/run_tests.py            # os 10 exercícios, Python e C
python exercicios/aula13/run_tests.py ex04 ex05  # só alguns
python exercicios/aula13/run_tests.py --python   # só Python (sem gcc)
python exercicios/aula13/run_tests.py -v         # mostra o diff das falhas
```

Cada caso de teste fica em `exNN_*/casos/` como `nome.entrada` (o arquivo
analisado), `nome.args` (opcional: `--trace`, `--pos`) e `nome.esperado`
(stdout, stderr e código de saída). **As duas linguagens são comparadas com o
mesmo `.esperado`**: um caso só passa nas duas se elas concordarem byte a byte,
que é o teste diferencial pedido no exercício 10, aplicado a todos.

Rodar um exercício direto (a partir da pasta dele):

```bash
cd exercicios/aula13/ex01_calculadora
python calculadora.py --trace casos/06_trace_associatividade.entrada

gcc -Wall -Wextra -std=c11 calculadora.c ../comum/fluxo.c ../../../src/c/lexer.c -o calculadora
./calculadora --trace casos/06_trace_associatividade.entrada
```

Fontes C de cada exercício (sempre com `-Wall -Wextra -std=c11`):

| Exercício | Arquivos |
|---|---|
| 01, 03, 04, 05 | `<exercício>.c ../comum/fluxo.c ../../../src/c/lexer.c` |
| 02 | `grafo.c` (não lê MINIC, então não usa o scanner) |
| 06 | `expr.c expressoes.c ../comum/fluxo.c ../../../src/c/lexer.c` |
| 07, 08, 09 | `<exercício>.c ../ex06_expressoes_minic/expr.c ../comum/fluxo.c ../../../src/c/lexer.c` |
| 10 | `scanner.c ast.c parser.c main.c ../../../src/c/lexer.c` |

Todos os programas leem de um arquivo ou, com `-`, da entrada padrão (o ex10
lê da entrada padrão também sem argumento).

## Convenções comuns

- **Scanner reaproveitado, não copiado.** Os exercícios que leem MINIC usam o
  lexer da etapa 1 (`src/python/lexer.py`, `src/c/lexer.c`) por meio de
  [`comum/fluxo.py`](comum/fluxo.py) / [`comum/fluxo.h`](comum/fluxo.h), que
  só acrescentam um fluxo de tokens com `next_token`/`lookahead`. O ex10 tem o
  próprio módulo `scanner`, também como adaptador do mesmo lexer.
- **Módulo de expressões compartilhado.** O parser de expressões do ex06
  ([`expr.py`](ex06_expressoes_minic/expr.py) / [`expr.c`](ex06_expressoes_minic/expr.c))
  é reaproveitado pelos exercícios 07, 08 e 09.
- **Diagnósticos no formato do projeto**, com linha e coluna:
  `Erro léxico/de sintaxe/semântico na linha L, coluna C: ...`.
- **Códigos de saída iguais em todos**: 0 ok, 1 uso/arquivo, 2 léxico,
  3 sintático, 4 semântico. Erro de sintaxe e erro semântico nunca se
  confundem.
- **AST por nós, nunca por concatenação de texto**: dataclasses imutáveis em
  Python, structs discriminadas por `enum` em C. O texto só aparece na
  serialização final.

## Regras e decisões por exercício

### 01 — calculadora com atributos sintetizados

`val` é sintetizado em E, T e F (`E.val = E1.val + T.val`, `F.val =
num.lexval`...). A recursão à esquerda vira laço no parser descendente, e o
laço reduz da esquerda para a direita: `18 / 3 / 2 = 3`. **Decisão:** divisão
por zero é erro **semântico** (exit 4), detectado na ação de `T -> T / F`.
A análise continua com `val` indefinido, para que um erro de sintaxe
posterior ainda prevaleça (exit 3). Inteiros de 64 bits, com estouro
detectado em vez de resultado errado.

```text
$ python calculadora.py --trace casos/06_trace_associatividade.entrada
F -> num      F.val = num.lexval = 18
T -> F        T.val = F.val = 18
F -> num      F.val = num.lexval = 3
T -> T / F    T.val = T1.val / F.val = 18 / 3 = 6
...
resultado = 3
```

### 02 — grafo de dependências

Cada instância de atributo (`num1.lexval`, `F1.val`, `T.val`) é um nó; cada
argumento de ação gera a aresta `argumento -> atributo`. A ordem sai do
algoritmo de Kahn. **Decisão:** a ordenação é feita **antes** de executar
qualquer ação. Com um ciclo, os atributos bloqueados (inclusive os que só
dependem do ciclo) são listados e nenhum valor é produzido, porque um valor
parcial não é válido. O grafo de `3 * 5` é declarado fora de ordem de
propósito, para mostrar que quem decide a ordem é o grafo.

### 03 — AST com SDD S-atribuída

`node` é sintetizado; toda ação usa só atributos dos filhos
(`E.node = Binary(op, E1.node, T.node)`). `a - 4 + c` produz
`(+ (- a 4) c)`. **Decisão (C):** cada construtor é dono dos filhos que
recebe e, se o `malloc` falhar, libera esses filhos antes de devolver NULL.
Assim nenhum caminho de erro precisa de limpeza manual.

### 04 — tipo herdado

`D -> T L ; { L.inh = T.type }`, `L -> L1 , id { L1.inh = L.inh; addtype }`.
**Justificativa L-atribuída:** `L.inh` depende só de `T.type`, que está **à
esquerda** de L, e de `L.inh` do pai. Nada depende de algo à direita, então uma
passada da esquerda para a direita calcula tudo. Em código, o atributo
herdado é o **parâmetro** de `parse_L(tipo)`. Redeclaração é erro semântico; a
primeira declaração continua valendo.

### 05 — escopos como contexto herdado

O escopo atual desce como parâmetro (`parse_item(escopo)`), sem variável
global. A busca vai do escopo atual aos ancestrais. Sombrear um escopo externo
é permitido e fica registrado na AST (`sombreia x de global (1:5)`), mas
redeclarar no mesmo escopo é erro. **Decisão (C):** o `Scope` é dono dos
símbolos e morre no `leave_scope`, então os nós da AST **copiam** o que
precisam da declaração resolvida em vez de apontar para ela.

### 06 — expressões MINIC

Seis níveis binários associativos à esquerda (`||`, `&&`, igualdade,
relacionais, aditivos, multiplicativos), e os unários `-` e `!` abaixo de
todos. Por isso `!a == b` é `(== (! a) b)`. As folhas guardam lexema, linha e
coluna (`--pos`).

### 07 — comandos

Nós Block, Assign, If, While, Return e ExprStmt, com invariantes documentadas:
`If.senao = None` marca explicitamente a ausência do else, `Return.valor =
None` representa `return;` e o alvo de um Assign é sempre um identificador.
O `else` pendente fica com o `if` mais próximo.

### 08 — tipos sintetizados

`check_expr(node, env)` calcula `type` a partir dos filhos. Os tipos ficam numa
tabela à parte, então a AST não é alterada. **Política:** promoção implícita
int → float em aritmética, relacionais e igualdade, nunca o contrário; `%`
só com int; `&&`, `||` e `!` só com bool. Um nó com filho `erro` também vira
`erro`, **sem** novo diagnóstico, para que um erro não se multiplique pela
árvore acima.

### 09 — erros e recuperação

`Diagnostic` com categoria, mensagem, lexema, linha e coluna. O erro de
sintaxe interrompe só o comando atual, e o parser sincroniza em `;` ou `}`.
Os construtores conferem as próprias pré-condições. **Decisão:** a
verificação semântica só roda sobre comandos completos, para que um comando
quebrado não gere ruído semântico. Com qualquer diagnóstico, **nenhuma AST é
publicada**. Um programa vazio e correto publica `Programa (vazio)`: uma árvore
vazia válida não é o mesmo que uma árvore ausente.

### 10 — pipeline integrador

Módulos separados, como pedido: `scanner`, `ast_nodes`/`ast` e `parser`, em
Python e em C. Atributos **herdados**: o tipo em cada item de uma declaração e
o escopo em cada bloco. Atributos **sintetizados**: `node` e o tipo de cada
expressão. Checagens: identificador não declarado, redeclaração,
compatibilidade em inicialização e atribuição, e condição de if/while do tipo
bool. **Decisão (C):** os nós ficam numa arena, então um erro de sintaxe no
meio da análise (via `longjmp`) libera tudo o que já tinha sido criado. A
gramática e as limitações estão na docstring de
[`ex10_pipeline/parser.py`](ex10_pipeline/parser.py).

## Diferenças de representação entre Python e C

| Aspecto | Python | C |
|---|---|---|
| Nós da AST | `@dataclass(frozen=True)` | struct + `enum` (e `union` no ex03) |
| Posse da memória | coletor de lixo | documentada em cada exercício: pai dono dos filhos (03, 06, 07, 09), escopo dono dos símbolos (05), arena (10) |
| Erro de sintaxe | exceção `ErroSintaxe` | código de retorno / NULL (01–09), `longjmp` (10) |
| Valor indefinido (ex01) | `None` | `Valor { definido, v }` |
| Tabelas | `dict` | vetor dinâmico com busca linear |
| Inteiros | precisão arbitrária, limitada a 64 bits de propósito | `long long` com checagem de estouro sem UB |

## Limitações conhecidas

- Nenhum exercício limita a profundidade de aninhamento das expressões (o
  parser principal do projeto limita, em `src/`). Entradas com centenas de
  parênteses aninhados podem estourar a recursão.
- No ex09, o lexema de um erro léxico é o caractere na posição reportada.
  Com caracteres não-ASCII **antes** dele na mesma linha, a coluna do C (em
  bytes) e a do Python (em caracteres) divergem, a mesma limitação registrada
  no item 1.5 de [`PENDENCIAS.md`](../../PENDENCIAS.md).
