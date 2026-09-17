"""Nós da AST do MINIC e as duas formas de imprimi-la.

Cada construção da linguagem tem um nó próprio, com os campos que as etapas
seguintes do compilador vão precisar (Seção 8 da especificação). Todo nó
carrega `line`/`column`: a posição não é enfeite de depuração, é o que permite
a análise semântica dizer "variável não declarada na linha 8, coluna 5" mais
adiante.

O módulo se chama `minic_ast` — e não `ast` — de propósito: `ast` é um módulo
da biblioteca padrão do Python, e como a pasta do script entra no começo do
`sys.path`, um arquivo `ast.py` aqui passaria a "sombrear" o módulo oficial
para qualquer coisa que rodasse junto.

Duas impressões, ambas determinísticas (nada de endereço de objeto ou ordem
de dicionário, que mudariam entre execuções):

- `to_sexpr(no)`: forma compacta de uma linha, `Program(VarDecl(int x))`, que
  é a notação usada pelos 50 casos oficiais de teste;
- `to_tree(no)`: forma indentada, um nó por linha, para leitura humana.

Ver docs/ast.md para a tabela completa da notação e as divergências entre as
fontes (especificação, apostila da aula 12 e pacote de testes).
"""

from dataclasses import dataclass, field

# Estilo da forma compacta, em uma regra só: **itens de uma lista (Program e
# Block) são separados por ", "; todo o resto é compacto**.
#
# Os arquivos oficiais não seguem uma regra única — o mesmo construto aparece
# nos dois estilos em casos diferentes (`VarDecl(int x = Lit(int,42))` no caso
# 02 contra `VarDecl(int i=Lit(int,0))` no caso 10; `If(c, t, e)` no caso 08
# contra `If(c,t,e)` no caso 23) —, então nenhum impressor determinístico bate
# byte a byte com todos os 25. Esta combinação é a que casa exatamente com o
# maior número deles (16/25); os outros 9 só diferem em espaço em branco, que é
# exatamente o que a comparação da suíte normaliza (tests/run_parser_tests.py).
SEP_LISTA = ", "  # entre itens de Program e Block
SEP = ","  # entre os demais filhos (expressões, If, While, For, …)
EQ = "="  # em volta do `=` de inicialização em VarDecl

# Sequências de escape na direção inversa da do lexer: o `lexeme` de um CHAR ou
# STRING já vem decodificado (o lexer traduziu "\n" na quebra de linha de
# verdade), então para imprimir o literal de volta na AST precisamos reescrever
# os caracteres especiais na forma com barra invertida.
ESCAPES_INVERSOS = {
    "\\": "\\\\",
    "\n": "\\n",
    "\t": "\\t",
    "\r": "\\r",
    "'": "\\'",
    '"': '\\"',
}


def escapar(texto: str) -> str:
    """Reescreve um valor de char/string na forma com escapes, para impressão."""
    return "".join(ESCAPES_INVERSOS.get(ch, ch) for ch in texto)


@dataclass
class Node:
    """Base de todos os nós: só a posição de origem no código-fonte."""

    line: int = 0
    column: int = 0


# -- programa e declarações ----------------------------------------------


@dataclass
class Program(Node):
    """Raiz da árvore: a lista de itens de topo, na ordem do código-fonte.

    A especificação descreve `Program` com três seções fixas (globais,
    funções, `main`), mas os casos oficiais aceitam qualquer ordem — e até
    comandos soltos no topo (caso 22, `a = b = 3;`). Por isso aqui é uma lista
    só, preservando a ordem em que as coisas aparecem.
    """

    decls: list = field(default_factory=list)


@dataclass
class VarDecl(Node):
    """`int x;`, `int x = 1;` ou `int v[3];`.

    `init` e `size` são mutuamente exclusivos: a gramática permite um
    inicializador **ou** um tamanho de vetor, nunca os dois (`declarador ::=
    identificador inicializacao? | identificador "[" tamanho "]"`).
    """

    type: str = ""
    name: str = ""
    init: Node | None = None
    size: Node | None = None


@dataclass
class Param(Node):
    """Um parâmetro de função: `int a` ou `int v[]`."""

    type: str = ""
    name: str = ""
    is_array: bool = False


@dataclass
class Function(Node):
    """`int soma(int a, int b) { ... }` — inclui o caso `void`."""

    return_type: str = ""
    name: str = ""
    params: list = field(default_factory=list)
    body: Node | None = None


# -- comandos --------------------------------------------------------------


@dataclass
class Block(Node):
    """`{ ... }` — lista ordenada de declarações locais e comandos."""

    items: list = field(default_factory=list)


@dataclass
class ExprStmt(Node):
    """Um comando-expressão (`x = 1;`, `foo();`) ou o comando vazio (`;`)."""

    expr: Node | None = None


@dataclass
class If(Node):
    """`if (c) então` com `senão` opcional (`None` quando não há `else`)."""

    cond: Node | None = None
    then: Node | None = None
    otherwise: Node | None = None


@dataclass
class While(Node):
    cond: Node | None = None
    body: Node | None = None


@dataclass
class For(Node):
    """`for (init; cond; step) corpo` — as três primeiras partes são opcionais."""

    init: Node | None = None
    cond: Node | None = None
    step: Node | None = None
    body: Node | None = None


@dataclass
class Return(Node):
    expr: Node | None = None


@dataclass
class Break(Node):
    pass


@dataclass
class Continue(Node):
    pass


@dataclass
class Print(Node):
    arg: Node | None = None


@dataclass
class Read(Node):
    target: Node | None = None


# -- expressões ------------------------------------------------------------


@dataclass
class Assign(Node):
    """`alvo = valor`, associativo à direita (`a = b = 3`)."""

    target: Node | None = None
    value: Node | None = None


@dataclass
class Binary(Node):
    op: str = ""
    left: Node | None = None
    right: Node | None = None


@dataclass
class Unary(Node):
    op: str = ""
    operand: Node | None = None


@dataclass
class Call(Node):
    callee: Node | None = None
    args: list = field(default_factory=list)


@dataclass
class Index(Node):
    base: Node | None = None
    index: Node | None = None


@dataclass
class Id(Node):
    name: str = ""


@dataclass
class Lit(Node):
    """Um literal. `kind` é `int`, `real`, `bool`, `char` ou `string`.

    `text` guarda o lexema **como escrito no código** (e não o valor
    convertido): assim `1.0` continua sendo impresso como `1.0`, e não como o
    `1` que uma conversão para float produziria.
    """

    kind: str = ""
    text: str = ""


# -- impressão compacta (a notação dos testes oficiais) --------------------


def to_sexpr(node) -> str:
    """Devolve a AST como uma S-expression de uma linha.

    `None` vira `NULL`, e não desaparece: `If` sempre tem três argumentos e
    `return;` é `Return(NULL)`, como nos arquivos `ast.esperada.txt`.
    """
    if node is None:
        return "NULL"

    if isinstance(node, Program):
        return f"Program({_filhos(node.decls)})"

    if isinstance(node, VarDecl):
        cabeca = f"{node.type} {node.name}"
        if node.size is not None:
            return f"VarDecl({cabeca} size={to_sexpr(node.size)})"
        if node.init is not None:
            return f"VarDecl({cabeca}{EQ}{to_sexpr(node.init)})"
        return f"VarDecl({cabeca})"

    if isinstance(node, Function):
        params = ",".join(_param_sexpr(p) for p in node.params)
        assinatura = f"{node.return_type} {node.name}({params})"
        return f"Function({assinatura} {to_sexpr(node.body)})"

    if isinstance(node, Block):
        return f"Block({_filhos(node.items)})"

    if isinstance(node, ExprStmt):
        return f"ExprStmt({to_sexpr(node.expr)})"

    if isinstance(node, If):
        return (
            "If("
            + _junta(
                to_sexpr(node.cond), to_sexpr(node.then), to_sexpr(node.otherwise)
            )
            + ")"
        )

    if isinstance(node, While):
        return "While(" + _junta(to_sexpr(node.cond), to_sexpr(node.body)) + ")"

    if isinstance(node, For):
        return (
            "For("
            + _junta(
                to_sexpr(node.init),
                to_sexpr(node.cond),
                to_sexpr(node.step),
                to_sexpr(node.body),
            )
            + ")"
        )

    if isinstance(node, Return):
        return f"Return({to_sexpr(node.expr)})"

    if isinstance(node, Break):
        return "Break()"

    if isinstance(node, Continue):
        return "Continue()"

    if isinstance(node, Print):
        return f"Print({to_sexpr(node.arg)})"

    if isinstance(node, Read):
        return f"Read({to_sexpr(node.target)})"

    if isinstance(node, Assign):
        return "Assign(" + _junta(to_sexpr(node.target), to_sexpr(node.value)) + ")"

    if isinstance(node, Binary):
        return (
            "Binary("
            + _junta(node.op, to_sexpr(node.left), to_sexpr(node.right))
            + ")"
        )

    if isinstance(node, Unary):
        return "Unary(" + _junta(node.op, to_sexpr(node.operand)) + ")"

    if isinstance(node, Call):
        partes = [to_sexpr(node.callee)] + [to_sexpr(a) for a in node.args]
        return "Call(" + _junta(*partes) + ")"

    if isinstance(node, Index):
        return "Index(" + _junta(to_sexpr(node.base), to_sexpr(node.index)) + ")"

    if isinstance(node, Id):
        return f"Id({node.name})"

    if isinstance(node, Lit):
        # `Lit` é o único nó em que o separador é sempre "," sem espaço, em
        # todos os arquivos oficiais — por isso não usa SEP.
        return f"Lit({node.kind},{node.text})"

    raise TypeError(f"nó de AST desconhecido: {type(node).__name__}")


def _param_sexpr(param: Param) -> str:
    return f"{param.type} {param.name}[]" if param.is_array else f"{param.type} {param.name}"


def _junta(*partes) -> str:
    """Junta os filhos de um nó (separador compacto)."""
    return SEP.join(partes)


def _filhos(nos) -> str:
    """Junta os itens de uma lista (`Program`/`Block`), com espaço."""
    return SEP_LISTA.join(to_sexpr(n) for n in nos)


# -- impressão indentada (para leitura humana) -----------------------------


def to_tree(node, nivel: int = 0) -> str:
    """Devolve a AST indentada, um nó por linha e dois espaços por nível."""
    linhas: list[str] = []
    _tree(node, nivel, linhas)
    return "\n".join(linhas)


def _tree(node, nivel: int, saida: list):
    recuo = "  " * nivel

    if node is None:
        saida.append(f"{recuo}NULL")
        return

    if isinstance(node, Program):
        saida.append(f"{recuo}Program")
        for d in node.decls:
            _tree(d, nivel + 1, saida)
    elif isinstance(node, VarDecl):
        saida.append(f"{recuo}VarDecl type={node.type} name={node.name}")
        if node.size is not None:
            saida.append(f"{recuo}  size")
            _tree(node.size, nivel + 2, saida)
        if node.init is not None:
            _tree(node.init, nivel + 1, saida)
    elif isinstance(node, Function):
        saida.append(f"{recuo}Function returnType={node.return_type} name={node.name}")
        for p in node.params:
            sufixo = "[]" if p.is_array else ""
            saida.append(f"{recuo}  Param type={p.type} name={p.name}{sufixo}")
        _tree(node.body, nivel + 1, saida)
    elif isinstance(node, Block):
        saida.append(f"{recuo}Block")
        for item in node.items:
            _tree(item, nivel + 1, saida)
    elif isinstance(node, ExprStmt):
        saida.append(f"{recuo}ExprStmt")
        _tree(node.expr, nivel + 1, saida)
    elif isinstance(node, If):
        saida.append(f"{recuo}If")
        _tree(node.cond, nivel + 1, saida)
        _tree(node.then, nivel + 1, saida)
        _tree(node.otherwise, nivel + 1, saida)
    elif isinstance(node, While):
        saida.append(f"{recuo}While")
        _tree(node.cond, nivel + 1, saida)
        _tree(node.body, nivel + 1, saida)
    elif isinstance(node, For):
        saida.append(f"{recuo}For")
        for parte in (node.init, node.cond, node.step, node.body):
            _tree(parte, nivel + 1, saida)
    elif isinstance(node, Return):
        saida.append(f"{recuo}Return")
        _tree(node.expr, nivel + 1, saida)
    elif isinstance(node, Break):
        saida.append(f"{recuo}Break")
    elif isinstance(node, Continue):
        saida.append(f"{recuo}Continue")
    elif isinstance(node, Print):
        saida.append(f"{recuo}Print")
        _tree(node.arg, nivel + 1, saida)
    elif isinstance(node, Read):
        saida.append(f"{recuo}Read")
        _tree(node.target, nivel + 1, saida)
    elif isinstance(node, Assign):
        saida.append(f"{recuo}Assign")
        _tree(node.target, nivel + 1, saida)
        _tree(node.value, nivel + 1, saida)
    elif isinstance(node, Binary):
        saida.append(f"{recuo}Binary op={node.op}")
        _tree(node.left, nivel + 1, saida)
        _tree(node.right, nivel + 1, saida)
    elif isinstance(node, Unary):
        saida.append(f"{recuo}Unary op={node.op}")
        _tree(node.operand, nivel + 1, saida)
    elif isinstance(node, Call):
        saida.append(f"{recuo}Call")
        _tree(node.callee, nivel + 1, saida)
        for a in node.args:
            _tree(a, nivel + 1, saida)
    elif isinstance(node, Index):
        saida.append(f"{recuo}Index")
        _tree(node.base, nivel + 1, saida)
        _tree(node.index, nivel + 1, saida)
    elif isinstance(node, Id):
        saida.append(f"{recuo}Id name={node.name}")
    elif isinstance(node, Lit):
        saida.append(f"{recuo}Lit type={node.kind} value={node.text}")
    else:
        raise TypeError(f"nó de AST desconhecido: {type(node).__name__}")
