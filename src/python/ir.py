"""Representação intermediária (IR) do MINIC: código de três endereços.

    python minic.py --ir codigo.c          # IR sem otimização (-O0)
    python minic.py --ir -O1 codigo.c      # IR depois de otimizar

Este módulo traduz a AST **já aceita pela análise semântica** para código de
três endereços (Seção 9 da especificação, aulas 16 e 17): cada instrução tem no
máximo um resultado e dois operandos, e o fluxo de controle vira rótulos e
desvios. O formato textual segue o dos gabaritos de
`ref/testes-oficiais/testes-ir-30/`:

    func combinar(x, y)            store v[t1], 12
      t1 = x * 10                  t3 = load v[t2]
      t2 = t1 + y                  ifFalse t1 goto L_else
      return t2                    param 3
    endfunc                        t1 = call combinar, 2

Cada instrução é um `Instr`, com um campo `kind` que diz qual forma ela tem
(tabela em `Instr`). O otimizador (`otimizador.py`) trabalha sobre essa mesma
lista de objetos, e só no fim tudo vira texto, por `formatar`.

Decisões (detalhadas em docs/ir.md):

- **Nomes.** Variáveis mantêm o nome do código. Quando o mesmo nome é
  declarado mais de uma vez na função (sombreamento, como em P09), coincide
  com uma global ou tem a forma de um temporário (`t1`), cada declaração
  ganha um sufixo: `x#1`, `x#2`, na ordem do código. Temporários são `t1`,
  `t2`, ... e rótulos `L_else`, `L_end`, `L_test`, ... — os dois numerados
  por função, como nos gabaritos.
- **Conversões implícitas** (int → float e char → int, Seção 5) viram
  instruções explícitas, `itof` e `ctoi`. Sem isso, o otimizador não teria
  como saber que `7 / 2` numa variável float não é uma divisão inteira.
- **Vetores**: o endereço do elemento é `índice * 1` (cada elemento ocupa uma
  posição, como no gabarito P07), seguido de `load`/`store`.
- **`&&` e `||`** viram `and`/`or` explícitos (P08). Quando o operando da
  direita contém uma chamada — o único caso em que avaliá-lo ou não muda o
  comportamento —, a tradução usa curto-circuito, com desvio.
- **Globais** são declaradas no topo (`global g`, `global v[10]`), e o que
  for preciso para inicializá-las roda numa função própria, `@init` (nome
  que não pode colidir com uma função do usuário), antes de `main`.
"""

import re
from dataclasses import dataclass, field

import minic_ast as A

# Forma de um temporário. Uma variável do usuário com esse formato ganha
# sufixo, para não ser confundida com um temporário.
TEMPORARIO = re.compile(r"t[0-9]+")


@dataclass
class Instr:
    """Uma instrução de três endereços.

    kind    texto                          campos usados
    ------  -----------------------------  ------------------
    copy    dst = a                        dst, a
    bin     dst = a op b                   dst, a, op, b
    un      dst = op a                     dst, op, a   (neg, not, itof, ctoi)
    load    dst = load a[b]                dst, a, b
    store   store a[b], c                  a, b, c
    print   print a                        a
    read    read dst                       dst
    param   param a                        a
    call    dst = call a, n  /  call a, n  dst (opcional), a, n
    ret     return a  /  return            a (opcional)
    goto    goto label                     label
    iff     ifFalse a goto label           a, label
    ift     ifTrue a goto label            a, label
    label   label:                         label
    """

    kind: str
    dst: str | None = None
    a: str | None = None
    b: str | None = None
    c: str | None = None
    op: str | None = None
    label: str | None = None
    n: int = 0

    def texto(self) -> str:
        k = self.kind
        if k == "copy":
            return f"  {self.dst} = {self.a}"
        if k == "bin":
            return f"  {self.dst} = {self.a} {self.op} {self.b}"
        if k == "un":
            return f"  {self.dst} = {self.op} {self.a}"
        if k == "load":
            return f"  {self.dst} = load {self.a}[{self.b}]"
        if k == "store":
            return f"  store {self.a}[{self.b}], {self.c}"
        if k == "print":
            return f"  print {self.a}"
        if k == "read":
            return f"  read {self.dst}"
        if k == "param":
            return f"  param {self.a}"
        if k == "call":
            if self.dst is None:
                return f"  call {self.a}, {self.n}"
            return f"  {self.dst} = call {self.a}, {self.n}"
        if k == "ret":
            return "  return" if self.a is None else f"  return {self.a}"
        if k == "goto":
            return f"  goto {self.label}"
        if k == "iff":
            return f"  ifFalse {self.a} goto {self.label}"
        if k == "ift":
            return f"  ifTrue {self.a} goto {self.label}"
        if k == "label":
            return f"{self.label}:"
        raise ValueError(f"instrução desconhecida: {k}")


@dataclass
class Funcao:
    nome: str
    params: list = field(default_factory=list)
    instrs: list = field(default_factory=list)


@dataclass
class ProgramaIR:
    globais: list = field(default_factory=list)  # (nome, tamanho ou None)
    funcoes: list = field(default_factory=list)

    def nomes_globais(self) -> set:
        return {nome for nome, _ in self.globais}


def formatar(programa: ProgramaIR) -> str:
    """O programa inteiro como texto, uma instrução por linha (LF no fim)."""
    linhas = []
    for nome, tamanho in programa.globais:
        linhas.append(f"global {nome}" if tamanho is None else f"global {nome}[{tamanho}]")
    for funcao in programa.funcoes:
        if funcao.params:
            linhas.append(f"func {funcao.nome}({', '.join(funcao.params)})")
        else:
            linhas.append(f"func {funcao.nome}")
        linhas.extend(instr.texto() for instr in funcao.instrs)
        linhas.append("endfunc")
    return "".join(linha + "\n" for linha in linhas)


# -- operandos -----------------------------------------------------------------


def eh_literal(operando: str) -> bool:
    """Literal (número, caractere, cadeia, true/false) ou nome?"""
    c = operando[0]
    if c.isdigit() or c in "'\"":
        return True
    if c == "-" and len(operando) > 1 and operando[1].isdigit():
        return True
    return operando in ("true", "false")


# Tipo de cada categoria de literal (`real` é o nome do token de float).
TIPO_LITERAL = {"int": "int", "real": "float", "bool": "bool", "char": "char", "string": "string"}


def _contem_chamada(no) -> bool:
    if no is None:
        return False
    if isinstance(no, A.Call):
        return True
    if isinstance(no, A.Binary):
        return _contem_chamada(no.left) or _contem_chamada(no.right)
    if isinstance(no, A.Unary):
        return _contem_chamada(no.operand)
    if isinstance(no, A.Assign):
        return _contem_chamada(no.target) or _contem_chamada(no.value)
    if isinstance(no, A.Index):
        return _contem_chamada(no.base) or _contem_chamada(no.index)
    return False


# -- gerador -------------------------------------------------------------------


@dataclass
class _Variavel:
    nome_ir: str
    tipo: str
    eh_vetor: bool


@dataclass
class _Laco:
    saida: str  # destino de break
    continuacao: str | None  # destino de continue (no for, criado sob demanda)


class Gerador:
    def __init__(self, programa: A.Program):
        self.programa = programa
        self.ir = ProgramaIR()
        self.assinaturas = {}  # nome da função -> (retorno, [(tipo, eh_vetor)])
        self.globais = {}  # nome no código -> _Variavel
        self.escopos: list[dict] = []
        self.funcao: Funcao | None = None
        self.tipo_retorno = "void"
        self.temporarios = 0
        self.rotulos: dict[str, int] = {}
        self.sufixar: set = set()  # nomes que ganham sufixo nesta função
        self.sufixos: dict[str, int] = {}
        self.lacos: list[_Laco] = []

    # -- programa ------------------------------------------------------------

    def gerar(self) -> ProgramaIR:
        for item in self.programa.decls:
            if isinstance(item, A.Function):
                params = [(p.type, p.is_array) for p in item.params]
                self.assinaturas[item.name] = (item.return_type, params)

        init = Funcao("@init")
        self._entrar(init, "void", [])
        for item in self.programa.decls:
            if isinstance(item, A.VarDecl):
                nome = item.name + "#g" if TEMPORARIO.fullmatch(item.name) else item.name
                tamanho = None if item.size is None else item.size.text
                self.ir.globais.append((nome, tamanho))
                self.globais[item.name] = _Variavel(nome, item.type, item.size is not None)
                if item.init is not None:
                    self._atribuir_variavel(nome, item.type, item.init)
            elif not isinstance(item, A.Function):
                self._comando(item)  # comando solto no topo
        if init.instrs:
            self.ir.funcoes.append(init)

        for item in self.programa.decls:
            if isinstance(item, A.Function):
                self._funcao(item)
        return self.ir

    def _entrar(self, funcao: Funcao, tipo_retorno: str, declaracoes: list):
        self.funcao = funcao
        self.tipo_retorno = tipo_retorno
        self.temporarios = 0
        self.rotulos = {}
        self.sufixos = {}
        self.escopos = [{}]
        contagem: dict[str, int] = {}
        for nome in declaracoes:
            contagem[nome] = contagem.get(nome, 0) + 1
        self.sufixar = {
            nome
            for nome, n in contagem.items()
            if n > 1 or nome in self.globais or TEMPORARIO.fullmatch(nome)
        }

    def _funcao(self, no: A.Function):
        declaracoes = [p.name for p in no.params]
        _coletar_declaracoes(no.body, declaracoes)
        funcao = Funcao(no.name)
        self._entrar(funcao, no.return_type, declaracoes)
        for p in no.params:
            funcao.params.append(self._declarar(p.name, p.type, p.is_array))
        # O corpo divide o escopo com os parâmetros (como na análise semântica).
        for item in no.body.items:
            self._item(item)
        self.ir.funcoes.append(funcao)

    # -- auxiliares ----------------------------------------------------------

    def _emitir(self, kind: str, **campos):
        self.funcao.instrs.append(Instr(kind, **campos))

    def _temp(self) -> str:
        self.temporarios += 1
        return f"t{self.temporarios}"

    def _rotulo(self, base: str) -> str:
        n = self.rotulos.get(base, 0) + 1
        self.rotulos[base] = n
        return base if n == 1 else f"{base}{n}"

    def _declarar(self, nome: str, tipo: str, eh_vetor: bool) -> str:
        if nome in self.sufixar:
            n = self.sufixos.get(nome, 0) + 1
            self.sufixos[nome] = n
            nome_ir = f"{nome}#{n}"
        else:
            nome_ir = nome
        self.escopos[-1][nome] = _Variavel(nome_ir, tipo, eh_vetor)
        return nome_ir

    def _buscar(self, nome: str) -> _Variavel:
        for escopo in reversed(self.escopos):
            if nome in escopo:
                return escopo[nome]
        return self.globais[nome]

    def _converter(self, operando: str, de: str, para: str) -> str:
        """Conversão implícita explícita na IR: char → int e int → float."""
        if de == "char" and para in ("int", "float"):
            t = self._temp()
            self._emitir("un", dst=t, op="ctoi", a=operando)
            operando, de = t, "int"
        if de == "int" and para == "float":
            if eh_literal(operando):
                return operando + ".0"
            t = self._temp()
            self._emitir("un", dst=t, op="itof", a=operando)
            return t
        return operando

    def _atribuir_variavel(self, nome_ir: str, tipo: str, valor):
        operando, tipo_valor = self._expr(valor)
        operando = self._converter(operando, tipo_valor, tipo)
        self._emitir("copy", dst=nome_ir, a=operando)

    def _endereco(self, indice) -> str:
        operando, tipo = self._expr(indice)
        operando = self._converter(operando, tipo, "int")
        t = self._temp()
        self._emitir("bin", dst=t, a=operando, op="*", b="1")
        return t

    # -- comandos ------------------------------------------------------------

    def _item(self, item):
        if isinstance(item, A.VarDecl):
            nome_ir = self._declarar(item.name, item.type, item.size is not None)
            if item.init is not None:
                self._atribuir_variavel(nome_ir, item.type, item.init)
        else:
            self._comando(item)

    def _comando(self, no):
        if isinstance(no, A.Block):
            self.escopos.append({})
            for item in no.items:
                self._item(item)
            self.escopos.pop()
        elif isinstance(no, A.ExprStmt):
            if no.expr is not None:
                self._efeito(no.expr)
        elif isinstance(no, A.If):
            self._if(no)
        elif isinstance(no, A.While):
            teste, fim = self._rotulo("L_test"), self._rotulo("L_end")
            self._emitir("label", label=teste)
            cond, _ = self._expr(no.cond)
            self._emitir("iff", a=cond, label=fim)
            self.lacos.append(_Laco(fim, teste))
            self._comando(no.body)
            self.lacos.pop()
            self._emitir("goto", label=teste)
            self._emitir("label", label=fim)
        elif isinstance(no, A.For):
            self._for(no)
        elif isinstance(no, A.Return):
            if no.expr is None:
                self._emitir("ret")
            else:
                operando, tipo = self._expr(no.expr)
                self._emitir("ret", a=self._converter(operando, tipo, self.tipo_retorno))
        elif isinstance(no, A.Break):
            self._emitir("goto", label=self.lacos[-1].saida)
        elif isinstance(no, A.Continue):
            laco = self.lacos[-1]
            if laco.continuacao is None:
                laco.continuacao = self._rotulo("L_step")
            self._emitir("goto", label=laco.continuacao)
        elif isinstance(no, A.Print):
            operando, _ = self._expr(no.arg)
            self._emitir("print", a=operando)
        elif isinstance(no, A.Read):
            if isinstance(no.target, A.Index):
                endereco = self._endereco(no.target.index)
                t = self._temp()
                self._emitir("read", dst=t)
                vetor = self._buscar(no.target.base.name).nome_ir
                self._emitir("store", a=vetor, b=endereco, c=t)
            else:
                self._emitir("read", dst=self._buscar(no.target.name).nome_ir)
        else:
            raise TypeError(f"comando desconhecido: {type(no).__name__}")

    def _if(self, no: A.If):
        cond, _ = self._expr(no.cond)
        if no.otherwise is None:
            fim = self._rotulo("L_end_if")
            self._emitir("iff", a=cond, label=fim)
            self._comando(no.then)
            self._emitir("label", label=fim)
            return
        senao, fim = self._rotulo("L_else"), self._rotulo("L_end")
        self._emitir("iff", a=cond, label=senao)
        self._comando(no.then)
        self._emitir("goto", label=fim)
        self._emitir("label", label=senao)
        self._comando(no.otherwise)
        self._emitir("label", label=fim)

    def _for(self, no: A.For):
        if no.init is not None:
            self._efeito(no.init)
        teste, fim = self._rotulo("L_test"), self._rotulo("L_end")
        self._emitir("label", label=teste)
        if no.cond is not None:
            cond, _ = self._expr(no.cond)
            self._emitir("iff", a=cond, label=fim)
        laco = _Laco(fim, None)
        self.lacos.append(laco)
        self._comando(no.body)
        self.lacos.pop()
        if laco.continuacao is not None:
            self._emitir("label", label=laco.continuacao)
        if no.step is not None:
            self._efeito(no.step)
        self._emitir("goto", label=teste)
        self._emitir("label", label=fim)

    def _efeito(self, expr):
        """Expressão usada como comando: o valor é descartado."""
        if isinstance(expr, A.Call):
            self._chamada(expr, com_valor=False)
        else:
            self._expr(expr)

    # -- expressões ----------------------------------------------------------

    def _expr(self, no) -> tuple[str, str]:
        """Gera o código da expressão; devolve (operando com o valor, tipo)."""
        if isinstance(no, A.Lit):
            # O parser já guarda char e cadeia com aspas e escapes (`'\n'`).
            return no.text, TIPO_LITERAL[no.kind]
        if isinstance(no, A.Id):
            var = self._buscar(no.name)
            return var.nome_ir, var.tipo
        if isinstance(no, A.Index):
            endereco = self._endereco(no.index)
            var = self._buscar(no.base.name)
            t = self._temp()
            self._emitir("load", dst=t, a=var.nome_ir, b=endereco)
            return t, var.tipo
        if isinstance(no, A.Call):
            return self._chamada(no, com_valor=True)
        if isinstance(no, A.Unary):
            operando, tipo = self._expr(no.operand)
            if no.op == "!":
                t = self._temp()
                self._emitir("un", dst=t, op="not", a=operando)
                return t, "bool"
            # "-": char é promovido a int antes
            if tipo == "char":
                operando, tipo = self._converter(operando, "char", "int"), "int"
            t = self._temp()
            self._emitir("un", dst=t, op="neg", a=operando)
            return t, tipo
        if isinstance(no, A.Binary):
            return self._binario(no)
        if isinstance(no, A.Assign):
            return self._atribuicao(no)
        raise TypeError(f"não é expressão: {type(no).__name__}")

    def _chamada(self, no: A.Call, com_valor: bool) -> tuple[str, str]:
        retorno, params = self.assinaturas[no.callee.name]
        operandos = []
        for arg, (tipo_param, eh_vetor) in zip(no.args, params):
            operando, tipo = self._expr(arg)
            if not eh_vetor:
                operando = self._converter(operando, tipo, tipo_param)
            operandos.append(operando)
        for operando in operandos:
            self._emitir("param", a=operando)
        if not com_valor:
            self._emitir("call", a=no.callee.name, n=len(operandos))
            return "", retorno
        t = self._temp()
        self._emitir("call", dst=t, a=no.callee.name, n=len(operandos))
        return t, retorno

    def _binario(self, no: A.Binary) -> tuple[str, str]:
        op = no.op
        if op in ("&&", "||") and _contem_chamada(no.right):
            return self._curto_circuito(no)

        esquerda, tipo_e = self._expr(no.left)
        direita, tipo_d = self._expr(no.right)
        if op in ("&&", "||"):
            t = self._temp()
            self._emitir("bin", dst=t, a=esquerda, op="and" if op == "&&" else "or", b=direita)
            return t, "bool"

        if op in ("==", "!=") and tipo_e == "bool":
            comum = "bool"
        elif op == "%":
            comum = "int"
        else:
            comum = "float" if "float" in (tipo_e, tipo_d) else "int"
        esquerda = self._converter(esquerda, tipo_e, comum)
        direita = self._converter(direita, tipo_d, comum)
        t = self._temp()
        self._emitir("bin", dst=t, a=esquerda, op=op, b=direita)
        resultado = "bool" if op in ("<", "<=", ">", ">=", "==", "!=") else comum
        return t, resultado

    def _curto_circuito(self, no: A.Binary) -> tuple[str, str]:
        esquerda, _ = self._expr(no.left)
        t = self._temp()
        fim = self._rotulo("L_sc")
        self._emitir("copy", dst=t, a=esquerda)
        self._emitir("iff" if no.op == "&&" else "ift", a=t, label=fim)
        direita, _ = self._expr(no.right)
        self._emitir("copy", dst=t, a=direita)
        self._emitir("label", label=fim)
        return t, "bool"

    def _atribuicao(self, no: A.Assign) -> tuple[str, str]:
        if isinstance(no.target, A.Index):
            endereco = self._endereco(no.target.index)
            var = self._buscar(no.target.base.name)
            operando, tipo = self._expr(no.value)
            operando = self._converter(operando, tipo, var.tipo)
            self._emitir("store", a=var.nome_ir, b=endereco, c=operando)
            return operando, var.tipo
        var = self._buscar(no.target.name)
        self._atribuir_variavel(var.nome_ir, var.tipo, no.value)
        return var.nome_ir, var.tipo


def _coletar_declaracoes(no, saida: list):
    """Nomes declarados num corpo de função, em ordem (para decidir sufixos)."""
    if isinstance(no, A.Block):
        for item in no.items:
            if isinstance(item, A.VarDecl):
                saida.append(item.name)
            else:
                _coletar_declaracoes(item, saida)
    elif isinstance(no, A.If):
        _coletar_declaracoes(no.then, saida)
        _coletar_declaracoes(no.otherwise, saida)
    elif isinstance(no, (A.While, A.For)):
        _coletar_declaracoes(no.body, saida)


def gerar(programa: A.Program) -> ProgramaIR:
    """IR (-O0) de um programa que passou pela análise semântica."""
    return Gerador(programa).gerar()
