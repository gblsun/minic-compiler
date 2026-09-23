"""Exercício 07 — ações semânticas para comandos e fluxo de controle.

Constrói a AST de comandos de um subconjunto MINIC (sem gerar código). As
expressões vêm do parser do exercício 06 (ex06_expressoes_minic/expr.py).

Gramática:

    programa -> comando*
    comando  -> bloco | if | while | return | atrib | expr_cmd
    bloco    -> { comando* }
    if       -> if ( expr ) comando [ else comando ]
    while    -> while ( expr ) comando
    return   -> return [ expr ] ;
    atrib    -> id = expr ;
    expr_cmd -> expr ;

Ações (atributo sintetizado `node`) e invariantes de cada nó:

    bloco    Block(comandos)          lista na ordem do texto; pode ser vazia
    if       If(cond, entao, senao)   cond e entao nunca ausentes; senao = None
                                      quando não há else (ausência explícita)
    while    While(cond, corpo)       os dois sempre presentes
    return   Return(valor)            valor = None em `return;`
    atrib    Assign(alvo, valor)      alvo é sempre um Identifier
    expr_cmd ExprStmt(expr)

`atrib` e `expr_cmd` começam igual (uma expressão). O parser lê a expressão e,
se vier `=`, exige que ela seja um identificador — só então o nó vira Assign.
O `else` pendente fica com o `if` mais próximo, porque o `if` interno consome
o `else` assim que o encontra.

Uso:  python comandos.py <arquivo | ->
"""

import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "ex06_expressoes_minic"))
import expr as E  # noqa: E402
from expr import F  # noqa: E402


@dataclass(frozen=True)
class Block:
    comandos: tuple
    linha: int
    coluna: int


@dataclass(frozen=True)
class Assign:
    alvo: E.Identifier
    valor: E.Expr
    linha: int
    coluna: int


@dataclass(frozen=True)
class If:
    cond: E.Expr
    entao: "Comando"
    senao: "Comando | None"
    linha: int
    coluna: int


@dataclass(frozen=True)
class While:
    cond: E.Expr
    corpo: "Comando"
    linha: int
    coluna: int


@dataclass(frozen=True)
class Return:
    valor: E.Expr | None
    linha: int
    coluna: int


@dataclass(frozen=True)
class ExprStmt:
    expr: E.Expr
    linha: int
    coluna: int


Comando = Block | Assign | If | While | Return | ExprStmt


class Parser:
    def __init__(self, fl: F.Fluxo):
        self.fl = fl
        self.expr = E.ExprParser(fl)

    def parse_programa(self) -> list:
        comandos = []
        while not self.fl.verifica("EOF"):
            comandos.append(self.parse_comando())
        return comandos

    def parse_comando(self) -> Comando:
        tok = self.fl.atual()
        if tok.type == "LBRACE":
            return self.parse_bloco()
        if tok.type == "KW_IF":
            self.fl.proximo()
            cond = self._condicao()
            entao = self.parse_comando()
            senao = self.parse_comando() if self.fl.aceita("KW_ELSE") else None
            return If(cond, entao, senao, tok.line, tok.column)
        if tok.type == "KW_WHILE":
            self.fl.proximo()
            cond = self._condicao()
            return While(cond, self.parse_comando(), tok.line, tok.column)
        if tok.type == "KW_RETURN":
            self.fl.proximo()
            valor = None if self.fl.verifica("SEMICOLON") else self.expr.parse_expr()
            self.fl.exige("SEMICOLON", "operador ou ';'")
            return Return(valor, tok.line, tok.column)
        if tok.type in ("RBRACE", "KW_ELSE", "EOF", "RPAREN", "SEMICOLON"):
            raise F.ErroSintaxe(tok, "comando")
        e = self.expr.parse_expr()
        igual = self.fl.aceita("ASSIGN")
        if igual is not None:
            if not isinstance(e, E.Identifier):
                raise F.ErroSintaxe(igual, "';' (o lado esquerdo de '=' deve ser um identificador)")
            valor = self.expr.parse_expr()
            self.fl.exige("SEMICOLON", "operador ou ';'")
            return Assign(e, valor, e.linha, e.coluna)
        self.fl.exige("SEMICOLON", "operador, '=' ou ';'")
        return ExprStmt(e, tok.line, tok.column)

    def parse_bloco(self) -> Block:
        abre = self.fl.exige("LBRACE", "'{'")
        comandos = []
        while not self.fl.verifica("RBRACE"):
            if self.fl.verifica("EOF"):
                raise F.ErroSintaxe(self.fl.atual(), "'}'")
            comandos.append(self.parse_comando())
        self.fl.proximo()
        return Block(tuple(comandos), abre.line, abre.column)

    def _condicao(self) -> E.Expr:
        self.fl.exige("LPAREN", "'('")
        cond = self.expr.parse_expr()
        self.fl.exige("RPAREN", "operador ou ')'")
        return cond


def imprime(no: Comando, nivel: int, rotulo: str = ""):
    ind = "  " * nivel
    p = f"{ind}{rotulo}: " if rotulo else ind
    filho = "  " * (nivel + 1)
    match no:
        case Block(comandos=cs, linha=l, coluna=c):
            print(f"{p}Block @{l}:{c}" + (" (vazio)" if not cs else ""))
            for cmd in cs:
                imprime(cmd, nivel + 1)
        case Assign(alvo=a, valor=v, linha=l, coluna=c):
            print(f"{p}Assign @{l}:{c}")
            print(f"{filho}alvo: {E.sexpr(a)}")
            print(f"{filho}valor: {E.sexpr(v)}")
        case If(cond=cond, entao=entao, senao=senao, linha=l, coluna=c):
            print(f"{p}If @{l}:{c}")
            print(f"{filho}cond: {E.sexpr(cond)}")
            imprime(entao, nivel + 1, "entao")
            if senao is None:
                print(f"{filho}senao: <ausente>")
            else:
                imprime(senao, nivel + 1, "senao")
        case While(cond=cond, corpo=corpo, linha=l, coluna=c):
            print(f"{p}While @{l}:{c}")
            print(f"{filho}cond: {E.sexpr(cond)}")
            imprime(corpo, nivel + 1, "corpo")
        case Return(valor=v, linha=l, coluna=c):
            print(f"{p}Return @{l}:{c}")
            print(f"{filho}valor: " + ("<ausente>" if v is None else E.sexpr(v)))
        case ExprStmt(expr=e, linha=l, coluna=c):
            print(f"{p}ExprStmt @{l}:{c}")
            print(f"{filho}expr: {E.sexpr(e)}")


def main(argv: list[str]) -> int:
    F.configurar_saida()
    if len(argv) != 2:
        print("uso: python comandos.py <arquivo | ->", file=sys.stderr)
        return F.ERRO_USO
    fl, codigo = F.abrir(argv[1])
    if fl is None:
        return codigo
    try:
        comandos = Parser(fl).parse_programa()
    except F.ErroSintaxe as erro:
        print(erro, file=sys.stderr)
        return F.ERRO_SINTAXE
    print("Programa" + (" (vazio)" if not comandos else ""))
    for cmd in comandos:
        imprime(cmd, 1)
    return F.OK


if __name__ == "__main__":
    sys.exit(main(sys.argv))
