"""Exercício 09 — execução segura de ações e tratamento de erros.

Parser de comandos MINIC (o subconjunto do exercício 07, mais declarações)
que **acumula** diagnósticos em vez de parar no primeiro, e que nunca publica
uma AST incompleta como válida.

    programa -> item*
    item     -> decl | comando
    decl     -> T id { , id } ;          T -> int | float | bool
    comando  -> bloco | if | while | return | atrib | expr_cmd   (como no ex07)

Diagnósticos (`Diagnostic`): categoria (léxico, sintático, semântico),
mensagem, lexema, linha e coluna.

- **Léxico**: vêm do scanner do projeto. O scanner descarta o caractere
  inválido e segue, então o parser ainda roda sobre os tokens restantes e pode
  achar outros erros.
- **Sintático**: `ErroSintaxe` (exceção específica) interrompe só a produção
  atual. O laço de itens registra o diagnóstico e **sincroniza**: descarta
  tokens até um `;` (consumido) ou até `}`/fim (não consumidos, porque
  fecham o bloco). Assim, dois comandos defeituosos separados por `;` geram
  dois diagnósticos.
- **Semântico**: identificador não declarado e redeclaração, numa tabela de
  símbolos única (escopos são o assunto do exercício 05). Só é verificado num
  comando **completo**: um comando com erro de sintaxe é descartado inteiro,
  sem gerar ruído semântico.

Ações com pré-condição: cada construtor de nó (`mk_*`) confere que os filhos
exigidos existem e têm o tipo certo (ex.: o alvo de Assign é Identifier) e
lança `PreCondicaoViolada` se não — isso indicaria um defeito do próprio
parser, e não um erro do programa analisado, por isso não é capturado.

Publicação: se houver qualquer diagnóstico, **nenhuma árvore é impressa** — a
saída diz `AST inválida` e quantos diagnósticos houve. Um programa vazio e
correto publica `Programa (vazio)`: árvore vazia válida não é o mesmo que
árvore ausente.

Código de saída: o da primeira fase com erro (2 léxico, 3 sintático,
4 semântico); 0 se não houver diagnóstico.

Uso:  python erros.py <arquivo | ->
"""

import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "ex06_expressoes_minic"))
import expr as E  # noqa: E402
from expr import F  # noqa: E402

TIPOS = {"KW_INT": "int", "KW_FLOAT": "float", "KW_BOOL": "bool"}
CODIGO = {"léxico": F.ERRO_LEXICO, "sintático": F.ERRO_SINTAXE, "semântico": F.ERRO_SEMANTICO}


@dataclass(frozen=True)
class Diagnostic:
    categoria: str
    mensagem: str
    lexema: str | None  # None = fim da entrada
    linha: int
    coluna: int

    def __str__(self):
        lex = "fim da entrada" if self.lexema is None else f"'{self.lexema}'"
        return f"[{self.categoria}] linha {self.linha}, coluna {self.coluna}: {self.mensagem}; lexema: {lex}"


# -- AST e ações com pré-condições -----------------------------------------


class PreCondicaoViolada(Exception):
    """Uma ação recebeu atributos inválidos: defeito do parser, não do programa."""


@dataclass(frozen=True)
class VarDecl:
    nome: str
    tipo: str
    linha: int
    coluna: int


@dataclass(frozen=True)
class Block:
    itens: tuple
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
    entao: object
    senao: object | None
    linha: int
    coluna: int


@dataclass(frozen=True)
class While:
    cond: E.Expr
    corpo: object
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


EXPRS = (E.Identifier, E.Integer, E.Float, E.Bool, E.Unary, E.Binary)
COMANDOS = (Block, Assign, If, While, Return, ExprStmt)


def _exige(cond: bool, acao: str):
    if not cond:
        raise PreCondicaoViolada(f"pré-condição violada em {acao}")


def mk_assign(alvo, valor):
    _exige(isinstance(alvo, E.Identifier) and isinstance(valor, EXPRS), "Assign")
    return Assign(alvo, valor, alvo.linha, alvo.coluna)


def mk_if(tok, cond, entao, senao):
    _exige(isinstance(cond, EXPRS) and isinstance(entao, COMANDOS), "If")
    _exige(senao is None or isinstance(senao, COMANDOS), "If")
    return If(cond, entao, senao, tok.line, tok.column)


def mk_while(tok, cond, corpo):
    _exige(isinstance(cond, EXPRS) and isinstance(corpo, COMANDOS), "While")
    return While(cond, corpo, tok.line, tok.column)


def mk_return(tok, valor):
    _exige(valor is None or isinstance(valor, EXPRS), "Return")
    return Return(valor, tok.line, tok.column)


def mk_expr_stmt(tok, e):
    _exige(isinstance(e, EXPRS), "ExprStmt")
    return ExprStmt(e, tok.line, tok.column)


# -- parser com recuperação ------------------------------------------------


class Parser:
    def __init__(self, fl: F.Fluxo, diags: list[Diagnostic]):
        self.fl = fl
        self.expr = E.ExprParser(fl)
        self.diags = diags
        self.simbolos: dict[str, VarDecl] = {}

    def _sintatico(self, erro: F.ErroSintaxe):
        t = erro.token
        lex = None if t.type == "EOF" else t.lexeme
        self.diags.append(Diagnostic("sintático", f"esperado {erro.esperado}", lex, t.line, t.column))

    def _semantico(self, mensagem: str, lexema: str, linha: int, coluna: int):
        self.diags.append(Diagnostic("semântico", mensagem, lexema, linha, coluna))

    def _sincroniza(self):
        while not self.fl.verifica("EOF", "RBRACE"):
            if self.fl.proximo().type == "SEMICOLON":
                return

    def parse_itens(self, dentro_de_bloco: bool) -> list:
        """Lê itens até `}` (em bloco) ou até o fim; um item com erro é descartado."""
        itens = []
        while not self.fl.verifica("EOF") and not (dentro_de_bloco and self.fl.verifica("RBRACE")):
            if not dentro_de_bloco and self.fl.verifica("RBRACE"):
                t = self.fl.proximo()
                self.diags.append(Diagnostic("sintático", "'}' sem '{' correspondente", "}", t.line, t.column))
                continue
            try:
                itens.extend(self.parse_item())
            except F.ErroSintaxe as erro:
                self._sintatico(erro)
                self._sincroniza()
        return itens

    def parse_item(self) -> list:
        tok = self.fl.atual()
        if tok.type in TIPOS:
            self.fl.proximo()
            ids = [self.fl.exige("IDENT", "identificador")]
            while self.fl.aceita("COMMA"):
                ids.append(self.fl.exige("IDENT", "identificador"))
            self.fl.exige("SEMICOLON", "',' ou ';'")
            # Ação semântica só depois que a declaração inteira foi reconhecida.
            decls = []
            for t in ids:
                if t.lexeme in self.simbolos:
                    ant = self.simbolos[t.lexeme]
                    self._semantico(
                        f"redeclaração de '{t.lexeme}' (declarado na linha {ant.linha}, coluna {ant.coluna})",
                        t.lexeme, t.line, t.column,
                    )
                    continue
                d = VarDecl(t.lexeme, TIPOS[tok.type], t.line, t.column)
                self.simbolos[t.lexeme] = d
                decls.append(d)
            return decls
        cmd = self.parse_comando()
        self._verifica_nomes(cmd)
        return [cmd]

    def parse_comando(self):
        tok = self.fl.atual()
        if tok.type == "LBRACE":
            self.fl.proximo()
            itens = self.parse_itens(dentro_de_bloco=True)
            self.fl.exige("RBRACE", "'}'")
            return Block(tuple(itens), tok.line, tok.column)
        if tok.type == "KW_IF":
            self.fl.proximo()
            cond = self._condicao()
            entao = self.parse_comando()
            senao = self.parse_comando() if self.fl.aceita("KW_ELSE") else None
            return mk_if(tok, cond, entao, senao)
        if tok.type == "KW_WHILE":
            self.fl.proximo()
            cond = self._condicao()
            return mk_while(tok, cond, self.parse_comando())
        if tok.type == "KW_RETURN":
            self.fl.proximo()
            valor = None if self.fl.verifica("SEMICOLON") else self.expr.parse_expr()
            self.fl.exige("SEMICOLON", "operador ou ';'")
            return mk_return(tok, valor)
        if tok.type in ("RBRACE", "KW_ELSE", "EOF", "RPAREN", "SEMICOLON") or tok.type in TIPOS:
            raise F.ErroSintaxe(tok, "comando")
        e = self.expr.parse_expr()
        igual = self.fl.aceita("ASSIGN")
        if igual is not None:
            if not isinstance(e, E.Identifier):
                raise F.ErroSintaxe(igual, "';' (o lado esquerdo de '=' deve ser um identificador)")
            valor = self.expr.parse_expr()
            self.fl.exige("SEMICOLON", "operador ou ';'")
            return mk_assign(e, valor)
        self.fl.exige("SEMICOLON", "operador, '=' ou ';'")
        return mk_expr_stmt(tok, e)

    def _condicao(self):
        self.fl.exige("LPAREN", "'('")
        cond = self.expr.parse_expr()
        self.fl.exige("RPAREN", "operador ou ')'")
        return cond

    def _verifica_nomes(self, no):
        """Ação semântica sobre um comando completo: todo identificador declarado."""
        match no:
            case E.Identifier(nome=nome, linha=l, coluna=c):
                if nome not in self.simbolos:
                    self._semantico(f"identificador '{nome}' não declarado", nome, l, c)
            case E.Unary(operando=o):
                self._verifica_nomes(o)
            case E.Binary(esq=a, dir=b) | Assign(alvo=a, valor=b):
                self._verifica_nomes(a)
                self._verifica_nomes(b)
            case If(cond=c, entao=t, senao=s):
                for filho in (c, t, s):
                    if filho is not None:
                        self._verifica_nomes(filho)
            case While(cond=c, corpo=corpo):
                self._verifica_nomes(c)
                self._verifica_nomes(corpo)
            case Return(valor=v) | ExprStmt(expr=v):
                if v is not None:
                    self._verifica_nomes(v)
            case Block():
                pass  # os itens de um bloco já foram verificados um a um


def imprime(no, nivel: int, rotulo: str = ""):
    ind = "  " * nivel
    p = f"{ind}{rotulo}: " if rotulo else ind
    filho = "  " * (nivel + 1)
    match no:
        case VarDecl(nome=n, tipo=t, linha=l, coluna=c):
            print(f"{p}VarDecl {n} : {t} @{l}:{c}")
        case Block(itens=itens, linha=l, coluna=c):
            print(f"{p}Block @{l}:{c}" + ("" if itens else " (vazio)"))
            for item in itens:
                imprime(item, nivel + 1)
        case Assign(alvo=a, valor=v, linha=l, coluna=c):
            print(f"{p}Assign @{l}:{c}")
            print(f"{filho}alvo: {E.sexpr(a)}")
            print(f"{filho}valor: {E.sexpr(v)}")
        case If(cond=cond, entao=t, senao=s, linha=l, coluna=c):
            print(f"{p}If @{l}:{c}")
            print(f"{filho}cond: {E.sexpr(cond)}")
            imprime(t, nivel + 1, "entao")
            if s is None:
                print(f"{filho}senao: <ausente>")
            else:
                imprime(s, nivel + 1, "senao")
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


def lexema_na_posicao(fonte: str, linha: int, coluna: int) -> str | None:
    linhas = fonte.split("\n")
    if 1 <= linha <= len(linhas) and 1 <= coluna <= len(linhas[linha - 1]):
        return linhas[linha - 1][coluna - 1]
    return None


def main(argv: list[str]) -> int:
    F.configurar_saida()
    if len(argv) != 2:
        print("uso: python erros.py <arquivo | ->", file=sys.stderr)
        return F.ERRO_USO
    try:
        fonte = F.ler_fonte(argv[1])
    except OSError as exc:
        print(f"erro ao abrir '{argv[1]}': {exc.strerror}", file=sys.stderr)
        return F.ERRO_USO

    tokens, erros_lexicos = F.tokenizar(fonte)
    diags = [
        Diagnostic("léxico", e.message, lexema_na_posicao(fonte, e.line, e.column), e.line, e.column)
        for e in erros_lexicos
    ]
    itens = Parser(F.Fluxo(tokens), diags).parse_itens(dentro_de_bloco=False)

    if diags:
        diags.sort(key=lambda d: (d.linha, d.coluna))  # estável: empate mantém a ordem da fase
        for d in diags:
            print(d, file=sys.stderr)
        print(f"AST inválida: {len(diags)} diagnóstico(s); nenhuma árvore publicada.")
        return min(CODIGO[d.categoria] for d in diags)
    print("AST válida:")
    print("Programa" + ("" if itens else " (vazio)"))
    for item in itens:
        imprime(item, 1)
    return F.OK


if __name__ == "__main__":
    sys.exit(main(sys.argv))
