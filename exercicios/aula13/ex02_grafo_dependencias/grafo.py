"""Exercício 02 — grafo de dependências e avaliador topológico.

Entrada: a descrição de instâncias de atributos, uma por linha,

    <atributo> = <ação> <argumentos...>
    resultado <atributo>          (opcional; padrão: o último atributo definido)

com as ações

    const N        valor inteiro fixo (ex.: num1.lexval = const 3)
    copia X        cópia de outro atributo (ex.: F1.val = copia num1.lexval)
    soma X Y | sub X Y | mul X Y | div X Y

Linhas em branco e comentários (`#`) são ignorados. Cada argumento que é um
atributo gera uma aresta dirigida `argumento -> atributo definido`: o atributo
só pode ser calculado depois dos que ele usa.

A ordem de avaliação sai do algoritmo de Kahn: começa pelos atributos sem
dependências (na ordem em que foram declarados) e libera um atributo quando o
último pré-requisito dele é avaliado. Se sobrar atributo que nunca chega a
grau de entrada zero, há ciclo: o programa lista os atributos bloqueados e
**não** executa nenhuma ação — não existe valor parcial "válido".

Uso:  python grafo.py <arquivo | ->
"""

import sys
from collections import deque
from pathlib import Path

OK, ERRO_USO, ERRO_SINTAXE, ERRO_SEMANTICO = 0, 1, 3, 4
MIN, MAX = -(2**63), 2**63 - 1
ARIDADE = {"const": 1, "copia": 1, "soma": 2, "sub": 2, "mul": 2, "div": 2}


class ErroGrafo(Exception):
    def __init__(self, codigo: int, mensagem: str):
        super().__init__(mensagem)
        self.codigo = codigo


def ler_grafo(texto: str):
    """Devolve (graph, actions, resultado).

    graph:   {atributo: [atributos que dependem dele]}, na ordem de declaração
    actions: {atributo: (linha, ação, argumentos)}
    """
    actions: dict[str, tuple[int, str, list]] = {}
    resultado = None
    for n, linha in enumerate(texto.splitlines(), start=1):
        partes = linha.split("#", 1)[0].split()
        if not partes:
            continue
        if partes[0] == "resultado":
            if len(partes) != 2:
                raise ErroGrafo(ERRO_SINTAXE, f"Erro de sintaxe na linha {n}: 'resultado' espera um atributo.")
            resultado = (n, partes[1])
            continue
        if len(partes) < 3 or partes[1] != "=":
            raise ErroGrafo(ERRO_SINTAXE, f"Erro de sintaxe na linha {n}: esperado '<atributo> = <ação> <argumentos>'.")
        nome, acao, args = partes[0], partes[2], partes[3:]
        if acao not in ARIDADE:
            raise ErroGrafo(ERRO_SINTAXE, f"Erro de sintaxe na linha {n}: ação desconhecida '{acao}'.")
        if len(args) != ARIDADE[acao]:
            raise ErroGrafo(
                ERRO_SINTAXE,
                f"Erro de sintaxe na linha {n}: '{acao}' espera {ARIDADE[acao]} argumento(s).",
            )
        if acao == "const":
            args = [inteiro(args[0], n)]
        if nome in actions:
            raise ErroGrafo(
                ERRO_SEMANTICO,
                f"Erro semântico na linha {n}: atributo '{nome}' já definido na linha {actions[nome][0]}.",
            )
        actions[nome] = (n, acao, args)

    if not actions:
        raise ErroGrafo(ERRO_SINTAXE, "Erro de sintaxe: nenhum atributo definido.")

    # Arestas: argumento -> atributo definido, na ordem de declaração.
    graph: dict[str, list[str]] = {nome: [] for nome in actions}
    for nome, (n, acao, args) in actions.items():
        if acao == "const":
            continue
        for arg in args:
            if arg not in actions:
                raise ErroGrafo(
                    ERRO_SEMANTICO,
                    f"Erro semântico na linha {n}: atributo '{arg}' usado por '{nome}' não foi definido.",
                )
            if nome not in graph[arg]:  # `soma a a` gera uma aresta só
                graph[arg].append(nome)

    if resultado is None:
        resultado = list(actions)[-1]
    else:
        n, nome = resultado
        if nome not in actions:
            raise ErroGrafo(ERRO_SEMANTICO, f"Erro semântico na linha {n}: atributo '{nome}' não foi definido.")
        resultado = nome
    return graph, actions, resultado


def inteiro(texto: str, n: int) -> int:
    corpo = texto[1:] if texto.startswith("-") else texto
    if not corpo.isascii() or not corpo.isdigit():
        raise ErroGrafo(ERRO_SINTAXE, f"Erro de sintaxe na linha {n}: 'const' espera um inteiro, encontrado '{texto}'.")
    valor = int(texto)
    if not MIN <= valor <= MAX:
        raise ErroGrafo(ERRO_SINTAXE, f"Erro de sintaxe na linha {n}: inteiro '{texto}' fora do intervalo de 64 bits.")
    return valor


def ordem_topologica(graph: dict[str, list[str]]):
    """Kahn. Devolve (ordem, bloqueados); bloqueados != [] indica ciclo."""
    grau = {nome: 0 for nome in graph}
    for sucessores in graph.values():
        for s in sucessores:
            grau[s] += 1
    fila = deque(nome for nome in graph if grau[nome] == 0)
    ordem = []
    while fila:
        atual = fila.popleft()
        ordem.append(atual)
        for s in graph[atual]:
            grau[s] -= 1
            if grau[s] == 0:
                fila.append(s)
    bloqueados = [nome for nome in graph if grau[nome] > 0]
    return ordem, bloqueados


def aplica(acao: str, a: int, b: int):
    """Ação aritmética com a semântica de inteiros de 64 bits do C. None = erro."""
    if acao == "div":
        if b == 0:
            return None, "divisão por zero"
        q = abs(a) // abs(b)
        r = -q if (a < 0) != (b < 0) else q
    else:
        r = {"soma": a + b, "sub": a - b, "mul": a * b}[acao]
    if not MIN <= r <= MAX:
        return None, "estouro de inteiro de 64 bits"
    return r, None


def evaluate(graph, actions):
    """Executa as ações em ordem topológica. Devolve (ordem, valores).

    Lança ErroGrafo em ciclo (antes de executar qualquer ação) ou em erro de
    execução de uma ação.
    """
    ordem, bloqueados = ordem_topologica(graph)
    if bloqueados:
        raise ErroGrafo(
            ERRO_SEMANTICO,
            "Erro semântico: ciclo de dependências; atributos bloqueados: " + ", ".join(bloqueados) + ".",
        )
    valores: dict[str, int] = {}
    for i, nome in enumerate(ordem, start=1):
        n, acao, args = actions[nome]
        if acao == "const":
            valores[nome] = args[0]
            print(f"  {i}. {nome} := {args[0]}")
        elif acao == "copia":
            valores[nome] = valores[args[0]]
            print(f"  {i}. {nome} := {args[0]} = {valores[nome]}")
        else:
            a, b = valores[args[0]], valores[args[1]]
            r, erro = aplica(acao, a, b)
            if erro:
                raise ErroGrafo(ERRO_SEMANTICO, f"Erro semântico na linha {n}: {erro} ao avaliar '{nome}' ({acao}({a}, {b})).")
            valores[nome] = r
            print(f"  {i}. {nome} := {acao}({args[0]}, {args[1]}) = {acao}({a}, {b}) = {r}")
    return ordem, valores


def main(argv: list[str]) -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    if len(argv) != 2:
        print("uso: python grafo.py <arquivo | ->", file=sys.stderr)
        return ERRO_USO
    try:
        dados = sys.stdin.buffer.read() if argv[1] == "-" else Path(argv[1]).read_bytes()
    except OSError as exc:
        print(f"erro ao abrir '{argv[1]}': {exc.strerror}", file=sys.stderr)
        return ERRO_USO

    try:
        graph, actions, resultado = ler_grafo(dados.decode("utf-8", errors="replace"))
        arestas = sum(len(s) for s in graph.values())
        print(f"atributos: {len(graph)}, dependências: {arestas}")
        for origem, sucessores in graph.items():
            for destino in sucessores:
                print(f"  {origem} -> {destino}")
        print("ordem de avaliação:")
        _, valores = evaluate(graph, actions)
    except ErroGrafo as erro:
        print(erro, file=sys.stderr)
        return erro.codigo
    print(f"resultado: {resultado} = {valores[resultado]}")
    return OK


if __name__ == "__main__":
    sys.exit(main(sys.argv))
