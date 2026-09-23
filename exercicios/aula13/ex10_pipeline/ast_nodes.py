"""Nós da AST do mini pipeline (exercício 10) e a serialização.

Os nós são imutáveis e já nascem com os atributos semânticos calculados pelas
ações do parser: toda expressão carrega o atributo sintetizado `tipo`, e toda
declaração carrega o tipo e o escopo que receberam como atributos herdados.
O tipo `erro` marca uma subexpressão que já gerou diagnóstico.
"""

from dataclasses import dataclass

# -- expressões (atributo sintetizado: tipo) ------------------------------


@dataclass(frozen=True)
class Literal:
    lexema: str
    tipo: str
    linha: int
    coluna: int


@dataclass(frozen=True)
class Id:
    nome: str
    tipo: str
    linha: int
    coluna: int


@dataclass(frozen=True)
class Unary:
    op: str
    operando: "Expr"
    tipo: str
    linha: int
    coluna: int


@dataclass(frozen=True)
class Binary:
    op: str
    esq: "Expr"
    dir: "Expr"
    tipo: str
    linha: int
    coluna: int


Expr = Literal | Id | Unary | Binary

# -- declarações e comandos -----------------------------------------------


@dataclass(frozen=True)
class VarDecl:
    nome: str
    tipo: str  # herdado da palavra de tipo da declaração
    escopo: str  # herdado do bloco que contém a declaração
    init: Expr | None
    linha: int
    coluna: int


@dataclass(frozen=True)
class Block:
    escopo: str
    itens: tuple
    linha: int
    coluna: int


@dataclass(frozen=True)
class Assign:
    alvo: Id
    valor: Expr
    linha: int
    coluna: int


@dataclass(frozen=True)
class If:
    cond: Expr
    entao: object
    senao: object | None
    linha: int
    coluna: int


@dataclass(frozen=True)
class While:
    cond: Expr
    corpo: object
    linha: int
    coluna: int


@dataclass(frozen=True)
class Return:
    valor: Expr | None
    linha: int
    coluna: int


@dataclass(frozen=True)
class Programa:
    itens: tuple


# -- serialização ----------------------------------------------------------


def expr_str(e: Expr) -> str:
    match e:
        case Literal(lexema=texto, tipo=t) | Id(nome=texto, tipo=t):
            return f"{texto}:{t}"
        case Unary(op=op, operando=o, tipo=t):
            return f"({op}:{t} {expr_str(o)})"
        case Binary(op=op, esq=a, dir=b, tipo=t):
            return f"({op}:{t} {expr_str(a)} {expr_str(b)})"


def serializa(no, nivel: int = 0, rotulo: str = "") -> list[str]:
    ind = "  " * nivel
    p = f"{ind}{rotulo}: " if rotulo else ind
    f = "  " * (nivel + 1)
    match no:
        case Programa(itens=itens):
            linhas = ["Programa" + ("" if itens else " (vazio)")]
            for item in itens:
                linhas += serializa(item, nivel + 1)
            return linhas
        case VarDecl(nome=n, tipo=t, escopo=esc, init=init, linha=l, coluna=c):
            linhas = [f"{p}VarDecl {n} : {t} [{esc}] @{l}:{c}"]
            if init is not None:
                linhas.append(f"{f}init: {expr_str(init)}")
            return linhas
        case Block(escopo=esc, itens=itens, linha=l, coluna=c):
            linhas = [f"{p}Block [{esc}] @{l}:{c}" + ("" if itens else " (vazio)")]
            for item in itens:
                linhas += serializa(item, nivel + 1)
            return linhas
        case Assign(alvo=a, valor=v, linha=l, coluna=c):
            return [f"{p}Assign @{l}:{c}", f"{f}alvo: {expr_str(a)}", f"{f}valor: {expr_str(v)}"]
        case If(cond=cond, entao=t, senao=s, linha=l, coluna=c):
            linhas = [f"{p}If @{l}:{c}", f"{f}cond: {expr_str(cond)}"]
            linhas += serializa(t, nivel + 1, "entao")
            linhas += [f"{f}senao: <ausente>"] if s is None else serializa(s, nivel + 1, "senao")
            return linhas
        case While(cond=cond, corpo=corpo, linha=l, coluna=c):
            return [f"{p}While @{l}:{c}", f"{f}cond: {expr_str(cond)}"] + serializa(corpo, nivel + 1, "corpo")
        case Return(valor=v, linha=l, coluna=c):
            return [f"{p}Return @{l}:{c}", f"{f}valor: " + ("<ausente>" if v is None else expr_str(v))]
