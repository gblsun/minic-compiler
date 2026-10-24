"""Back-end do MINIC: IR de três endereços → código da máquina de pilha (MVM).

    python minic.py --target=asm codigo.c      # imprime o código gerado
    python minic.py --run codigo.c             # gera e executa (vm.py)
    python minic.py --symbols codigo.c         # tabela de símbolos com deslocamentos

O alvo escolhido (Seção 10 da especificação) é uma **máquina virtual de pilha
própria**, a MVM, executada por `vm.py` (e por `vm.c` na versão em C). Uma
máquina de pilha é o alvo didático da aula 17: cada instrução de três
endereços vira "empilha os operandos, opera, desempilha no destino".

Modelo de execução (aula 17):

- **Registro de ativação.** Cada chamada ganha um quadro com uma posição
  (*slot*) por nome da função: primeiro os parâmetros, na ordem; depois as
  variáveis locais, na ordem de declaração; por fim os temporários, na ordem
  em que aparecem. O deslocamento de cada nome no quadro é o índice do slot,
  e é o que o `--symbols` mostra. Vetores locais são alocados no quadro na
  entrada da função e liberados no retorno.
- **Convenção de chamada.** O chamador empilha os argumentos da esquerda para
  a direita (`param`) e executa `CALL f n`; a máquina tira os `n` valores da
  pilha e os coloca nos primeiros slots do quadro novo. Vetor é passado por
  referência. Toda função devolve exatamente um valor na pilha (`RET`); uma
  função `void` devolve 0, que o chamador descarta (`POP`).
- **Globais** ficam numa área estática, acessada pelo nome (`GLOAD`/`GSTORE`).
  A função `@init` da IR, se existir, roda antes de `main`.

Instruções da MVM:

    PUSH lit            empilha um literal (int, float, bool, char ou cadeia)
    LOAD n / STORE n    lê/escreve o slot n do quadro atual
    GLOAD g / GSTORE g  lê/escreve a global g
    ALOAD n / ASTORE n  vetor no slot n: ALOAD tira o índice; ASTORE tira valor e índice
    GALOAD g / GASTORE g  idem, vetor global
    ADD SUB MUL DIV MOD NEG          aritmética (int de 32 bits ou float)
    LT LE GT GE EQ NE AND OR NOT     comparação e lógica
    ITOF CTOI                        conversões
    JMP L / JZ L / JNZ L             desvio; JZ/JNZ tiram a condição da pilha
    CALL f n / RET / POP
    PRINT / READ tipo
"""

from dataclasses import dataclass, field

from ir import eh_literal

OPERADORES = {
    "+": "ADD",
    "-": "SUB",
    "*": "MUL",
    "/": "DIV",
    "%": "MOD",
    "<": "LT",
    "<=": "LE",
    ">": "GT",
    ">=": "GE",
    "==": "EQ",
    "!=": "NE",
    "and": "AND",
    "or": "OR",
}
UNARIOS = {"neg": "NEG", "not": "NOT", "itof": "ITOF", "ctoi": "CTOI"}


class ErroGeracao(Exception):
    """Erro na geração de código (código de saída 5)."""


@dataclass
class InstrVM:
    op: str
    arg: str | None = None
    n: int = 0  # CALL: quantidade de argumentos
    comentario: str | None = None  # nome do slot, só para leitura

    def texto(self) -> str:
        if self.op == "LABEL":
            return f"{self.arg}:"
        linha = f"  {self.op}"
        if self.arg is not None:
            linha += f" {self.arg}"
        if self.op == "CALL":
            linha += f" {self.n}"
        if self.comentario is not None:
            linha += f"  ; {self.comentario}"
        return linha


@dataclass
class Slot:
    indice: int
    nome: str  # nome na IR
    tipo: str | None  # tipo base; None para temporário
    tamanho: int | None  # vetor local: tamanho; senão None
    vetor_param: bool = False

    def texto(self) -> str:
        if self.tipo is None:
            return f".slot {self.indice} {self.nome}"
        if self.tamanho is not None:
            return f".slot {self.indice} {self.nome} {self.tipo}[{self.tamanho}]"
        if self.vetor_param:
            return f".slot {self.indice} {self.nome} {self.tipo}[]"
        return f".slot {self.indice} {self.nome} {self.tipo}"


@dataclass
class FuncaoVM:
    nome: str
    nparams: int
    slots: list = field(default_factory=list)
    codigo: list = field(default_factory=list)


@dataclass
class ProgramaVM:
    globais: list = field(default_factory=list)  # (nome, tipo, tamanho ou None)
    funcoes: list = field(default_factory=list)


def formatar(programa: ProgramaVM) -> str:
    linhas = ["; MINIC — código para a máquina de pilha (MVM)"]
    for nome, tipo, tamanho in programa.globais:
        sufixo = "" if tamanho is None else f"[{tamanho}]"
        linhas.append(f".global {nome} {tipo}{sufixo}")
    for funcao in programa.funcoes:
        linhas.append(f".func {funcao.nome} {funcao.nparams}")
        linhas.extend(slot.texto() for slot in funcao.slots)
        linhas.extend(ins.texto() for ins in funcao.codigo)
        linhas.append(".endfunc")
    return "".join(linha + "\n" for linha in linhas)


# -- registro de ativação ------------------------------------------------------------


def _usos_e_definicoes(ins) -> list:
    """Nomes que a instrução lê ou escreve, na ordem do texto (sem função/rótulo)."""
    k = ins.kind
    if k == "call":
        return [ins.dst] if ins.dst else []
    if k in ("goto", "label"):
        return []
    return [x for x in (ins.dst, ins.a, ins.b, ins.c) if x is not None]


def quadro(funcao, globais: set) -> list:
    """Slots do registro de ativação: parâmetros, locais declarados, temporários."""
    slots, vistos = [], set()
    tipos_param = {s.nome_ir: s for s in funcao.simbolos}
    for nome in funcao.params:
        simbolo = tipos_param[nome]
        slots.append(Slot(len(slots), nome, simbolo.tipo, None, simbolo.categoria == "parâmetro vetor"))
        vistos.add(nome)
    for simbolo in funcao.simbolos:
        if simbolo.nome_ir in vistos:
            continue
        tamanho = int(simbolo.tamanho) if simbolo.tamanho is not None else None
        slots.append(Slot(len(slots), simbolo.nome_ir, simbolo.tipo, tamanho))
        vistos.add(simbolo.nome_ir)
    for ins in funcao.instrs:
        for nome in _usos_e_definicoes(ins):
            if nome in vistos or nome in globais or eh_literal(nome):
                continue
            slots.append(Slot(len(slots), nome, funcao.tipos.get(nome), None))
            vistos.add(nome)
    return slots


# -- tradução ------------------------------------------------------------------------


class _Gerador:
    def __init__(self, programa_ir):
        self.ir = programa_ir
        self.globais = programa_ir.nomes_globais()

    def gerar(self) -> ProgramaVM:
        if not any(f.nome == "main" for f in self.ir.funcoes):
            raise ErroGeracao("o programa não define a função main.")
        saida = ProgramaVM()
        for nome, tamanho in self.ir.globais:
            saida.globais.append((nome, self.ir.tipos[nome], tamanho))
        for funcao in self.ir.funcoes:
            saida.funcoes.append(self._funcao(funcao))
        return saida

    def _funcao(self, funcao) -> FuncaoVM:
        slots = quadro(funcao, self.globais)
        self.posicao = {s.nome: s.indice for s in slots}
        self.tipos = funcao.tipos
        self.codigo = []
        for ins in funcao.instrs:
            self._instr(ins)
        if not self.codigo or self.codigo[-1].op != "RET":
            self._emitir("PUSH", "0")  # fim sem return: função void
            self._emitir("RET")
        return FuncaoVM(funcao.nome, len(funcao.params), slots, self.codigo)

    def _emitir(self, op, arg=None, n=0, comentario=None):
        self.codigo.append(InstrVM(op, arg, n, comentario))

    def _empilhar(self, operando: str):
        if eh_literal(operando):
            self._emitir("PUSH", operando)
        elif operando in self.posicao:
            self._emitir("LOAD", str(self.posicao[operando]), comentario=operando)
        else:
            self._emitir("GLOAD", operando)

    def _guardar(self, nome: str):
        if nome in self.posicao:
            self._emitir("STORE", str(self.posicao[nome]), comentario=nome)
        else:
            self._emitir("GSTORE", nome)

    def _vetor(self, op: str, nome: str):
        """ALOAD/ASTORE no vetor local (slot) ou global (nome)."""
        if nome in self.posicao:
            self._emitir(op, str(self.posicao[nome]), comentario=nome)
        else:
            self._emitir("G" + op, nome)

    def _instr(self, ins):
        k = ins.kind
        if k == "copy":
            self._empilhar(ins.a)
            self._guardar(ins.dst)
        elif k == "bin":
            self._empilhar(ins.a)
            self._empilhar(ins.b)
            self._emitir(OPERADORES[ins.op])
            self._guardar(ins.dst)
        elif k == "un":
            self._empilhar(ins.a)
            self._emitir(UNARIOS[ins.op])
            self._guardar(ins.dst)
        elif k == "load":
            self._empilhar(ins.b)
            self._vetor("ALOAD", ins.a)
            self._guardar(ins.dst)
        elif k == "store":
            self._empilhar(ins.b)
            self._empilhar(ins.c)
            self._vetor("ASTORE", ins.a)
        elif k == "print":
            self._empilhar(ins.a)
            self._emitir("PRINT")
        elif k == "read":
            tipo = self.tipos.get(ins.dst) or self.ir.tipos.get(ins.dst)
            if tipo is None:
                raise ErroGeracao(f"tipo desconhecido para read de “{ins.dst}”.")
            self._emitir("READ", tipo)
            self._guardar(ins.dst)
        elif k == "param":
            self._empilhar(ins.a)
        elif k == "call":
            self._emitir("CALL", ins.a, ins.n)
            if ins.dst is None:
                self._emitir("POP")
            else:
                self._guardar(ins.dst)
        elif k == "ret":
            self._empilhar(ins.a if ins.a is not None else "0")
            self._emitir("RET")
        elif k == "goto":
            self._emitir("JMP", ins.label)
        elif k in ("iff", "ift"):
            self._empilhar(ins.a)
            self._emitir("JZ" if k == "iff" else "JNZ", ins.label)
        elif k == "label":
            self._emitir("LABEL", ins.label)
        else:
            raise ErroGeracao(f"instrução de IR desconhecida: {k}")


def gerar(programa_ir) -> ProgramaVM:
    return _Gerador(programa_ir).gerar()


# -- tabela de símbolos (--symbols) ---------------------------------------------------


def _usados(programa_ir) -> set:
    """(função, nome) de cada nome lido em algum lugar da IR (e globais lidas)."""
    usados = set()
    for funcao in programa_ir.funcoes:
        for ins in funcao.instrs:
            k = ins.kind
            lidos = []
            if k in ("copy", "un", "print", "param", "ret", "iff", "ift"):
                lidos = [ins.a]
            elif k == "bin":
                lidos = [ins.a, ins.b]
            elif k == "load":
                lidos = [ins.a, ins.b]
            elif k == "store":
                lidos = [ins.a, ins.b, ins.c]  # escrever no elemento usa o vetor
            elif k == "call":
                usados.add(("", ins.a))  # função chamada
            for nome in lidos:
                if nome is not None and not eh_literal(nome):
                    usados.add((funcao.nome, nome))
                    usados.add(("", nome))
    return usados


def _tipo_texto(simbolo) -> str:
    if simbolo.categoria == "vetor":
        return f"{simbolo.tipo}[{simbolo.tamanho}]"
    if simbolo.categoria == "parâmetro vetor":
        return f"{simbolo.tipo}[]"
    return simbolo.tipo


def _estado(simbolo, usado: bool) -> str:
    partes = []
    if simbolo.categoria != "função":
        partes.append("inicializada" if simbolo.inicializada else "não inicializada")
    partes.append("usada" if usado else "não usada")
    return ", ".join(partes)


def simbolos(programa_ir) -> str:
    """Tabela de símbolos (Seção 6.1): nome, categoria, tipo, nível, posição,
    estado de inicialização e uso, e o deslocamento no registro de ativação."""
    globais = programa_ir.nomes_globais()
    usados = _usados(programa_ir)
    linhas = ["escopo global (nível 0):"]
    for s in programa_ir.simbolos:
        nome = s.nome if s.nome == s.nome_ir else f"{s.nome} ({s.nome_ir})"
        usado = ("", s.nome_ir) in usados
        linhas.append(
            f"  {nome}: {s.categoria} {_tipo_texto(s)} — linha {s.linha}, coluna {s.coluna}; "
            f"{_estado(s, usado)}"
        )
    for funcao in programa_ir.funcoes:
        slots = quadro(funcao, globais)
        por_nome = {s.nome_ir: s for s in funcao.simbolos}
        linhas.append(f"função {funcao.nome} (registro de ativação: {len(slots)} posições):")
        for slot in slots:
            s = por_nome.get(slot.nome)
            if s is None:
                linhas.append(f"  {slot.nome}: temporário — deslocamento {slot.indice}")
                continue
            nome = s.nome if s.nome == s.nome_ir else f"{s.nome} ({s.nome_ir})"
            usado = (funcao.nome, s.nome_ir) in usados
            linhas.append(
                f"  {nome}: {s.categoria} {_tipo_texto(s)}, nível {s.nivel} — linha {s.linha}, "
                f"coluna {s.coluna}; {_estado(s, usado)}; deslocamento {slot.indice}"
            )
    return "".join(linha + "\n" for linha in linhas)
