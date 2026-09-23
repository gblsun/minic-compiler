"""Exercício 08 — validação de tipos como atributos sintetizados.

Entrada: declarações e expressões, em qualquer ordem, cada uma terminada por
`;`:

    item -> T id { , id } ;   |   expr ;        T -> int | float | bool

As expressões são analisadas pelo parser do exercício 06 (a AST não muda).
Depois, uma passagem separada — `check_expr(node, env)` — calcula o atributo
sintetizado `type` de cada nó a partir do `type` dos filhos. Os tipos ficam
numa tabela à parte (nó -> tipo); a árvore sintática não é alterada.

Regras (política de promoção: int -> float implícito, nunca o contrário):

    literal inteiro / real / true|false     int / float / bool
    identificador                           o tipo declarado (não declarado: erro)
    + - * /     numérico, numérico          float se algum for float; senão int
    %           int, int                    int
    < <= > >=   numérico, numérico          bool
    == !=       numérico, numérico | bool, bool    bool
    && ||       bool, bool                  bool
    - (unário)  numérico                    o mesmo tipo
    ! (unário)  bool                        bool

Operando incompatível gera um diagnóstico com a posição do operador e o nó
recebe o tipo `erro`. Um nó cujo filho já é `erro` também vira `erro`, mas sem
novo diagnóstico — o erro original não se multiplica pela árvore acima.

Saída: cada expressão com o tipo de cada nó, `(+:float 3.0:float 4:int)`.
Códigos de saída distintos: 3 para erro de sintaxe (nada é verificado) e 4
para erro semântico (a AST é impressa mesmo assim).

Uso:  python tipos.py <arquivo | ->
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "ex06_expressoes_minic"))
import expr as E  # noqa: E402
from expr import F  # noqa: E402

TIPOS_DECL = {"KW_INT": "int", "KW_FLOAT": "float", "KW_BOOL": "bool"}
NUMERICOS = ("int", "float")
ERRO = "erro"


class Verificador:
    def __init__(self):
        self.env: dict[str, str] = {}
        self.tipos: dict[int, str] = {}  # id(nó) -> tipo: o atributo, fora da AST
        self.erros: list[str] = []

    def _erro(self, no, mensagem: str):
        self.erros.append(F.erro_semantico(no.linha, no.coluna, mensagem))

    def check_expr(self, no: E.Expr) -> str:
        tipo = self._tipo(no)
        self.tipos[id(no)] = tipo
        return tipo

    def _tipo(self, no: E.Expr) -> str:
        match no:
            case E.Integer():
                return "int"
            case E.Float():
                return "float"
            case E.Bool():
                return "bool"
            case E.Identifier(nome=nome):
                if nome not in self.env:
                    self._erro(no, f"identificador '{nome}' não declarado")
                    return ERRO
                return self.env[nome]
            case E.Unary(op=op, operando=o):
                t = self.check_expr(o)
                if t == ERRO:
                    return ERRO
                if op == "-" and t in NUMERICOS:
                    return t
                if op == "!" and t == "bool":
                    return "bool"
                exigido = "numérico" if op == "-" else "bool"
                self._erro(no, f"operador '{op}' exige operando {exigido}; recebeu {t}")
                return ERRO
            case E.Binary(op=op, esq=e, dir=d):
                a, b = self.check_expr(e), self.check_expr(d)
                if ERRO in (a, b):
                    return ERRO
                return self._binario(no, op, a, b)

    def _binario(self, no, op: str, a: str, b: str) -> str:
        numericos = a in NUMERICOS and b in NUMERICOS
        if op in ("+", "-", "*", "/"):
            if numericos:
                return "float" if "float" in (a, b) else "int"
            exigido = "numéricos"
        elif op == "%":
            if a == b == "int":
                return "int"
            exigido = "int"
        elif op in ("<", "<=", ">", ">="):
            if numericos:
                return "bool"
            exigido = "numéricos"
        elif op in ("==", "!="):
            if numericos or a == b == "bool":
                return "bool"
            exigido = "do mesmo tipo (numéricos ou bool)"
        else:  # && ||
            if a == b == "bool":
                return "bool"
            exigido = "bool"
        self._erro(no, f"operador '{op}' exige operandos {exigido}; recebeu {a} e {b}")
        return ERRO

    def declara(self, tok, tipo: str):
        if tok.lexeme in self.env:
            self.erros.append(
                F.erro_semantico(tok.line, tok.column, f"redeclaração de '{tok.lexeme}'")
            )
            return
        self.env[tok.lexeme] = tipo

    def anotada(self, no: E.Expr) -> str:
        t = self.tipos[id(no)]
        match no:
            case E.Unary(op=op, operando=o):
                return f"({op}:{t} {self.anotada(o)})"
            case E.Binary(op=op, esq=e, dir=d):
                return f"({op}:{t} {self.anotada(e)} {self.anotada(d)})"
            case _:
                return f"{E.sexpr(no)}:{t}"


def main(argv: list[str]) -> int:
    F.configurar_saida()
    if len(argv) != 2:
        print("uso: python tipos.py <arquivo | ->", file=sys.stderr)
        return F.ERRO_USO
    fl, codigo = F.abrir(argv[1])
    if fl is None:
        return codigo

    # 1) análise sintática: declarações e ASTs das expressões
    itens = []
    parser = E.ExprParser(fl)
    try:
        while not fl.verifica("EOF"):
            tok = fl.atual()
            if tok.type in TIPOS_DECL:
                fl.proximo()
                ids = [fl.exige("IDENT", "identificador")]
                while fl.aceita("COMMA"):
                    ids.append(fl.exige("IDENT", "identificador"))
                fl.exige("SEMICOLON", "',' ou ';'")
                itens.append(("decl", TIPOS_DECL[tok.type], ids))
            else:
                arvore = parser.parse_expr()
                fl.exige("SEMICOLON", "operador ou ';'")
                itens.append(("expr", arvore))
    except F.ErroSintaxe as erro:
        print(erro, file=sys.stderr)
        return F.ERRO_SINTAXE

    # 2) análise semântica: atributo `type` de cada nó
    v = Verificador()
    for item in itens:
        if item[0] == "decl":
            for tok in item[2]:
                v.declara(tok, item[1])
        else:
            v.check_expr(item[1])
            print(v.anotada(item[1]))
    for e in v.erros:
        print(e, file=sys.stderr)
    return F.ERRO_SEMANTICO if v.erros else F.OK


if __name__ == "__main__":
    sys.exit(main(sys.argv))
