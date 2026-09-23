"""Exercício 03 — construtor de AST com SDD S-atribuída.

Gramática:

    E -> E + T | E - T | T
    T -> T * F | T / F | F
    F -> - F | ( E ) | id | num

Um único atributo, `node`, sintetizado em todos os não terminais. Cada ação
usa **só** atributos dos filhos (por isso a SDD é S-atribuída):

    E -> E1 op T   E.node = Binary(op, E1.node, T.node)
    E -> T         E.node = T.node
    T -> T1 op F   T.node = Binary(op, T1.node, F.node)
    T -> F         T.node = F.node
    F -> - F1      F.node = Unary('-', F1.node)
    F -> ( E )     F.node = E.node          (parênteses não geram nó)
    F -> id        F.node = Identifier(id.lexema)
    F -> num       F.node = Number(num.lexema)

Os laços de E e T reduzem da esquerda para a direita, então em `a - 4 + c` o
Binary('-') vira o filho esquerdo do Binary('+'): (+ (- a 4) c).

Duas travessias sobre a árvore pronta: impressão prefixa totalmente
parentizada e avaliação (inteiros de 64 bits, como no C). Uma expressão com
identificador não é avaliável — isso não é erro, só não há valor para mostrar.

Uso:  python ast_s.py [--trace] <arquivo | ->
"""

import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "comum"))
import fluxo as F  # noqa: E402

MIN, MAX = -(2**63), 2**63 - 1


# -- nós da AST (imutáveis) ------------------------------------------------


@dataclass(frozen=True)
class Number:
    lexema: str
    linha: int
    coluna: int


@dataclass(frozen=True)
class Identifier:
    nome: str
    linha: int
    coluna: int


@dataclass(frozen=True)
class Unary:
    op: str
    operando: "No"
    linha: int
    coluna: int


@dataclass(frozen=True)
class Binary:
    op: str
    esq: "No"
    dir: "No"
    linha: int
    coluna: int


No = Number | Identifier | Unary | Binary


# -- parser com as ações semânticas ----------------------------------------


class Construtor:
    def __init__(self, fl: F.Fluxo, trace: bool):
        self.fl = fl
        self.trace = trace

    def _regra(self, producao: str, acao: str):
        if self.trace:
            print(f"{producao:<14}{acao}")

    def parse_E(self) -> No:
        node = self.parse_T()
        self._regra("E -> T", "E.node = T.node")
        while self.fl.verifica("PLUS", "MINUS"):
            op = self.fl.proximo()
            direita = self.parse_T()
            node = Binary(op.lexeme, node, direita, op.line, op.column)
            self._regra(f"E -> E {op.lexeme} T", f"E.node = Binary('{op.lexeme}', E1.node, T.node)")
        return node

    def parse_T(self) -> No:
        node = self.parse_F()
        self._regra("T -> F", "T.node = F.node")
        while self.fl.verifica("STAR", "SLASH"):
            op = self.fl.proximo()
            direita = self.parse_F()
            node = Binary(op.lexeme, node, direita, op.line, op.column)
            self._regra(f"T -> T {op.lexeme} F", f"T.node = Binary('{op.lexeme}', T1.node, F.node)")
        return node

    def parse_F(self) -> No:
        tok = self.fl.atual()
        if self.fl.aceita("MINUS"):
            operando = self.parse_F()
            self._regra("F -> - F", "F.node = Unary('-', F1.node)")
            return Unary("-", operando, tok.line, tok.column)
        if self.fl.aceita("LPAREN"):
            node = self.parse_E()
            self.fl.exige("RPAREN", "')'")
            self._regra("F -> ( E )", "F.node = E.node")
            return node
        if self.fl.aceita("IDENT"):
            self._regra("F -> id", f"F.node = Identifier({tok.lexeme})")
            return Identifier(tok.lexeme, tok.line, tok.column)
        if self.fl.aceita("INT"):
            self._regra("F -> num", f"F.node = Number({tok.lexeme})")
            return Number(tok.lexeme, tok.line, tok.column)
        raise F.ErroSintaxe(tok, "número, identificador, '-' ou '('")


# -- travessias ------------------------------------------------------------


def prefixa(no: No) -> str:
    match no:
        case Number(lexema=lex):
            return lex
        case Identifier(nome=nome):
            return nome
        case Unary(op=op, operando=o):
            return f"({op} {prefixa(o)})"
        case Binary(op=op, esq=e, dir=d):
            return f"({op} {prefixa(e)} {prefixa(d)})"


class NaoAvaliavel(Exception):
    """A expressão tem identificador: não há valor numérico (não é erro)."""


class ErroAvaliacao(Exception):
    """Erro semântico durante a avaliação (divisão por zero, estouro)."""


def avalia(no: No) -> int:
    match no:
        case Number(lexema=lex, linha=l, coluna=c):
            v = int(lex)
            if v > MAX:
                raise ErroAvaliacao(F.erro_semantico(l, c, f"número {lex} fora do intervalo de 64 bits"))
            return v
        case Identifier(nome=nome, linha=l, coluna=c):
            raise NaoAvaliavel(f"identificador '{nome}' na linha {l}, coluna {c}")
        case Unary(operando=o, linha=l, coluna=c):
            v = -avalia(o)
            if v > MAX:
                raise ErroAvaliacao(F.erro_semantico(l, c, "estouro de inteiro de 64 bits"))
            return v
        case Binary(op=op, esq=e, dir=d, linha=l, coluna=c):
            a, b = avalia(e), avalia(d)
            if op == "/":
                if b == 0:
                    raise ErroAvaliacao(F.erro_semantico(l, c, f"divisão por zero ({a} / {b})"))
                q = abs(a) // abs(b)
                r = -q if (a < 0) != (b < 0) else q
            else:
                r = {"+": a + b, "-": a - b, "*": a * b}[op]
            if not MIN <= r <= MAX:
                raise ErroAvaliacao(F.erro_semantico(l, c, f"estouro de inteiro de 64 bits ({a} {op} {b})"))
            return r


def main(argv: list[str]) -> int:
    F.configurar_saida()
    trace = "--trace" in argv[1:]
    args = [a for a in argv[1:] if a != "--trace"]
    if len(args) != 1:
        print("uso: python ast_s.py [--trace] <arquivo | ->", file=sys.stderr)
        return F.ERRO_USO

    fl, codigo = F.abrir(args[0])
    if fl is None:
        return codigo
    try:
        arvore = Construtor(fl, trace).parse_E()
        if not fl.verifica("EOF"):
            raise F.ErroSintaxe(fl.atual(), "operador ou fim da entrada")
    except F.ErroSintaxe as erro:
        print(erro, file=sys.stderr)
        return F.ERRO_SINTAXE

    print(f"prefixa: {prefixa(arvore)}")
    try:
        print(f"valor: {avalia(arvore)}")
    except NaoAvaliavel as motivo:
        print(f"valor: não avaliável ({motivo})")
    except ErroAvaliacao as erro:
        print(erro, file=sys.stderr)
        return F.ERRO_SEMANTICO
    return F.OK


if __name__ == "__main__":
    sys.exit(main(sys.argv))
