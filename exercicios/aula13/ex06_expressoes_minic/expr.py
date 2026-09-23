"""Parser de expressões MINIC que constrói a AST durante o reconhecimento.

Módulo do exercício 06, reaproveitado pelos exercícios 07, 08 e 09. Consome
os tokens do scanner do projeto (via comum/fluxo.py) — nenhuma regra léxica é
repetida aqui.

Níveis de precedência, do mais fraco ao mais forte (Seção 4 da especificação
MINIC), todos binários associativos à esquerda:

    ou        -> e { || e }
    e         -> igualdade { && igualdade }
    igualdade -> relacional { (== | !=) relacional }
    relacional-> aditiva { (< | <= | > | >=) aditiva }
    aditiva   -> mult { (+ | -) mult }
    mult      -> unaria { (* | / | %) unaria }
    unaria    -> (- | !) unaria | primaria
    primaria  -> id | int | float | true | false | ( ou )

Ação de cada produção (atributo sintetizado `node`):

    X -> X1 op Y   X.node = Binary(op, X1.node, Y.node)
    X -> Y         X.node = Y.node
    unaria -> op u unaria.node = Unary(op, u.node)
    primaria -> id / int / float / true / false:  a folha correspondente
    primaria -> ( ou )   primaria.node = ou.node   (sem nó para parênteses)

Os unários ficam abaixo de todos os binários, então `!a == b` é
`(== (! a) b)`: o `!` se aplica só ao `a`. Literais `float`, `true` e `false`
não são exigidos no exercício 06, mas o exercício 08 (tipos) precisa deles.

As folhas guardam lexema, linha e coluna; os nós internos guardam a posição
do operador.
"""

import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "comum"))
import fluxo as F  # noqa: E402


@dataclass(frozen=True)
class Identifier:
    nome: str
    linha: int
    coluna: int


@dataclass(frozen=True)
class Integer:
    lexema: str
    linha: int
    coluna: int


@dataclass(frozen=True)
class Float:
    lexema: str
    linha: int
    coluna: int


@dataclass(frozen=True)
class Bool:
    lexema: str  # "true" ou "false"
    linha: int
    coluna: int


@dataclass(frozen=True)
class Unary:
    op: str
    operando: "Expr"
    linha: int
    coluna: int


@dataclass(frozen=True)
class Binary:
    op: str
    esq: "Expr"
    dir: "Expr"
    linha: int
    coluna: int


Expr = Identifier | Integer | Float | Bool | Unary | Binary

# Níveis binários, do mais fraco ao mais forte: tipos de token de cada um.
NIVEIS = [
    ("OR",),
    ("AND",),
    ("EQ", "NEQ"),
    ("LT", "LE", "GT", "GE"),
    ("PLUS", "MINUS"),
    ("STAR", "SLASH", "PERCENT"),
]
INICIO_EXPRESSAO = "expressão"


class ExprParser:
    def __init__(self, fl: F.Fluxo):
        self.fl = fl

    def parse_expr(self) -> Expr:
        return self._binario(0)

    def _binario(self, nivel: int) -> Expr:
        if nivel == len(NIVEIS):
            return self._unaria()
        node = self._binario(nivel + 1)
        while self.fl.verifica(*NIVEIS[nivel]):
            op = self.fl.proximo()
            direita = self._binario(nivel + 1)
            node = Binary(op.lexeme, node, direita, op.line, op.column)
        return node

    def _unaria(self) -> Expr:
        op = self.fl.aceita("MINUS", "NOT")
        if op is not None:
            return Unary(op.lexeme, self._unaria(), op.line, op.column)
        return self._primaria()

    def _primaria(self) -> Expr:
        tok = self.fl.atual()
        folhas = {"IDENT": Identifier, "INT": Integer, "FLOAT": Float, "KW_TRUE": Bool, "KW_FALSE": Bool}
        if tok.type in folhas:
            self.fl.proximo()
            return folhas[tok.type](tok.lexeme, tok.line, tok.column)
        if self.fl.aceita("LPAREN"):
            node = self.parse_expr()
            self.fl.exige("RPAREN", "')'")
            return node
        raise F.ErroSintaxe(tok, INICIO_EXPRESSAO)


def sexpr(no: Expr, pos: bool = False) -> str:
    """Serialização determinística: prefixa, totalmente parentizada."""
    match no:
        case Identifier(nome=texto, linha=l, coluna=c) | Integer(lexema=texto, linha=l, coluna=c) | Float(
            lexema=texto, linha=l, coluna=c
        ) | Bool(lexema=texto, linha=l, coluna=c):
            return f"{texto}@{l}:{c}" if pos else texto
        case Unary(op=op, operando=o):
            return f"({op} {sexpr(o, pos)})"
        case Binary(op=op, esq=e, dir=d):
            return f"({op} {sexpr(e, pos)} {sexpr(d, pos)})"
