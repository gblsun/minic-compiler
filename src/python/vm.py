"""Máquina virtual de pilha do MINIC (MVM): executa o código de `geracao.py`.

    python minic.py --run codigo.c < entrada.txt
    python minic.py --run -O1 --stats codigo.c     # + instruções executadas

Semântica (a mesma da versão em C, `vm.c`, byte a byte):

- `int` tem 32 bits: soma, subtração, multiplicação e negação transbordam em
  complemento de dois; `/` trunca em direção a zero e `%` tem o sinal do
  dividendo, como em C. `INT_MIN / -1` dá `INT_MIN` (e `INT_MIN % -1`, 0) em vez
  do comportamento indefinido do C.
- Divisão ou resto por zero (int ou float), índice fora do vetor, entrada
  inválida em `read` e recursão além de 10000 chamadas são **erros de
  execução**: mensagem em stderr e código de saída 6 (Seção 11.1).
- Variável não inicializada vale 0 (0.0, false, o caractere nulo).
- `print` escreve o valor e uma quebra de linha. Um `float` sai com o menor
  número de dígitos que o representa de volta (`%g` com precisão crescente),
  e com `.0` se ficar sem ponto: `0.5`, `2.0`, `1e+20`.
- `read` lê da entrada padrão o próximo item separado por espaço: `int`
  (`-?[0-9]+`, dentro dos 32 bits), `float` (`-?[0-9]+` com parte decimal
  opcional), `bool` (`true`/`false`) ou `char` (exatamente um caractere).
"""

import re
import sys

INT_MIN = -(2**31)
PROFUNDIDADE_MAXIMA = 10000
ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", "'": "'", '"': '"'}
LEITURA_INT = re.compile(rb"-?[0-9]+")
LEITURA_FLOAT = re.compile(rb"-?[0-9]+(\.[0-9]+)?")


class ErroExecucao(Exception):
    """Erro durante a execução (código de saída 6)."""


def int32(valor: int) -> int:
    return (valor + 2**31) % 2**32 - 2**31


def formatar_float(x: float) -> str:
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


def formatar(valor) -> str:
    if isinstance(valor, bool):
        return "true" if valor else "false"
    if isinstance(valor, float):
        return formatar_float(valor)
    return str(valor)


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


def literal(texto: str):
    """Valor de um literal da IR (o mesmo texto que aparece no código-fonte)."""
    c = texto[0]
    if c in "'\"":
        return _decodificar(texto[1:-1])
    if texto == "true":
        return True
    if texto == "false":
        return False
    if "." in texto:
        return float(texto)
    return int32(int(texto))


def valor_inicial(tipo):
    if tipo == "float":
        return 0.0
    if tipo == "bool":
        return False
    if tipo == "char":
        return "\0"
    return 0


class Vetor:
    def __init__(self, nome: str, tipo: str, tamanho: int):
        self.nome = nome.split("#")[0]  # nome como no código, para as mensagens
        self.itens = [valor_inicial(tipo)] * tamanho


# Códigos de operação: a VM decodifica o texto uma vez, antes de executar.
(PUSH, LOAD, STORE, GLOAD, GSTORE, ALOAD, ASTORE, GALOAD, GASTORE, ADD, SUB, MUL, DIV, MOD,
 NEG, LT, LE, GT, GE, EQ, NE, AND, OR, NOT, ITOF, CTOI, JMP, JZ, JNZ, CALL, RET, POP,
 PRINT, READ) = range(34)
CODIGOS = {nome: i for i, nome in enumerate(
    "PUSH LOAD STORE GLOAD GSTORE ALOAD ASTORE GALOAD GASTORE ADD SUB MUL DIV MOD NEG LT LE "
    "GT GE EQ NE AND OR NOT ITOF CTOI JMP JZ JNZ CALL RET POP PRINT READ".split())}


class _Funcao:
    def __init__(self, funcao):
        self.nome = funcao.nome
        self.nparams = funcao.nparams
        self.slots = funcao.slots
        rotulos, codigo = {}, []
        for ins in funcao.codigo:
            if ins.op == "LABEL":
                rotulos[ins.arg] = len(codigo)
            else:
                codigo.append(ins)
        self.codigo = []
        for ins in codigo:
            op = CODIGOS[ins.op]
            if op == PUSH:
                arg = literal(ins.arg)
            elif op in (LOAD, STORE, ALOAD, ASTORE):
                arg = int(ins.arg)
            elif op in (JMP, JZ, JNZ):
                arg = rotulos[ins.arg]
            elif op == CALL:
                arg = (ins.arg, ins.n)
            else:
                arg = ins.arg
            self.codigo.append((op, arg))


class _Entrada:
    def __init__(self, fluxo):
        self.fluxo = fluxo
        self.itens = None

    def proximo(self, tipo: str):
        if self.itens is None:
            self.itens = self.fluxo.read().split()
            self.itens.reverse()
        if not self.itens:
            raise ErroExecucao(f"entrada esgotada: read de {tipo} sem valor.")
        item = self.itens.pop()
        texto = item.decode("utf-8", "surrogateescape")
        if tipo == "int" and LEITURA_INT.fullmatch(item):
            valor = int(item)
            if INT_MIN <= valor <= 2**31 - 1:
                return valor
        elif tipo == "float" and LEITURA_FLOAT.fullmatch(item):
            return float(item)
        elif tipo == "bool" and item in (b"true", b"false"):
            return item == b"true"
        elif tipo == "char" and len(texto) == 1:
            return texto
        raise ErroExecucao(f"entrada “{texto}” inválida para read de {tipo}.")


class MaquinaVirtual:
    def __init__(self, programa, entrada=None, saida=None):
        self.funcoes = {f.nome: _Funcao(f) for f in programa.funcoes}
        self.globais = {}
        for nome, tipo, tamanho in programa.globais:
            if tamanho is None:
                self.globais[nome] = valor_inicial(tipo)
            else:
                self.globais[nome] = Vetor(nome, tipo, int(tamanho))
        self.entrada = _Entrada(entrada if entrada is not None else sys.stdin.buffer)
        self.saida = saida if saida is not None else sys.stdout.buffer
        self.executadas = 0

    def executar(self):
        if "@init" in self.funcoes:
            self._rodar("@init")
        self._rodar("main")

    def _quadro(self, funcao, argumentos):
        slots = []
        for slot in funcao.slots:
            if slot.tamanho is not None:
                slots.append(Vetor(slot.nome, slot.tipo, slot.tamanho))
            else:
                slots.append(valor_inicial(slot.tipo))
        slots[: len(argumentos)] = argumentos
        return slots

    def _rodar(self, nome: str):
        funcao = self.funcoes[nome]
        pilha = []
        quadros = []  # (função, slots, pc) dos chamadores
        slots = self._quadro(funcao, [])
        codigo = funcao.codigo
        pc = 0
        globais = self.globais
        escrever = self.saida.write
        executadas = 0
        try:
            while True:
                op, arg = codigo[pc]
                pc += 1
                executadas += 1
                if op == LOAD:
                    pilha.append(slots[arg])
                elif op == STORE:
                    slots[arg] = pilha.pop()
                elif op == PUSH:
                    pilha.append(arg)
                elif op == GLOAD:
                    pilha.append(globais[arg])
                elif op == GSTORE:
                    globais[arg] = pilha.pop()
                elif op in (ALOAD, GALOAD):
                    vetor = slots[arg] if op == ALOAD else globais[arg]
                    indice = pilha.pop()
                    self._conferir(vetor, indice)
                    pilha.append(vetor.itens[indice])
                elif op in (ASTORE, GASTORE):
                    vetor = slots[arg] if op == ASTORE else globais[arg]
                    valor = pilha.pop()
                    indice = pilha.pop()
                    self._conferir(vetor, indice)
                    vetor.itens[indice] = valor
                elif op <= MOD:
                    b = pilha.pop()
                    a = pilha.pop()
                    pilha.append(_aritmetica(op, a, b))
                elif op == NEG:
                    a = pilha.pop()
                    pilha.append(-a if isinstance(a, float) else int32(-a))
                elif op <= NE:  # LT .. NE
                    b = pilha.pop()
                    a = pilha.pop()
                    if op == LT:
                        pilha.append(a < b)
                    elif op == LE:
                        pilha.append(a <= b)
                    elif op == GT:
                        pilha.append(a > b)
                    elif op == GE:
                        pilha.append(a >= b)
                    elif op == EQ:
                        pilha.append(a == b)
                    else:
                        pilha.append(a != b)
                elif op == AND:
                    b = pilha.pop()
                    pilha.append(pilha.pop() and b)
                elif op == OR:
                    b = pilha.pop()
                    pilha.append(pilha.pop() or b)
                elif op == NOT:
                    pilha.append(not pilha.pop())
                elif op == ITOF:
                    pilha.append(float(pilha.pop()))
                elif op == CTOI:
                    pilha.append(ord(pilha.pop()))
                elif op == JMP:
                    pc = arg
                elif op == JZ:
                    if not pilha.pop():
                        pc = arg
                elif op == JNZ:
                    if pilha.pop():
                        pc = arg
                elif op == CALL:
                    destino, n = arg
                    if len(quadros) + 1 >= PROFUNDIDADE_MAXIMA:
                        raise ErroExecucao(
                            f"estouro da pilha de chamadas (mais de {PROFUNDIDADE_MAXIMA} "
                            "chamadas aninhadas)."
                        )
                    argumentos = pilha[len(pilha) - n :] if n else []
                    del pilha[len(pilha) - n :]
                    quadros.append((funcao, slots, pc))
                    funcao = self.funcoes[destino]
                    slots = self._quadro(funcao, argumentos)
                    codigo = funcao.codigo
                    pc = 0
                elif op == RET:
                    if not quadros:
                        return
                    funcao, slots, pc = quadros.pop()
                    codigo = funcao.codigo
                elif op == POP:
                    pilha.pop()
                elif op == PRINT:
                    escrever((formatar(pilha.pop()) + "\n").encode("utf-8", "surrogateescape"))
                elif op == READ:
                    pilha.append(self.entrada.proximo(arg))
        except ErroExecucao as erro:
            erro.funcao = funcao.nome
            raise
        finally:
            self.executadas += executadas

    @staticmethod
    def _conferir(vetor, indice):
        if not 0 <= indice < len(vetor.itens):
            raise ErroExecucao(
                f"índice {indice} fora dos limites do vetor “{vetor.nome}” "
                f"(tamanho {len(vetor.itens)})."
            )


def _aritmetica(op, a, b):
    if isinstance(a, float) or isinstance(b, float):
        if op == ADD:
            return a + b
        if op == SUB:
            return a - b
        if op == MUL:
            return a * b
        if b == 0:
            raise ErroExecucao("divisão por zero.")
        return a / b
    if op == ADD:
        return int32(a + b)
    if op == SUB:
        return int32(a - b)
    if op == MUL:
        return int32(a * b)
    if b == 0:
        raise ErroExecucao("divisão por zero." if op == DIV else "resto de divisão por zero.")
    q = abs(a) // abs(b)
    if (a < 0) != (b < 0):
        q = -q
    return int32(q) if op == DIV else int32(a - q * b)


def executar(programa, entrada=None, saida=None) -> int:
    """Executa; devolve a quantidade de instruções executadas."""
    vm = MaquinaVirtual(programa, entrada, saida)
    vm.executar()
    return vm.executadas
