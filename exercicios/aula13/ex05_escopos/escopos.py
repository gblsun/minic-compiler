"""Exercício 05 — parser descendente com contexto herdado (escopos).

Amplia o exercício 04 com blocos aninhados. Gramática:

    P      -> item*                     (escopo "global")
    item   -> decl | bloco | uso
    decl   -> T id { , id } ;           T -> int | float | char | bool
    bloco  -> { item* }                 (abre um escopo filho do atual)
    uso    -> id = valor ;              valor -> id | num

Atributo herdado: o **escopo atual**. Ele desce como parâmetro explícito
(`parse_item(escopo)`, `parse_bloco(pai)`, ...) — não há variável global com
"o escopo corrente". Ao entrar num bloco, `Scope(pai)` cria o escopo filho;
ao sair (`}`), o parser simplesmente volta a usar o escopo que recebeu, e o
filho deixa de ser alcançável.

Regras semânticas:

- declarar um nome já declarado **no mesmo escopo** é erro (redeclaração);
- declarar um nome que existe num escopo ancestral é **sombreamento
  permitido**, registrado na AST (`sombreia x de global (1:5)`);
- cada uso procura o nome primeiro no escopo atual e depois nos ancestrais; o
  nó Id guarda a declaração resolvida (escopo, tipo e posição). Nome ausente é
  erro semântico na posição do uso.

A AST guarda linha e coluna de todos os nós, e os erros semânticos não
interrompem a análise (código de saída 4 no fim).

Uso:  python escopos.py <arquivo | ->
"""

import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "comum"))
import fluxo as F  # noqa: E402

TIPOS = {"KW_INT": "int", "KW_FLOAT": "float", "KW_CHAR": "char", "KW_BOOL": "bool"}


# -- escopos ---------------------------------------------------------------


@dataclass
class Simbolo:
    nome: str
    tipo: str
    linha: int
    coluna: int
    escopo: str


class Scope:
    def __init__(self, nome: str, parent: "Scope | None"):
        self.nome = nome
        self.parent = parent
        self.symbols: dict[str, Simbolo] = {}

    def declare(self, simbolo: Simbolo):
        """Insere no escopo atual; devolve a declaração anterior do mesmo escopo, se houver."""
        anterior = self.symbols.get(simbolo.nome)
        if anterior is None:
            self.symbols[simbolo.nome] = simbolo
        return anterior

    def lookup(self, nome: str):
        """Procura no escopo atual e depois nos ancestrais."""
        escopo = self
        while escopo is not None:
            if nome in escopo.symbols:
                return escopo.symbols[nome]
            escopo = escopo.parent
        return None


# -- AST -------------------------------------------------------------------


@dataclass
class Id:
    nome: str
    linha: int
    coluna: int
    decl: Simbolo | None  # declaração resolvida (None = não declarado)


@dataclass
class Num:
    lexema: str
    linha: int
    coluna: int


@dataclass
class VarDecl:
    simbolo: Simbolo
    sombreia: Simbolo | None
    redeclaracao: bool


@dataclass
class Assign:
    alvo: Id
    valor: "Id | Num"
    linha: int
    coluna: int


@dataclass
class Block:
    escopo: str
    itens: list = field(default_factory=list)


def imprime(no, nivel: int = 0):
    ind = "  " * nivel
    match no:
        case Block(escopo=escopo, itens=itens):
            print(f"{ind}Block escopo={escopo}")
            for item in itens:
                imprime(item, nivel + 1)
        case VarDecl(simbolo=s, sombreia=sombra, redeclaracao=redecl):
            texto = f"{ind}VarDecl {s.nome} : {s.tipo} @{s.linha}:{s.coluna} [{s.escopo}]"
            if sombra is not None:
                texto += f" sombreia {sombra.nome} de {sombra.escopo} ({sombra.linha}:{sombra.coluna})"
            if redecl:
                texto += " redeclaração"
            print(texto)
        case Assign(alvo=alvo, valor=valor, linha=l, coluna=c):
            print(f"{ind}Assign @{l}:{c}")
            imprime(alvo, nivel + 1)
            imprime(valor, nivel + 1)
        case Id(nome=nome, linha=l, coluna=c, decl=d):
            destino = (
                f"{d.tipo} [{d.escopo}, declarado em {d.linha}:{d.coluna}]" if d else "não declarado"
            )
            print(f"{ind}Id {nome} @{l}:{c} -> {destino}")
        case Num(lexema=lex, linha=l, coluna=c):
            print(f"{ind}Num {lex} @{l}:{c}")


# -- parser ----------------------------------------------------------------


class Parser:
    def __init__(self, fl: F.Fluxo):
        self.fl = fl
        self.erros: list[str] = []
        self.blocos = 0  # só para dar nome aos escopos (bloco1, bloco2, ...)

    def parse_P(self) -> Block:
        global_ = Scope("global", None)
        raiz = Block("global")
        while not self.fl.verifica("EOF"):
            raiz.itens.extend(self.parse_item(global_))
        return raiz

    def parse_item(self, escopo: Scope) -> list:
        if self.fl.atual().type in TIPOS:
            return self.parse_decl(escopo)
        if self.fl.verifica("LBRACE"):
            return [self.parse_bloco(escopo)]
        if self.fl.verifica("IDENT"):
            return [self.parse_uso(escopo)]
        raise F.ErroSintaxe(self.fl.atual(), "declaração, bloco ou atribuição")

    def parse_bloco(self, pai: Scope) -> Block:
        self.fl.exige("LBRACE", "'{'")
        self.blocos += 1
        escopo = Scope(f"bloco{self.blocos}", pai)  # enter_scope
        bloco = Block(escopo.nome)
        while not self.fl.verifica("RBRACE"):
            if self.fl.verifica("EOF"):
                raise F.ErroSintaxe(self.fl.atual(), "'}'")
            bloco.itens.extend(self.parse_item(escopo))
        self.fl.proximo()
        return bloco  # leave_scope: `escopo` sai de alcance aqui

    def parse_decl(self, escopo: Scope) -> list:
        tipo = TIPOS[self.fl.proximo().type]
        decls = [self.declara(self.fl.exige("IDENT", "identificador"), tipo, escopo)]
        while self.fl.aceita("COMMA"):
            decls.append(self.declara(self.fl.exige("IDENT", "identificador"), tipo, escopo))
        self.fl.exige("SEMICOLON", "',' ou ';'")
        return decls

    def declara(self, tok, tipo: str, escopo: Scope) -> VarDecl:
        simbolo = Simbolo(tok.lexeme, tipo, tok.line, tok.column, escopo.nome)
        sombra = escopo.parent.lookup(tok.lexeme) if escopo.parent else None
        anterior = escopo.declare(simbolo)
        if anterior is not None:
            self.erros.append(
                F.erro_semantico(
                    tok.line,
                    tok.column,
                    f"redeclaração de '{tok.lexeme}' no escopo {escopo.nome} "
                    f"(declarado na linha {anterior.linha}, coluna {anterior.coluna})",
                )
            )
            return VarDecl(simbolo, None, True)
        return VarDecl(simbolo, sombra, False)

    def parse_uso(self, escopo: Scope) -> Assign:
        alvo = self.resolve(self.fl.proximo(), escopo)
        self.fl.exige("ASSIGN", "'='")
        tok = self.fl.atual()
        if tok.type == "IDENT":
            valor = self.resolve(self.fl.proximo(), escopo)
        elif tok.type == "INT":
            self.fl.proximo()
            valor = Num(tok.lexeme, tok.line, tok.column)
        else:
            raise F.ErroSintaxe(tok, "identificador ou número")
        self.fl.exige("SEMICOLON", "';'")
        return Assign(alvo, valor, alvo.linha, alvo.coluna)

    def resolve(self, tok, escopo: Scope) -> Id:
        decl = escopo.lookup(tok.lexeme)
        if decl is None:
            self.erros.append(F.erro_semantico(tok.line, tok.column, f"identificador '{tok.lexeme}' não declarado"))
        return Id(tok.lexeme, tok.line, tok.column, decl)


def main(argv: list[str]) -> int:
    F.configurar_saida()
    if len(argv) != 2:
        print("uso: python escopos.py <arquivo | ->", file=sys.stderr)
        return F.ERRO_USO
    fl, codigo = F.abrir(argv[1])
    if fl is None:
        return codigo
    p = Parser(fl)
    try:
        arvore = p.parse_P()
    except F.ErroSintaxe as erro:
        print(erro, file=sys.stderr)
        return F.ERRO_SINTAXE
    imprime(arvore)
    for e in p.erros:
        print(e, file=sys.stderr)
    return F.ERRO_SEMANTICO if p.erros else F.OK


if __name__ == "__main__":
    sys.exit(main(sys.argv))
