"""Exercício 10 — mini pipeline scanner -> tokens -> parser -> atributos -> AST.

Subconjunto MINIC implementado:

    programa  -> item*
    item      -> decl | cmd
    decl      -> tipo decl_item { , decl_item } ;      tipo -> int | float | bool
    decl_item -> id [ = expr ]
    cmd       -> bloco | if | while | return | atrib
    bloco     -> { item* }
    if        -> if ( expr ) cmd [ else cmd ]
    while     -> while ( expr ) cmd
    return    -> return [ expr ] ;
    atrib     -> id = expr ;
    expr      -> níveis || , && , == != , < <= > >= , + - , * / % , unário - ! ,
                 primária (id, int, float, true, false, ( expr ))

Atributos:

- **herdados**: `tipo` desce da palavra de tipo para cada decl_item
  (`decl_item.inh = tipo.type`, como no exercício 04); `escopo` desce do
  bloco para tudo o que está dentro dele (como no exercício 05). Os dois são
  parâmetros das funções do parser;
- **sintetizados**: `node` (a AST, construída pelas ações) e `tipo` de cada
  expressão, calculado a partir dos filhos com as regras do exercício 08
  (promoção int -> float; `erro` não gera diagnóstico em cascata).

Dependências relevantes: o tipo de um Id depende da declaração visível no
escopo herdado; o tipo de um Binary depende dos tipos dos dois filhos; a
checagem de uma atribuição depende do tipo do alvo e do valor. Tudo isso fica
disponível numa única passada da esquerda para a direita, então as ações
rodam durante a própria análise sintática.

Regras semânticas verificadas: identificador não declarado, redeclaração no
mesmo escopo (sombrear um escopo externo é permitido), compatibilidade de
tipos em inicialização e atribuição (mesmo tipo, ou int -> float) e condição
de if/while do tipo bool.

Limitações: sem funções, vetores, for, print/read nem char/string; a análise
para no primeiro erro de sintaxe (recuperação é o assunto do exercício 09).

Uso:  python parser.py [arquivo | -]      (sem argumento: lê a entrada padrão)
"""

import sys
from dataclasses import dataclass
from pathlib import Path

import ast_nodes as A
from scanner import TokenStream, scan

OK, ERRO_USO, ERRO_LEXICO, ERRO_SINTAXE, ERRO_SEMANTICO = 0, 1, 2, 3, 4
TIPOS = {"KW_INT": "int", "KW_FLOAT": "float", "KW_BOOL": "bool"}
NUMERICOS = ("int", "float")
ERRO = "erro"
NIVEIS = [("OR",), ("AND",), ("EQ", "NEQ"), ("LT", "LE", "GT", "GE"), ("PLUS", "MINUS"), ("STAR", "SLASH", "PERCENT")]


class ErroSintaxe(Exception):
    pass


class Scope:
    def __init__(self, nome: str, pai: "Scope | None"):
        self.nome = nome
        self.pai = pai
        self.simbolos: dict[str, tuple[str, int, int]] = {}

    def lookup(self, nome: str):
        s = self
        while s is not None:
            if nome in s.simbolos:
                return s.simbolos[nome]
            s = s.pai
        return None


@dataclass
class Resultado:
    programa: A.Programa | None
    diagnosticos: list[str]
    codigo: int


class Parser:
    def __init__(self, ts: TokenStream):
        self.ts = ts
        self.diags: list[str] = []
        self.blocos = 0

    # -- utilitários ------------------------------------------------------

    def _erro_sintaxe(self, token, esperado: str):
        encontrado = "fim da entrada" if token.type == "EOF" else f"'{token.lexeme}'"
        raise ErroSintaxe(
            f"Erro de sintaxe na linha {token.line}, coluna {token.column}: "
            f"esperado {esperado}; encontrado {encontrado}."
        )

    def _exige(self, tipo: str, esperado: str):
        if not self.ts.check(tipo):
            self._erro_sintaxe(self.ts.lookahead(), esperado)
        return self.ts.next_token()

    def _semantico(self, linha: int, coluna: int, mensagem: str):
        self.diags.append(f"Erro semântico na linha {linha}, coluna {coluna}: {mensagem}.")

    # -- programa, declarações e comandos --------------------------------

    def programa(self) -> A.Programa:
        escopo = Scope("global", None)
        itens = []
        while not self.ts.check("EOF"):
            itens += self.item(escopo)
        return A.Programa(tuple(itens))

    def item(self, escopo: Scope) -> list:
        if self.ts.lookahead().type in TIPOS:
            return self.decl(escopo)
        return [self.cmd(escopo)]

    def decl(self, escopo: Scope) -> list:
        tipo = TIPOS[self.ts.next_token().type]  # tipo.type (sintetizado)
        itens = [self.decl_item(tipo, escopo)]  # decl_item.inh = tipo.type
        while self.ts.accept("COMMA"):
            itens.append(self.decl_item(tipo, escopo))
        self._exige("SEMICOLON", "',' ou ';'")
        return itens

    def decl_item(self, inh: str, escopo: Scope) -> A.VarDecl:
        tok = self._exige("IDENT", "identificador")
        init = self.expr(escopo) if self.ts.accept("ASSIGN") else None
        if init is not None and not compativel(inh, init.tipo):
            self._semantico(
                init.linha, init.coluna, f"não é possível inicializar '{tok.lexeme}' ({inh}) com {init.tipo}"
            )
        if tok.lexeme in escopo.simbolos:
            _, l, c = escopo.simbolos[tok.lexeme]
            self._semantico(
                tok.line, tok.column,
                f"redeclaração de '{tok.lexeme}' no escopo {escopo.nome} (declarado na linha {l}, coluna {c})",
            )
        else:
            escopo.simbolos[tok.lexeme] = (inh, tok.line, tok.column)
        return A.VarDecl(tok.lexeme, inh, escopo.nome, init, tok.line, tok.column)

    def cmd(self, escopo: Scope):
        tok = self.ts.lookahead()
        if tok.type == "LBRACE":
            self.ts.next_token()
            self.blocos += 1
            interno = Scope(f"bloco{self.blocos}", escopo)
            itens = []
            while not self.ts.check("RBRACE"):
                if self.ts.check("EOF"):
                    self._erro_sintaxe(self.ts.lookahead(), "'}'")
                itens += self.item(interno)
            self.ts.next_token()
            return A.Block(interno.nome, tuple(itens), tok.line, tok.column)
        if tok.type in ("KW_IF", "KW_WHILE"):
            self.ts.next_token()
            self._exige("LPAREN", "'('")
            cond = self.expr(escopo)
            self._exige("RPAREN", "operador ou ')'")
            nome = "if" if tok.type == "KW_IF" else "while"
            if cond.tipo not in ("bool", ERRO):
                self._semantico(cond.linha, cond.coluna, f"condição do {nome} deve ser bool; recebeu {cond.tipo}")
            corpo = self.cmd(escopo)
            if tok.type == "KW_WHILE":
                return A.While(cond, corpo, tok.line, tok.column)
            senao = self.cmd(escopo) if self.ts.accept("KW_ELSE") else None
            return A.If(cond, corpo, senao, tok.line, tok.column)
        if tok.type == "KW_RETURN":
            self.ts.next_token()
            valor = None if self.ts.check("SEMICOLON") else self.expr(escopo)
            self._exige("SEMICOLON", "operador ou ';'")
            return A.Return(valor, tok.line, tok.column)
        if tok.type == "IDENT":
            alvo = self.identificador(self.ts.next_token(), escopo)
            self._exige("ASSIGN", "'='")
            valor = self.expr(escopo)
            self._exige("SEMICOLON", "operador ou ';'")
            if ERRO not in (alvo.tipo, valor.tipo) and not compativel(alvo.tipo, valor.tipo):
                self._semantico(
                    valor.linha, valor.coluna, f"não é possível atribuir {valor.tipo} a '{alvo.nome}' ({alvo.tipo})"
                )
            return A.Assign(alvo, valor, alvo.linha, alvo.coluna)
        self._erro_sintaxe(tok, "comando")

    # -- expressões (ações calculam o atributo sintetizado `tipo`) ----------

    def expr(self, escopo: Scope, nivel: int = 0) -> A.Expr:
        if nivel == len(NIVEIS):
            return self.unaria(escopo)
        no = self.expr(escopo, nivel + 1)
        while self.ts.check(*NIVEIS[nivel]):
            op = self.ts.next_token()
            direita = self.expr(escopo, nivel + 1)
            tipo = self.tipo_binario(op, no.tipo, direita.tipo)
            no = A.Binary(op.lexeme, no, direita, tipo, op.line, op.column)
        return no

    def unaria(self, escopo: Scope) -> A.Expr:
        op = self.ts.accept("MINUS", "NOT")
        if op is None:
            return self.primaria(escopo)
        o = self.unaria(escopo)
        tipo = ERRO
        if o.tipo == ERRO:
            pass
        elif op.lexeme == "-" and o.tipo in NUMERICOS:
            tipo = o.tipo
        elif op.lexeme == "!" and o.tipo == "bool":
            tipo = "bool"
        else:
            exigido = "numérico" if op.lexeme == "-" else "bool"
            self._semantico(op.line, op.column, f"operador '{op.lexeme}' exige operando {exigido}; recebeu {o.tipo}")
        return A.Unary(op.lexeme, o, tipo, op.line, op.column)

    def primaria(self, escopo: Scope) -> A.Expr:
        tok = self.ts.lookahead()
        literais = {"INT": "int", "FLOAT": "float", "KW_TRUE": "bool", "KW_FALSE": "bool"}
        if tok.type in literais:
            self.ts.next_token()
            return A.Literal(tok.lexeme, literais[tok.type], tok.line, tok.column)
        if tok.type == "IDENT":
            return self.identificador(self.ts.next_token(), escopo)
        if self.ts.accept("LPAREN"):
            e = self.expr(escopo)
            self._exige("RPAREN", "operador ou ')'")
            return e
        self._erro_sintaxe(tok, "expressão")

    def identificador(self, tok, escopo: Scope) -> A.Id:
        decl = escopo.lookup(tok.lexeme)
        if decl is None:
            self._semantico(tok.line, tok.column, f"identificador '{tok.lexeme}' não declarado")
            return A.Id(tok.lexeme, ERRO, tok.line, tok.column)
        return A.Id(tok.lexeme, decl[0], tok.line, tok.column)

    def tipo_binario(self, op, a: str, b: str) -> str:
        if ERRO in (a, b):
            return ERRO
        s = op.lexeme
        nums = a in NUMERICOS and b in NUMERICOS
        if s in ("+", "-", "*", "/"):
            if nums:
                return "float" if "float" in (a, b) else "int"
            exigido = "numéricos"
        elif s == "%":
            if a == b == "int":
                return "int"
            exigido = "int"
        elif s in ("<", "<=", ">", ">="):
            if nums:
                return "bool"
            exigido = "numéricos"
        elif s in ("==", "!="):
            if nums or a == b == "bool":
                return "bool"
            exigido = "do mesmo tipo (numéricos ou bool)"
        else:
            if a == b == "bool":
                return "bool"
            exigido = "bool"
        self._semantico(op.line, op.column, f"operador '{s}' exige operandos {exigido}; recebeu {a} e {b}")
        return ERRO


def compativel(destino: str, origem: str) -> bool:
    return origem == ERRO or destino == origem or (destino == "float" and origem == "int")


def parse(source: str) -> Resultado:
    """Pipeline completo: scanner -> parser/ações -> AST com atributos."""
    ts, erros_lexicos = scan(source)
    if erros_lexicos:
        return Resultado(None, [str(e) for e in erros_lexicos], ERRO_LEXICO)
    p = Parser(ts)
    try:
        programa = p.programa()
    except ErroSintaxe as erro:
        return Resultado(None, [str(erro)], ERRO_SINTAXE)
    return Resultado(programa, p.diags, ERRO_SEMANTICO if p.diags else OK)


def main(argv: list[str]) -> int:
    sys.stdout.reconfigure(encoding="utf-8", errors="surrogateescape")
    sys.stderr.reconfigure(encoding="utf-8", errors="surrogateescape")
    if len(argv) > 2:
        print("uso: python parser.py [arquivo | -]", file=sys.stderr)
        return ERRO_USO
    caminho = argv[1] if len(argv) == 2 else "-"
    try:
        dados = sys.stdin.buffer.read() if caminho == "-" else Path(caminho).read_bytes()
    except OSError as exc:
        print(f"erro ao abrir '{caminho}': {exc.strerror}", file=sys.stderr)
        return ERRO_USO

    r = parse(dados.decode("utf-8", errors="surrogateescape"))
    if r.programa is not None:
        print("\n".join(A.serializa(r.programa)))
    for d in r.diagnosticos:
        print(d, file=sys.stderr)
    return r.codigo


if __name__ == "__main__":
    sys.exit(main(sys.argv))
