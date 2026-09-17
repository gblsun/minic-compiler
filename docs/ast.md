# AST — árvore sintática abstrata do MINIC

Contrato dos nós da AST e do seu formato de impressão, para a **etapa 2**
(parser). Três fontes se somam aqui, e elas não dizem exatamente a mesma coisa:

| Fonte | O que define |
|---|---|
| Seção 8 de [`ref/especificacao-completa-minic.pdf`](../ref/especificacao-completa-minic.pdf) | os nós mínimos e o que cada nó carrega (posição, tipo inferido, referência a símbolo) |
| [`ref/apostilas/apostila-aula-12-integracao-scanner-parser-ast.pdf`](../ref/apostilas/apostila-aula-12-integracao-scanner-parser-ast.pdf) | o contrato dos nós e as regras de impressão textual canônica |
| [`ref/testes-oficiais/testes-parser-50/`](../ref/testes-oficiais/testes-parser-50/) | a notação **exata** que os 50 casos de avaliação esperam |

Onde houver conflito, quem manda para a entrega é o pacote de testes — é ele
que vai ser comparado. As diferenças estão anotadas no fim deste documento.

## Nós (Seção 8 da especificação)

```text
Program
├── GlobalDeclarations
├── FunctionDeclarations
└── MainFunction
```

```text
FunctionDecl(returnType, name, parameters, body)
VarDecl(type, name, initializer?)
BinaryOp(operator, left, right)
UnaryOp(operator, operand)
IfStmt(condition, thenBranch, elseBranch?)
WhileStmt(condition, body)
CallExpr(function, arguments)
ReturnStmt(expression?)
```

Todo nó deve carregar, sempre que possível, **posição de origem** (linha e
coluna), tipo inferido e referência ao símbolo. Na etapa 2 só a posição é
preenchida; tipo e símbolo entram na etapa 3 — mas o campo já deve existir,
porque a mensagem "variável não declarada na linha 8, coluna 5" da etapa 3
depende da posição gravada aqui.

## Notação usada pelos testes oficiais

Os arquivos `ast.esperada.txt` usam uma S-expression compacta, de uma linha.
Esta é a tabela completa dos nós que aparecem nos 25 casos aceitos:

| Nó | Forma | Exemplo (caso oficial) |
|---|---|---|
| Programa | `Program(decl, decl, …)` | `Program(VarDecl(int x))` |
| Variável | `VarDecl(tipo nome)` / `VarDecl(tipo nome = expr)` | `VarDecl(int x = Lit(int,42))` |
| Vetor | `VarDecl(tipo nome size=expr)` | `VarDecl(int dados size=Lit(int,10))` |
| Função | `Function(tipo nome(params) Block(…))` | `Function(int soma(int a,int b) Block(…))` |
| Bloco | `Block(item, item, …)` | `Block(VarDecl(int y=Lit(int,2)), …)` |
| Comando-expressão | `ExprStmt(expr)` | `ExprStmt(Assign(Id(x),Lit(int,7)))` |
| Atribuição | `Assign(alvo, valor)` | `Assign(Id(a),Assign(Id(b),Lit(int,3)))` |
| `if` | `If(cond, entao, senao)` | `If(Id(x), ExprStmt(…), NULL)` |
| `while` | `While(cond, corpo)` | `While(Binary(<,Id(i),Lit(int,10)), …)` |
| `return` | `Return(expr)` / `Return(NULL)` | `Return(Lit(int,0))`, `Return(NULL)` |
| Binário | `Binary(op, esq, dir)` | `Binary(+, Id(a), Id(b))` |
| Unário | `Unary(op, operando)` | `Unary(-,Lit(int,5))` |
| Chamada | `Call(Id(nome), arg, …)` | `Call(Id(soma),Lit(int,1),Lit(int,2))` |
| Indexação | `Index(base, indice)` | `Index(Id(v),Lit(int,0))` |
| Identificador | `Id(nome)` | `Id(total)` |
| Literal | `Lit(tipo,valor)` | `Lit(int,42)`, `Lit(real,1.5)`, `Lit(bool,true)` |

Detalhes que não são óbvios e que os casos fixam:

- **`NULL` marca ramo ausente**, e é obrigatório: `If` sempre tem três
  argumentos (`If(cond, entao, NULL)` quando não há `else`), e `return;` vira
  `Return(NULL)`. Não é para omitir o argumento.
- **O tipo do literal real é `real`, não `float`**: `Lit(real,1.5)`. Booleanos
  são `Lit(bool,true)` / `Lit(bool,false)`.
- **Assinatura da função fica inline**, com os parâmetros no formato
  `tipo nome` separados por vírgula **sem espaço**: `int soma(int a,int b)`.
  O corpo é um `Block` que vem depois de um espaço, e não como argumento
  separado por vírgula.
- **Chamada sem argumentos é `Call(Id(foo))`** — só o callee, sem vírgula nem
  parênteses vazios extras.
- **Unário não é dobrado em constante**: `-5` fica `Unary(-,Lit(int,5))`, e não
  `Lit(int,-5)`.
- **Parênteses desaparecem**: `(2 + 3) * 4` fica
  `Binary(*,Binary(+,Lit(int,2),Lit(int,3)),Lit(int,4))` — a estrutura já
  carrega o agrupamento.
- **Declarações globais e funções ficam na mesma lista de `Program`**, na ordem
  em que aparecem no código.
- **Vetor não usa `Index` na declaração**: declarar é `size=`, usar é `Index`.
- O corpo de `if`/`while` sem chaves é o comando direto (`ExprStmt(…)`), não um
  `Block` de um item; com chaves, é `Block(…)`.

### Exemplo completo (caso 25, "programa integrado")

Entrada:

```c
int soma(int a, int b) { return a + b; } int main() { int v[3]; v[0] = soma(1, 2); while (v[0] < 5) { v[0] = v[0] + 1; } return v[0]; }
```

AST esperada (quebrada em várias linhas aqui só para leitura — no arquivo é uma
linha só):

```text
Program(
  Function(int soma(int a,int b) Block(Return(Binary(+,Id(a),Id(b))))),
  Function(int main() Block(
    VarDecl(int v size=Lit(int,3)),
    ExprStmt(Assign(Index(Id(v),Lit(int,0)),Call(Id(soma),Lit(int,1),Lit(int,2)))),
    While(Binary(<,Index(Id(v),Lit(int,0)),Lit(int,5)),
          Block(ExprStmt(Assign(Index(Id(v),Lit(int,0)),
                                Binary(+,Index(Id(v),Lit(int,0)),Lit(int,1)))))),
    Return(Index(Id(v),Lit(int,0))))))
```

## Por que não dá para comparar byte a byte

O pacote oficial **não** é consistente na própria notação:

- espaçamento varia entre casos — `VarDecl(int x = Lit(int,42))` (caso 02) vs.
  `VarDecl(int i=Lit(int,0))` (caso 10); `Binary(+, Lit(int,2), …)` (caso 03)
  vs. `Binary(+,Id(i),…)` (caso 10);
- o caso 24 tem um `)` faltando, o que joga o `Return` para fora do corpo da
  função (ver [defeitos conhecidos](../ref/testes-oficiais/README.md#defeitos-conhecidos-do-pacote)).

Um impressor canônico produz **um** estilo só — é justamente o que a apostila
da aula 12 pede ("ordem fixa de campos, nada dependente de execução"). Logo:

- a nossa forma canônica escolhe **um** espaçamento e o aplica sempre;
- a comparação com os arquivos oficiais é feita **normalizando espaços em
  branco fora de lexemas** antes de comparar;
- o caso 24 fica como exceção conhecida e documentada, não como falha do parser.

## Forma canônica adotada pelo projeto

Duas saídas, ambas determinísticas (a apostila da aula 12 sugere exatamente
isso: uma para teste, outra para leitura humana). Implementadas em
[`src/python/minic_ast.py`](../src/python/minic_ast.py) e
[`src/c/ast.c`](../src/c/ast.c), com o mesmo resultado byte a byte.

1. **Compacta** (padrão, usada nos testes): S-expression de uma linha. A regra
   de espaçamento é uma só — **itens de uma lista (`Program` e `Block`) são
   separados por `", "`; todo o resto é compacto** (`","` entre filhos, `=` sem
   espaços em `VarDecl` e em `size=`). Foi a combinação que casa exatamente com
   o maior número de arquivos oficiais (16 dos 25); os outros 9 diferem apenas
   em espaço em branco, que é o que a comparação da suíte normaliza.
2. **Indentada** (`--tree`, para depuração): um nó por linha, dois espaços por
   nível, atributos escalares em ordem fixa (`type=`, `name=`, `op=`,
   `value=`). Nada dependente da execução (endereço de ponteiro, ordem de
   dicionário) aparece na saída.

```bash
python parser.py --tree codigo.c   # ou ./parser --tree codigo.c
```

## Nós que os testes oficiais não exercitam

A gramática tem construções que nenhum dos 50 casos usa, e para elas a notação
é convenção nossa (registrada aqui para ficar estável nas etapas seguintes):

| Nó | Forma | Exemplo |
|---|---|---|
| `for` | `For(init, cond, passo, corpo)`, com `NULL` nas partes ausentes | `For(NULL,NULL,NULL,Block(Break()))` |
| `break` / `continue` | `Break()` / `Continue()` | `Block(Break())` |
| `print` | `Print(expr)` | `Print(Binary(+,Id(x),Lit(int,1)))` |
| `read` | `Read(alvo)` — só `Id` ou `Index` | `Read(Index(Id(v),Id(i)))` |
| literal de caractere | `Lit(char,'a')`, com escapes reescritos | `Lit(char,'\n')` |
| literal de cadeia | `Lit(string,"…")`, com escapes reescritos | `Lit(string,"a\tb")` |
| parâmetro vetor | `tipo nome[]` na assinatura | `Function(float media(float valores[],int n) …)` |
| comando vazio | `ExprStmt(NULL)` | `Block(ExprStmt(NULL))` |

Os literais de caractere e de cadeia são impressos **como estavam no código**:
o lexer entrega o valor já decodificado (o `\n` virou uma quebra de linha de
verdade), e a impressão reescreve os escapes, de modo que
`char quebra = '\n';` volta a sair como `Lit(char,'\n')`.

```text
Program
  VarDecl type=int name=x
  ExprStmt
    Assign
      Id name=x
      Binary op=+
        Id name=a
        Binary op=*
          Id name=b
          Lit type=int value=2
```

## Divergências entre as três fontes

Registradas para não se perder tempo depois discutindo qual está "certa":

| Assunto | Especificação (Seção 8) | Apostila 12 | Testes oficiais | Adotado |
|---|---|---|---|---|
| Nome do nó binário | `BinaryOp` | `Binary` | `Binary` | `Binary` |
| Nome do nó unário | `UnaryOp` | `Unary` | `Unary` | `Unary` |
| Nome do nó de `if` | `IfStmt` | `If` | `If` | `If` |
| Nome do nó de `while` | `WhileStmt` | `While` | `While` | `While` |
| Nome do nó de `return` | `ReturnStmt` | `Return` | `Return` | `Return` |
| Nome do nó de chamada | `CallExpr` | `Call` | `Call` | `Call` |
| Nome do nó de função | `FunctionDecl` | `FunctionDecl` | `Function` | `Function` |
| Identificador | `Variable` / `Identifier` | `Identifier` | `Id` | `Id` |
| Literal | `IntegerLiteral` | `Literal` | `Lit(tipo,valor)` | `Lit` |
| Topo do programa | `Program` com três seções fixas (globais, funções, `main`) | `Program` com lista de declarações | `Program` com lista na ordem do código | lista na ordem do código |
| Ramo ausente | campo opcional | omitido | `NULL` explícito | `NULL` |

Os nomes da especificação são mantidos como **comentário/apelido** no código
(por exemplo, a classe pode se chamar `Binary` e a docstring citar `BinaryOp`),
para que a rastreabilidade com o PDF não se perca.
