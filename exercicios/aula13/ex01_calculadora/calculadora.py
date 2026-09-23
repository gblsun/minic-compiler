"""Exercício 01 — calculadora de expressões com atributos sintetizados.

Gramática (a da atividade, com recursão à esquerda):

    E -> E + T | E - T | T
    T -> T * F | T / F | F
    F -> ( E ) | num

SDD S-atribuída — um único atributo, `val`, sintetizado em todos os não
terminais:

    E -> E1 + T    E.val = E1.val + T.val
    E -> E1 - T    E.val = E1.val - T.val
    E -> T         E.val = T.val
    T -> T1 * F    T.val = T1.val * F.val
    T -> T1 / F    T.val = T1.val / F.val     (erro semântico se F.val = 0)
    T -> F         T.val = F.val
    F -> ( E )     F.val = E.val
    F -> num       F.val = num.lexval

Um parser descendente não executa recursão à esquerda, então cada nível vira
um laço (`E -> T { (+|-) T }`). O laço reduz da esquerda para a direita, o que
dá exatamente a associatividade à esquerda da gramática original
(18 / 3 / 2 = (18 / 3) / 2 = 3).

Aritmética inteira de 64 bits, igual à versão em C: divisão truncada em
direção a zero, e estouro é erro semântico em vez de resultado errado.

Uso:  python calculadora.py [--trace] <arquivo | ->
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "comum"))
import fluxo as F  # noqa: E402

MIN, MAX = -(2**63), 2**63 - 1


class Calculadora:
    def __init__(self, fl: F.Fluxo, trace: bool):
        self.fl = fl
        self.trace = trace
        self.erros: list[str] = []  # erros semânticos (a análise continua)

    def _regra(self, producao: str, texto: str):
        if self.trace:
            print(f"{producao:<14}{texto}")

    def _opera(self, op_tok, a, b):
        """Ação semântica de E -> E op T / T -> T op F. None = valor indefinido."""
        if a is None or b is None:
            return None  # erro já reportado mais abaixo na árvore
        op = op_tok.lexeme
        if op == "/" and b == 0:
            self._erro(op_tok, f"divisão por zero ({a} / {b})")
            return None
        if op == "+":
            r = a + b
        elif op == "-":
            r = a - b
        elif op == "*":
            r = a * b
        else:
            q = abs(a) // abs(b)  # truncamento em direção a zero, como em C
            r = -q if (a < 0) != (b < 0) else q
        if not MIN <= r <= MAX:
            self._erro(op_tok, f"estouro de inteiro de 64 bits ({a} {op} {b})")
            return None
        return r

    def _erro(self, tok, mensagem: str):
        self.erros.append(F.erro_semantico(tok.line, tok.column, mensagem))

    @staticmethod
    def _mostra(v):
        return "indefinido" if v is None else str(v)

    def parse_E(self):
        val = self.parse_T()
        self._regra("E -> T", f"E.val = T.val = {self._mostra(val)}")
        while self.fl.verifica("PLUS", "MINUS"):
            op = self.fl.proximo()
            t = self.parse_T()
            novo = self._opera(op, val, t)
            self._regra(
                f"E -> E {op.lexeme} T",
                f"E.val = E1.val {op.lexeme} T.val = "
                f"{self._mostra(val)} {op.lexeme} {self._mostra(t)} = {self._mostra(novo)}",
            )
            val = novo
        return val

    def parse_T(self):
        val = self.parse_F()
        self._regra("T -> F", f"T.val = F.val = {self._mostra(val)}")
        while self.fl.verifica("STAR", "SLASH"):
            op = self.fl.proximo()
            f = self.parse_F()
            novo = self._opera(op, val, f)
            self._regra(
                f"T -> T {op.lexeme} F",
                f"T.val = T1.val {op.lexeme} F.val = "
                f"{self._mostra(val)} {op.lexeme} {self._mostra(f)} = {self._mostra(novo)}",
            )
            val = novo
        return val

    def parse_F(self):
        if self.fl.aceita("LPAREN"):
            val = self.parse_E()
            self.fl.exige("RPAREN", "')'")
            self._regra("F -> ( E )", f"F.val = E.val = {self._mostra(val)}")
            return val
        tok = self.fl.atual()
        if tok.type != "INT":
            raise F.ErroSintaxe(tok, "número ou '('")
        self.fl.proximo()
        val = int(tok.lexeme)
        if val > MAX:
            self._erro(tok, f"número {tok.lexeme} fora do intervalo de 64 bits")
            val = None
        self._regra("F -> num", f"F.val = num.lexval = {self._mostra(val)}")
        return val


def main(argv: list[str]) -> int:
    F.configurar_saida()
    trace = "--trace" in argv[1:]
    args = [a for a in argv[1:] if a != "--trace"]
    if len(args) != 1:
        print("uso: python calculadora.py [--trace] <arquivo | ->", file=sys.stderr)
        return F.ERRO_USO

    fl, codigo = F.abrir(args[0])
    if fl is None:
        return codigo

    calc = Calculadora(fl, trace)
    try:
        val = calc.parse_E()
        if not fl.verifica("EOF"):
            raise F.ErroSintaxe(fl.atual(), "operador ou fim da entrada")
    except F.ErroSintaxe as erro:
        # Erro de sintaxe prevalece: sem árvore válida, os valores calculados
        # até aqui não significam nada.
        print(erro, file=sys.stderr)
        return F.ERRO_SINTAXE

    if calc.erros:
        for e in calc.erros:
            print(e, file=sys.stderr)
        return F.ERRO_SEMANTICO
    print(f"resultado = {val}")
    return F.OK


if __name__ == "__main__":
    sys.exit(main(sys.argv))
