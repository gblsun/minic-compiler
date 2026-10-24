"""Otimizador da IR do MINIC (-O1): passes locais e globais até ponto fixo.

    python minic.py --ir -O1 codigo.c

Toda transformação aqui tem de preservar o comportamento observável do
programa (aula 18, seção 1): o que é impresso, o que é lido, as chamadas e os
acessos a vetor. Os passes, na ordem em que rodam, repetidos enquanto algum
deles ainda mudar alguma coisa:

1. **Simplificação local** (`_simplificar`): dobra de constantes inteiras e
   lógicas (`2 + 3` → `5`, `3 < 5` → `true`), identidades algébricas
   (`x + 0`, `x * 1`, `x * 0`, `b and true`, ...), conversões de literal
   (`itof 2` → `2.0`, `ctoi 'A'` → `65`) e desvio condicional sobre
   constante (`ifFalse true` some; `ifFalse false` vira `goto`).
2. **Propagação global de constantes e cópias** (`_propagar`): análise de
   fluxo de dados *forward* sobre o grafo de blocos básicos — no fundo, uma
   análise de *reaching definitions* restrita a cópias: um uso de `x` vira
   `c` quando, em **todos** os caminhos até ali, a última definição de `x`
   foi `x = c` e nem `x` nem `c` mudaram depois.
3. **Eliminação de subexpressões comuns local** (`_cse`): dentro de um bloco
   básico, `t3 = a * b` depois de `t1 = a * b` (sem redefinir `a`, `b` ou
   `t1` no meio) vira `t3 = t1`.
4. **Limpeza do fluxo de controle** (`_limpar`): remove blocos
   inalcançáveis (código depois de `return`, ramo de `if` constante), `goto`
   para o rótulo seguinte e rótulos que ninguém usa.
5. **Eliminação de código morto global** (`_mortos`): análise *backward* de
   variáveis vivas (*live variables*); uma atribuição a uma variável local ou
   temporário que não é lida em nenhum caminho adiante é removida.

No fim, os temporários são renumerados em ordem de aparição (`t1`, `t2`, ...).

Cuidados que valem para todos os passes:

- **Globais** podem ser lidas e escritas por qualquer função chamada: uma
  `call` invalida tudo o que se sabia sobre elas, e elas estão vivas na saída
  da função e em toda chamada.
- **Nada com efeito é removido**: `print`, `read`, `call`, `store` e `load`
  (um `load` pode acessar fora do vetor) ficam sempre; uma divisão ou resto
  só é removida se o divisor for uma constante diferente de zero.
- **Inteiros têm 32 bits**: a dobra de constantes reproduz o
  transbordamento com complemento de dois e a divisão truncada em direção a
  zero do C, e não dobra divisão por zero nem `INT_MIN / -1`. Valores `float`
  não são dobrados — o texto do resultado dependeria de como cada linguagem
  formata números reais, e as versões em C e Python precisam produzir a mesma
  IR byte a byte.
"""

import re

from ir import Instr, eh_literal

INTEIRO = re.compile(r"-?[0-9]+")
TEMPORARIO = re.compile(r"t[0-9]+")
INT_MIN, INT_MAX = -(2**31), 2**31 - 1

# Valor de cada sequência de escape de um literal de caractere na IR (a forma
# que `minic_ast.escapar` produz).
ESCAPES = {"n": 10, "t": 9, "r": 13, "\\": 92, "'": 39, '"': 34}

DESVIOS = ("goto", "iff", "ift", "ret")
# Instruções cujo resultado pode ser descartado sem mudar o comportamento.
REMOVIVEIS = ("copy", "bin", "un")


# -- operandos e instruções ------------------------------------------------------


def _inteiro(operando):
    """Valor de um literal inteiro que cabe em 32 bits, ou None."""
    if operando is None or len(operando) > 11 or not INTEIRO.fullmatch(operando):
        return None
    valor = int(operando)
    return valor if INT_MIN <= valor <= INT_MAX else None


def _int32(valor: int) -> int:
    return (valor + 2**31) % 2**32 - 2**31


def _codigo_char(operando):
    """Código de um literal de caractere ASCII (`'A'`, `'\\n'`), ou None."""
    if operando is None or len(operando) < 3 or operando[0] != "'" or operando[-1] != "'":
        return None
    corpo = operando[1:-1]
    if len(corpo) == 1 and corpo != "\\" and ord(corpo) < 128:
        return ord(corpo)
    if len(corpo) == 2 and corpo[0] == "\\" and corpo[1] in ESCAPES:
        return ESCAPES[corpo[1]]
    return None


def _definido(ins: Instr):
    """Nome definido (escrito) pela instrução, ou None."""
    if ins.kind in ("copy", "bin", "un", "load", "read", "call"):
        return ins.dst
    return None


def _campos_de_uso(ins: Instr) -> tuple:
    """Campos da instrução que são operandos lidos (sem nomes de vetor/função)."""
    k = ins.kind
    if k in ("copy", "un", "print", "param", "ret", "iff", "ift"):
        return ("a",)
    if k == "bin":
        return ("a", "b")
    if k == "load":
        return ("b",)
    if k == "store":
        return ("b", "c")
    return ()


def _usos(ins: Instr, globais: set) -> set:
    usos = set()
    for campo in _campos_de_uso(ins):
        valor = getattr(ins, campo)
        if valor is not None and not eh_literal(valor):
            usos.add(valor)
    if ins.kind in ("call", "ret"):
        usos |= globais
    return usos


# -- blocos básicos ----------------------------------------------------------------


def _blocos(instrs: list):
    """Particiona em blocos básicos: (lista de (início, fim), sucessores, predecessores)."""
    if not instrs:
        return [], [], []
    lideres = {0}
    for i, ins in enumerate(instrs):
        if ins.kind == "label":
            lideres.add(i)
        if ins.kind in DESVIOS and i + 1 < len(instrs):
            lideres.add(i + 1)
    inicios = sorted(lideres)
    blocos = [
        (inicio, inicios[j + 1] if j + 1 < len(inicios) else len(instrs))
        for j, inicio in enumerate(inicios)
    ]
    bloco_do_rotulo = {}
    for b, (inicio, _) in enumerate(blocos):
        if instrs[inicio].kind == "label":
            bloco_do_rotulo[instrs[inicio].label] = b

    sucessores = []
    for b, (_, fim) in enumerate(blocos):
        ultima = instrs[fim - 1]
        seguinte = [b + 1] if b + 1 < len(blocos) else []
        if ultima.kind == "goto":
            succ = [bloco_do_rotulo[ultima.label]]
        elif ultima.kind in ("iff", "ift"):
            succ = [bloco_do_rotulo[ultima.label]] + seguinte
        elif ultima.kind == "ret":
            succ = []
        else:
            succ = seguinte
        sucessores.append(list(dict.fromkeys(succ)))

    predecessores = [[] for _ in blocos]
    for b, succ in enumerate(sucessores):
        for s in succ:
            predecessores[s].append(b)
    return blocos, sucessores, predecessores


# -- 1. simplificação local ----------------------------------------------------------


def _dobrar_binario(op, a, b):
    """Resultado constante de `a op b` (texto), ou None se não der para dobrar."""
    x, y = _inteiro(a), _inteiro(b)
    if x is not None and y is not None:
        if op == "+":
            return str(_int32(x + y))
        if op == "-":
            return str(_int32(x - y))
        if op == "*":
            return str(_int32(x * y))
        if op in ("/", "%"):
            if y == 0 or (x == INT_MIN and y == -1):
                return None
            q = abs(x) // abs(y)
            if (x < 0) != (y < 0):
                q = -q
            return str(q) if op == "/" else str(x - q * y)
        comparacoes = {
            "<": x < y,
            "<=": x <= y,
            ">": x > y,
            ">=": x >= y,
            "==": x == y,
            "!=": x != y,
        }
        if op in comparacoes:
            return "true" if comparacoes[op] else "false"
        return None
    if a in ("true", "false") and b in ("true", "false"):
        p, q = a == "true", b == "true"
        resultado = {"and": p and q, "or": p or q, "==": p == q, "!=": p != q}.get(op)
        return None if resultado is None else ("true" if resultado else "false")
    return None


def _identidade(op, a, b):
    """Operando equivalente a `a op b` por identidade algébrica, ou None."""
    if op == "+":
        if b == "0":
            return a
        if a == "0":
            return b
    elif op == "-":
        if b == "0":
            return a
    elif op == "*":
        if b == "1":
            return a
        if a == "1":
            return b
        if a == "0" or b == "0":
            return "0"
    elif op == "/":
        if b == "1":
            return a
    elif op == "and":
        if b == "true":
            return a
        if a == "true":
            return b
        if a == "false" or b == "false":
            return "false"
    elif op == "or":
        if b == "false":
            return a
        if a == "false":
            return b
        if a == "true" or b == "true":
            return "true"
    return None


def _dobrar_unario(op, a):
    if op == "neg":
        x = _inteiro(a)
        return None if x is None else str(_int32(-x))
    if op == "not":
        return {"true": "false", "false": "true"}.get(a)
    if op == "itof":
        return a + ".0" if INTEIRO.fullmatch(a) else None
    if op == "ctoi":
        codigo = _codigo_char(a)
        return None if codigo is None else str(codigo)
    return None


def _simplificar(funcao) -> bool:
    mudou = False
    novas = []
    for ins in funcao.instrs:
        k = ins.kind
        if k == "bin":
            valor = _dobrar_binario(ins.op, ins.a, ins.b)
            if valor is None:
                valor = _identidade(ins.op, ins.a, ins.b)
            if valor is not None:
                ins = Instr("copy", dst=ins.dst, a=valor)
                mudou = True
        elif k == "un":
            valor = _dobrar_unario(ins.op, ins.a)
            if valor is not None:
                ins = Instr("copy", dst=ins.dst, a=valor)
                mudou = True
        elif k in ("iff", "ift") and ins.a in ("true", "false"):
            mudou = True
            salta = (ins.a == "false") == (k == "iff")
            if not salta:
                continue
            ins = Instr("goto", label=ins.label)
        if ins.kind == "copy" and ins.dst == ins.a:
            mudou = True
            continue
        novas.append(ins)
    funcao.instrs = novas
    return mudou


# -- 2. propagação de constantes e cópias ---------------------------------------------


def _encontro(conjuntos: list) -> dict:
    """Interseção: só os fatos `x = c` que valem em todos os caminhos."""
    resultado = dict(conjuntos[0])
    for outro in conjuntos[1:]:
        resultado = {x: c for x, c in resultado.items() if outro.get(x) == c}
    return resultado


def _matar(fatos: dict, nome: str):
    """`nome` mudou: some o fato sobre ele e todo fato que o usava como valor."""
    for x in [x for x, c in fatos.items() if x == nome or c == nome]:
        del fatos[x]


def _transferir(ins: Instr, fatos: dict, globais: set):
    if ins.kind == "call":
        for x in [x for x, c in fatos.items() if x in globais or c in globais]:
            del fatos[x]
    destino = _definido(ins)
    if destino is None:
        return
    valor = fatos.get(ins.a, ins.a) if ins.kind == "copy" else None
    _matar(fatos, destino)
    if valor is not None and valor != destino:
        fatos[destino] = valor


def _propagar(funcao, globais: set) -> bool:
    instrs = funcao.instrs
    blocos, _, predecessores = _blocos(instrs)
    entrada = [None] * len(blocos)  # None = ainda sem informação (topo)
    saida = [None] * len(blocos)
    mudou = True
    while mudou:
        mudou = False
        for b, (inicio, fim) in enumerate(blocos):
            if b == 0:
                fatos = {}
            else:
                conhecidos = [saida[p] for p in predecessores[b] if saida[p] is not None]
                fatos = _encontro(conhecidos) if conhecidos else None
            entrada[b] = fatos
            if fatos is not None:
                fatos = dict(fatos)
                for i in range(inicio, fim):
                    _transferir(instrs[i], fatos, globais)
            if fatos != saida[b]:
                saida[b] = fatos
                mudou = True

    reescreveu = False
    for b, (inicio, fim) in enumerate(blocos):
        fatos = dict(entrada[b] or {})
        for i in range(inicio, fim):
            ins = instrs[i]
            for campo in _campos_de_uso(ins):
                valor = getattr(ins, campo)
                if valor in fatos:
                    setattr(ins, campo, fatos[valor])
                    reescreveu = True
            _transferir(ins, fatos, globais)
    return reescreveu


# -- 3. subexpressões comuns (local) ------------------------------------------------------


def _cse(funcao, globais: set) -> bool:
    instrs = funcao.instrs
    blocos, _, _ = _blocos(instrs)
    mudou = False
    for inicio, fim in blocos:
        disponiveis = []  # (kind, op, a, b, quem guarda o valor)
        for i in range(inicio, fim):
            ins = instrs[i]
            if ins.kind in ("bin", "un"):
                for kind, op, a, b, guardiao in disponiveis:
                    if (kind, op, a, b) == (ins.kind, ins.op, ins.a, ins.b):
                        ins = instrs[i] = Instr("copy", dst=ins.dst, a=guardiao)
                        mudou = True
                        break
            if ins.kind == "call":
                disponiveis = [d for d in disponiveis if not ({d[2], d[3], d[4]} & globais)]
            destino = _definido(ins)
            if destino is not None:
                disponiveis = [d for d in disponiveis if destino not in (d[2], d[3], d[4])]
            if ins.kind in ("bin", "un") and ins.dst not in (ins.a, ins.b):
                disponiveis.append((ins.kind, ins.op, ins.a, ins.b, ins.dst))
    return mudou


# -- 4. limpeza do fluxo de controle ---------------------------------------------------------


def _limpar(funcao) -> bool:
    mudou_alguma = False
    while True:
        instrs = funcao.instrs
        mudou = False

        # Blocos inalcançáveis a partir da entrada.
        blocos, sucessores, _ = _blocos(instrs)
        alcancados = set()
        pilha = [0] if blocos else []
        while pilha:
            b = pilha.pop()
            if b not in alcancados:
                alcancados.add(b)
                pilha.extend(sucessores[b])
        if len(alcancados) < len(blocos):
            instrs = [
                instrs[i] for b, (ini, fim) in enumerate(blocos) if b in alcancados
                for i in range(ini, fim)
            ]
            mudou = True

        # Desvio para um rótulo que já vem logo em seguida.
        novas = []
        for i, ins in enumerate(instrs):
            if ins.kind in ("goto", "iff", "ift"):
                j = i + 1
                seguintes = set()
                while j < len(instrs) and instrs[j].kind == "label":
                    seguintes.add(instrs[j].label)
                    j += 1
                if ins.label in seguintes:
                    mudou = True
                    continue
            novas.append(ins)
        instrs = novas

        # Rótulos sem nenhum desvio apontando para eles.
        usados = {ins.label for ins in instrs if ins.kind in ("goto", "iff", "ift")}
        novas = [ins for ins in instrs if ins.kind != "label" or ins.label in usados]
        if len(novas) < len(instrs):
            mudou = True

        funcao.instrs = novas
        if not mudou:
            return mudou_alguma
        mudou_alguma = True


# -- 5. código morto (variáveis vivas) ----------------------------------------------------------


def _removivel(ins: Instr) -> bool:
    if ins.kind not in REMOVIVEIS:
        return False
    if ins.kind == "bin" and ins.op in ("/", "%"):
        divisor = _inteiro(ins.b)
        return divisor is not None and divisor != 0
    return True


def _mortos(funcao, globais: set) -> bool:
    instrs = funcao.instrs
    blocos, sucessores, _ = _blocos(instrs)
    vivas_entrada = [set() for _ in blocos]
    vivas_saida = [set() for _ in blocos]
    mudou = True
    while mudou:
        mudou = False
        for b in range(len(blocos) - 1, -1, -1):
            inicio, fim = blocos[b]
            if sucessores[b]:
                vivas = set().union(*(vivas_entrada[s] for s in sucessores[b]))
            else:
                vivas = set(globais)  # fim da função
            vivas_saida[b] = set(vivas)
            for i in range(fim - 1, inicio - 1, -1):
                ins = instrs[i]
                destino = _definido(ins)
                if destino is not None:
                    vivas.discard(destino)
                vivas |= _usos(ins, globais)
            if vivas != vivas_entrada[b]:
                vivas_entrada[b] = vivas
                mudou = True

    removeu = False
    manter = [True] * len(instrs)
    for b, (inicio, fim) in enumerate(blocos):
        vivas = set(vivas_saida[b])
        for i in range(fim - 1, inicio - 1, -1):
            ins = instrs[i]
            destino = _definido(ins)
            morto = destino is not None and destino not in vivas and destino not in globais
            if morto and _removivel(ins):
                manter[i] = False
                removeu = True
                continue
            if morto and ins.kind == "call":
                ins.dst = None
                removeu = True
            if destino is not None:
                vivas.discard(destino)
            vivas |= _usos(ins, globais)
    funcao.instrs = [ins for i, ins in enumerate(instrs) if manter[i]]
    return removeu


# -- pipeline --------------------------------------------------------------------------------------


def _renumerar(funcao):
    """Temporários renumerados em ordem de aparição no texto."""
    novos = {}
    for ins in funcao.instrs:
        # Na `call`, o campo `a` é o nome da função, não um operando.
        campos = ("dst",) if ins.kind == "call" else ("dst", "a", "b", "c")
        for campo in campos:
            valor = getattr(ins, campo)
            if valor is not None and TEMPORARIO.fullmatch(valor):
                if valor not in novos:
                    novos[valor] = f"t{len(novos) + 1}"
                setattr(ins, campo, novos[valor])


# Teto de rodadas: cada rodada só encolhe ou simplifica a IR, então o ponto fixo
# chega bem antes disso; o limite só garante que o laço termina.
MAX_RODADAS = 50


def otimizar(programa):
    """Aplica os passes em cada função até nada mais mudar (modifica no lugar)."""
    globais = programa.nomes_globais()
    for funcao in programa.funcoes:
        for _ in range(MAX_RODADAS):
            mudou = _simplificar(funcao)
            mudou |= _propagar(funcao, globais)
            mudou |= _cse(funcao, globais)
            mudou |= _limpar(funcao)
            mudou |= _mortos(funcao, globais)
            if not mudou:
                break
        _renumerar(funcao)
    return programa
