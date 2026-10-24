"""Interpretador da IR textual do MINIC — o oráculo semântico dos testes da IR.

    python tests/ir_interp.py programa.ir [entrada ...]

Executa o código de três endereços que `minic --ir` imprime (e também o dos
gabaritos de `ref/testes-oficiais/testes-ir-30/`, que usa o mesmo formato) e
devolve as linhas impressas por `print`. É assim que a suíte confere que:

- a IR -O0 faz o mesmo que a IR canônica do gabarito, mesmo quando o texto
  difere (rótulos, ordem de instruções independentes, forma de um `if`);
- a IR -O1 faz o mesmo que a -O0 — "a corretude deve ser demonstrada com
  testes antes e depois da otimização" (Seção 10.1 da especificação).

Semântica adotada (a mesma que o gerador supõe):

- `int` tem 32 bits, com transbordamento em complemento de dois; `/` trunca em
  direção a zero e `%` tem o sinal do dividendo, como em C;
- divisão por zero e passo demais (laço infinito) são erros de execução;
- variável lida antes de ser escrita vale 0, e vetor sem tamanho conhecido
  cresce sob demanda (o interpretador não confere limites);
- `read` consome a próxima entrada da lista (inteiros).

Não faz parte do compilador: é ferramenta de teste, escrita só em Python.
"""

import re
import sys

TOKEN = re.compile(r"'(?:\\.|[^'\\])*'|\"(?:\\.|[^\"\\])*\"|\S+")
ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", "'": "'", '"': '"'}
BINARIOS = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "and", "or"}
UNARIOS = {"neg", "not", "itof", "ctoi"}
LIMITE_PASSOS = 2_000_000


class ErroExecucao(Exception):
    pass


def _decodificar(corpo: str) -> str:
    saida, i = [], 0
    while i < len(corpo):
        if corpo[i] == "\\" and i + 1 < len(corpo):
            saida.append(ESCAPES.get(corpo[i + 1], corpo[i + 1]))
            i += 2
        else:
            saida.append(corpo[i])
            i += 1
    return "".join(saida)


def _int32(valor: int) -> int:
    return (valor + 2**31) % 2**32 - 2**31


def _formatar_float(x: float) -> str:
    """Mesmo formato do `print` da máquina virtual (src/python/vm.py)."""
    if x != x:
        return "nan"
    if x in (float("inf"), float("-inf")):
        return "inf" if x > 0 else "-inf"
    for precisao in range(1, 18):
        texto = "%.*g" % (precisao, x)
        if float(texto) == x:
            break
    if not any(c in texto for c in ".e"):
        texto += ".0"
    return texto


def _formatar(valor) -> str:
    if isinstance(valor, bool):
        return "true" if valor else "false"
    if isinstance(valor, float):
        return _formatar_float(valor)
    if isinstance(valor, dict):
        raise ErroExecucao("print de um vetor inteiro")
    return str(valor)


class Programa:
    """IR já separada em funções: nome -> (parâmetros, instruções, rótulos)."""

    def __init__(self, texto: str):
        self.globais: dict = {}
        self.funcoes: dict = {}
        atual = None
        for bruta in texto.splitlines():
            linha = bruta.strip()
            if not linha:
                continue
            if linha.startswith("global "):
                nome = linha.split()[1]
                if "[" in nome:
                    self.globais[nome[: nome.index("[")]] = {}
                else:
                    self.globais[nome] = 0
            elif linha.startswith("func "):
                cabecalho = linha[5:].strip()
                if "(" in cabecalho:
                    nome = cabecalho[: cabecalho.index("(")]
                    params = [p.strip() for p in cabecalho[cabecalho.index("(") + 1 : -1].split(",")]
                    params = [p for p in params if p]
                else:
                    nome, params = cabecalho, []
                atual = (params, [], {})
                self.funcoes[nome] = atual
            elif linha == "endfunc":
                atual = None
            elif atual is None:
                raise ErroExecucao(f"instrução fora de função: {linha}")
            elif linha.endswith(":") and " " not in linha:
                atual[2][linha[:-1]] = len(atual[1])
            else:
                atual[1].append(TOKEN.findall(linha))


class Interpretador:
    def __init__(self, programa: Programa, entradas=()):
        self.p = programa
        self.globais = dict(programa.globais)
        self.entradas = list(entradas)
        self.saida: list[str] = []
        self.passos = 0

    def executar(self) -> list[str]:
        if "@init" in self.p.funcoes:
            self._chamar("@init", [])
        if "main" not in self.p.funcoes:
            raise ErroExecucao("programa sem main")
        self._chamar("main", [])
        return self.saida

    # -- valores ----------------------------------------------------------------

    def _valor(self, quadro: dict, operando: str):
        c = operando[0]
        if c == "'" or c == '"':
            return _decodificar(operando[1:-1])
        if operando == "true":
            return True
        if operando == "false":
            return False
        if c.isdigit() or (c == "-" and len(operando) > 1):
            return float(operando) if "." in operando else _int32(int(operando))
        if operando in quadro:
            return quadro[operando]
        if operando in self.globais:
            return self.globais[operando]
        return 0

    def _escrever(self, quadro: dict, nome: str, valor):
        if nome not in quadro and nome in self.globais:
            self.globais[nome] = valor
        else:
            quadro[nome] = valor

    def _vetor(self, quadro: dict, nome: str) -> dict:
        if nome in quadro and isinstance(quadro[nome], dict):
            return quadro[nome]
        if nome in self.globais and isinstance(self.globais[nome], dict):
            return self.globais[nome]
        quadro[nome] = {}
        return quadro[nome]

    # -- operações --------------------------------------------------------------

    @staticmethod
    def _binario(op: str, x, y):
        if op in ("and", "or"):
            return (x and y) if op == "and" else (x or y)
        if op == "==":
            return x == y
        if op == "!=":
            return x != y
        if op == "<":
            return x < y
        if op == "<=":
            return x <= y
        if op == ">":
            return x > y
        if op == ">=":
            return x >= y
        inteiros = isinstance(x, int) and isinstance(y, int)
        if op in ("/", "%") and y == 0:
            raise ErroExecucao("divisão por zero")
        if op == "+":
            r = x + y
        elif op == "-":
            r = x - y
        elif op == "*":
            r = x * y
        elif op == "/":
            if not inteiros:
                return x / y
            q = abs(x) // abs(y)
            r = -q if (x < 0) != (y < 0) else q
        else:  # %
            q = abs(x) // abs(y)
            q = -q if (x < 0) != (y < 0) else q
            r = x - q * y
        return _int32(r) if inteiros else float(r)

    @staticmethod
    def _unario(op: str, x):
        if op == "neg":
            return _int32(-x) if isinstance(x, int) else -x
        if op == "not":
            return not x
        if op == "itof":
            return float(x)
        return ord(x)  # ctoi

    # -- execução -----------------------------------------------------------------

    def _chamar(self, nome: str, argumentos: list):
        if nome not in self.p.funcoes:
            raise ErroExecucao(f"função inexistente: {nome}")
        params, instrs, rotulos = self.p.funcoes[nome]
        if len(params) != len(argumentos):
            raise ErroExecucao(f"{nome}: esperava {len(params)} argumentos")
        quadro = dict(zip(params, argumentos))
        pendentes = []
        pc = 0
        while pc < len(instrs):
            self.passos += 1
            if self.passos > LIMITE_PASSOS:
                raise ErroExecucao("limite de passos excedido (laço infinito?)")
            t = instrs[pc]
            pc += 1
            k = t[0]
            if k == "print":
                self.saida.append(_formatar(self._valor(quadro, " ".join(t[1:]))))
            elif k == "read":
                if not self.entradas:
                    raise ErroExecucao("read sem entrada")
                self._escrever(quadro, t[1], self.entradas.pop(0))
            elif k == "param":
                if len(t) > 1:  # o gabarito O10 tem um `param` sem operando
                    nome_arg = t[1]
                    desconhecido = nome_arg not in quadro and nome_arg not in self.globais
                    if nome_arg[0].isalpha() and nome_arg not in ("true", "false") and desconhecido:
                        # nome nunca escrito: é um vetor local ainda vazio
                        pendentes.append(self._vetor(quadro, nome_arg))
                    else:
                        pendentes.append(self._valor(quadro, nome_arg))
            elif k == "call":
                self._fazer_chamada(t[1].rstrip(","), int(t[2]), pendentes)
            elif k == "return":
                return self._valor(quadro, t[1]) if len(t) > 1 else None
            elif k == "goto":
                pc = rotulos[t[1]]
            elif k in ("ifFalse", "ifTrue"):
                cond = self._valor(quadro, t[1])
                if bool(cond) == (k == "ifTrue"):
                    pc = rotulos[t[3]]
            elif k == "store":
                vetor, indice = self._acesso(quadro, t[1].rstrip(","))
                vetor[indice] = self._valor(quadro, t[2])
            elif len(t) >= 3 and t[1] == "=":
                self._escrever(quadro, t[0], self._expressao(quadro, t[2:], pendentes))
            else:
                raise ErroExecucao(f"instrução desconhecida: {' '.join(t)}")
        return None

    def _acesso(self, quadro: dict, texto: str):
        nome, resto = texto.split("[", 1)
        indice = self._valor(quadro, resto.rstrip("]"))
        return self._vetor(quadro, nome), indice

    def _expressao(self, quadro: dict, t: list, pendentes: list):
        if t[0] == "call":
            return self._fazer_chamada(t[1].rstrip(","), int(t[2]), pendentes)
        if t[0] == "load":
            vetor, indice = self._acesso(quadro, t[1])
            return vetor.get(indice, 0)
        if len(t) == 2 and t[0] in UNARIOS:
            return self._unario(t[0], self._valor(quadro, t[1]))
        if len(t) == 3 and t[1] in BINARIOS:
            return self._binario(t[1], self._valor(quadro, t[0]), self._valor(quadro, t[2]))
        if len(t) == 1:
            return self._valor(quadro, t[0])
        raise ErroExecucao(f"expressão desconhecida: {' '.join(t)}")

    def _fazer_chamada(self, nome: str, n: int, pendentes: list):
        argumentos = pendentes[len(pendentes) - n :] if n else []
        del pendentes[len(pendentes) - n :]
        return self._chamar(nome, argumentos)


def executar(texto: str, entradas=()) -> list[str]:
    """Linhas impressas pelo programa em IR (levanta ErroExecucao em erro)."""
    return Interpretador(Programa(texto), entradas).executar()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__.strip())
        sys.exit(1)
    with open(sys.argv[1], encoding="utf-8") as arquivo:
        linhas = executar(arquivo.read(), [int(x) for x in sys.argv[2:]])
    print("\n".join(linhas))
