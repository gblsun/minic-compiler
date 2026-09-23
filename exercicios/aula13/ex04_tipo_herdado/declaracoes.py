"""Exercício 04 — propagação de tipo herdado em listas de identificadores.

Gramática e SDD L-atribuída:

    P -> D*
    D -> T L ;        L.inh = T.type
    T -> int          T.type = int        (idem float, char)
    L -> L1 , id      L1.inh = L.inh;  addtype(id, L.inh)
    L -> id           addtype(id, L.inh)

`T.type` é sintetizado; `L.inh` é herdado — vem do irmão à esquerda (T) e
desce pela lista. A SDD é L-atribuída porque todo atributo herdado depende só
de atributos do pai ou de irmãos **à esquerda**: L.inh usa T.type (à esquerda
de L em D -> T L ;) e L1.inh usa L.inh (do pai). Nada depende de algo à
direita, então uma única passada da esquerda para a direita — a ordem em que o
parser descendente lê os tokens — calcula tudo.

Em código, "herdado" vira **parâmetro**: `parse_L(tipo)` recebe o valor de
L.inh de quem a chama. A recursão à esquerda de L vira um laço
(`L -> id { , id }`), com o mesmo tipo repassado a cada identificador.

A tabela de símbolos guarda nome, tipo, linha e coluna, e lista em ordem de
declaração. Redeclarar um nome é erro semântico: a análise continua (para
mostrar todos os erros), a primeira declaração fica valendo e o código de
saída é 4.

Uso:  python declaracoes.py [--trace] <arquivo | ->
"""

import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "comum"))
import fluxo as F  # noqa: E402

TIPOS = {"KW_INT": "int", "KW_FLOAT": "float", "KW_CHAR": "char"}


@dataclass
class Simbolo:
    nome: str
    tipo: str
    linha: int
    coluna: int


class TabelaSimbolos:
    def __init__(self):
        self.simbolos: list[Simbolo] = []  # ordem de declaração
        self._por_nome: dict[str, Simbolo] = {}

    def procura(self, nome: str):
        return self._por_nome.get(nome)

    def insere(self, simbolo: Simbolo):
        self.simbolos.append(simbolo)
        self._por_nome[simbolo.nome] = simbolo


class Declaracoes:
    def __init__(self, fl: F.Fluxo, trace: bool):
        self.fl = fl
        self.trace = trace
        self.tabela = TabelaSimbolos()
        self.erros: list[str] = []

    def _regra(self, producao: str, acao: str):
        if self.trace:
            print(f"{producao:<14}{acao}")

    def parse_P(self):
        while not self.fl.verifica("EOF"):
            self.parse_D()

    def parse_D(self):
        tipo = self.parse_T()
        self._regra("D -> T L ;", f"L.inh = T.type = {tipo}")
        self.parse_L(tipo)
        self.fl.exige("SEMICOLON", "',' ou ';'")

    def parse_T(self) -> str:
        tok = self.fl.atual()
        if tok.type not in TIPOS:
            raise F.ErroSintaxe(tok, "tipo (int, float ou char)")
        self.fl.proximo()
        tipo = TIPOS[tok.type]
        self._regra(f"T -> {tipo}", f"T.type = {tipo}")
        return tipo

    def parse_L(self, inh: str):
        """L -> id { , id }; `inh` é o atributo herdado L.inh."""
        self.addtype(self.fl.exige("IDENT", "identificador"), inh, "L -> id")
        while self.fl.aceita("COMMA"):
            self.addtype(self.fl.exige("IDENT", "identificador"), inh, "L -> L , id")

    def addtype(self, tok, tipo: str, producao: str):
        """Ação semântica: registra id com o tipo herdado."""
        anterior = self.tabela.procura(tok.lexeme)
        if anterior is not None:
            self._regra(producao, f"addtype({tok.lexeme}, L.inh) -> redeclaração")
            self.erros.append(
                F.erro_semantico(
                    tok.line,
                    tok.column,
                    f"redeclaração de '{tok.lexeme}' (declarado na linha {anterior.linha}, "
                    f"coluna {anterior.coluna})",
                )
            )
            return
        self._regra(producao, f"addtype({tok.lexeme}, L.inh) -> {tok.lexeme} : {tipo}")
        self.tabela.insere(Simbolo(tok.lexeme, tipo, tok.line, tok.column))


def main(argv: list[str]) -> int:
    F.configurar_saida()
    trace = "--trace" in argv[1:]
    args = [a for a in argv[1:] if a != "--trace"]
    if len(args) != 1:
        print("uso: python declaracoes.py [--trace] <arquivo | ->", file=sys.stderr)
        return F.ERRO_USO

    fl, codigo = F.abrir(args[0])
    if fl is None:
        return codigo
    d = Declaracoes(fl, trace)
    try:
        d.parse_P()
    except F.ErroSintaxe as erro:
        print(erro, file=sys.stderr)
        return F.ERRO_SINTAXE

    print("tabela de símbolos:")
    for s in d.tabela.simbolos:
        print(f"  {s.nome} : {s.tipo} (linha {s.linha}, coluna {s.coluna})")
    for e in d.erros:
        print(e, file=sys.stderr)
    return F.ERRO_SEMANTICO if d.erros else F.OK


if __name__ == "__main__":
    sys.exit(main(sys.argv))
