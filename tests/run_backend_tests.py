"""Testa o back-end: geração de código para a máquina de pilha e execução.

    python tests/run_backend_tests.py              # tudo, nas duas implementações
    python tests/run_backend_tests.py --python     # só a versão Python
    python tests/run_backend_tests.py --c          # só a versão C
    python tests/run_backend_tests.py --update     # regrava os golden de execução
    python tests/run_backend_tests.py -v           # detalha cada caso

Três suítes:

**1. Execução** — `tests/exec_inputs/*.c` (entrada de `read` em `<nome>.in`),
golden em `tests/exec_expected/<nome>.{out,err,exit}`, byte a byte, com
`--run -O0` **e** `--run -O1`: os dois níveis têm de produzir exatamente a
mesma saída, o mesmo erro e o mesmo código. Cobre os programas de referência
da especificação (13.1, 19.1, 19.2), o formato do `print` de cada tipo, o
`read` de cada tipo, vetores e globais, e os erros: de execução (código 6:
divisão por zero, índice fora do vetor, recursão infinita, entrada inválida
ou esgotada) e de geração de código (código 5: programa sem `main`).

**2. Oráculo** — todo programa válido do repositório (casos de IR oficiais e
próprios, de execução, da semântica e do parser) é executado na máquina
virtual em `-O0` e `-O1`, e a saída tem de ser a mesma que a do interpretador
da IR (`tests/ir_interp.py`), que é independente do back-end. Assim o
gerador de código e a VM são conferidos contra a semântica da IR.

**3. Equivalência** — C e Python produzem stdout, stderr e código de saída
idênticos em `--tokens`, `--ast`, `--symbols`, `--target=asm` e `--run`
(nos dois níveis), em todas as entradas.
"""

import argparse
import subprocess
import sys
from pathlib import Path

import ir_interp

RAIZ = Path(__file__).resolve().parent.parent
AQUI = Path(__file__).resolve().parent
EXECUCAO = AQUI / "exec_inputs"
ESPERADO = AQUI / "exec_expected"
OUTRAS_ENTRADAS = [
    RAIZ / "ref" / "testes-oficiais" / "testes-ir-30" / "testes",
    AQUI / "ir_inputs",
    AQUI / "semantic_inputs",
    AQUI / "parser_inputs",
    RAIZ / "ref" / "testes-oficiais" / "testes-semanticos-20",
    RAIZ / "ref" / "testes-oficiais" / "testes-parser-50" / "casos",
]
MINIC_PY = RAIZ / "minic.py"
MINIC_C_FONTE = RAIZ / "minic.c"
MINIC_C_ALVO = RAIZ / "src" / "c" / "minic"
# Entrada para programas com `read` que não trazem um .in próprio.
ENTRADA_PADRAO = (b"3 -2 7 0 5 " * 20) + b"\n"


def binario_c() -> Path:
    com_exe = MINIC_C_ALVO.with_suffix(".exe")
    return com_exe if com_exe.exists() else MINIC_C_ALVO


def compilar_c() -> bool:
    """Compila o minic.c da raiz como unidade única, como os scripts oficiais."""
    comando = ["gcc", "-Wall", "-Wextra", "-std=c11", str(MINIC_C_FONTE), "-o", str(MINIC_C_ALVO)]
    processo = subprocess.run(comando, capture_output=True, text=True, cwd=RAIZ)
    if processo.returncode != 0:
        print("falha ao compilar o minic.c:", file=sys.stderr)
        print(processo.stdout + processo.stderr, file=sys.stderr)
        return False
    return True


def entrada_de(fonte: Path) -> bytes:
    arquivo = fonte.with_suffix(".in")
    if arquivo.exists():
        return arquivo.read_bytes()
    return ENTRADA_PADRAO if b"read" in fonte.read_bytes() else b""


def rodar(comando, opcoes, fonte: Path, entrada: bytes = b""):
    p = subprocess.run(comando + opcoes + [str(fonte)], input=entrada, capture_output=True, cwd=RAIZ)
    return p.returncode, p.stdout, p.stderr


def mostrar(dados: bytes) -> str:
    return dados.decode("utf-8", errors="replace").strip().replace("\n", " ⏎ ")[:200]


# -- suíte 1: execução com golden ---------------------------------------------------------


def suite_execucao(nome: str, comando, verboso: bool, update: bool):
    print(f"== Back-end — execução ({nome}) ==")
    aprovados = total = 0
    for fonte in sorted(EXECUCAO.glob("*.c")):
        total += 1
        entrada = entrada_de(fonte)
        r0 = rodar(comando, ["--run", "-O0"], fonte, entrada)
        r1 = rodar(comando, ["--run", "-O1"], fonte, entrada)
        falhas = []
        if r0 != r1:
            falhas.append(f"-O0 e -O1 diferem: {mostrar(r0[1] + r0[2])} | {mostrar(r1[1] + r1[2])}")
        base = ESPERADO / fonte.stem
        if update:
            base.with_suffix(".out").write_bytes(r0[1])
            base.with_suffix(".err").write_bytes(r0[2])
            base.with_suffix(".exit").write_text(f"{r0[0]}\n")
        elif not base.with_suffix(".exit").exists():
            falhas.append("sem golden (rode com --update)")
        else:
            esperado = (
                int(base.with_suffix(".exit").read_text()),
                base.with_suffix(".out").read_bytes(),
                base.with_suffix(".err").read_bytes(),
            )
            if r0 != esperado:
                falhas.append(
                    f"esperado código {esperado[0]} {mostrar(esperado[1] + esperado[2])}; "
                    f"obtido código {r0[0]} {mostrar(r0[1] + r0[2])}"
                )
        if falhas:
            print(f"  FALHA {fonte.name}: " + "; ".join(falhas))
        else:
            aprovados += 1
            if verboso:
                print(f"  ok    {fonte.name}: código {r0[0]}  {mostrar(r0[1] + r0[2])}")
    print(f"  {aprovados}/{total} aprovados" + (" (golden regravados)" if update else ""))
    return aprovados, total


# -- suíte 2: oráculo (interpretador da IR) -------------------------------------------------


def todas_as_entradas() -> list[Path]:
    fontes = sorted(EXECUCAO.glob("*.c"))
    for pasta in OUTRAS_ENTRADAS:
        fontes += sorted(pasta.rglob("*.c"))
    return fontes


def interpretar(ir_texto: str, entrada: bytes):
    """Saída do interpretador no formato do print da VM, ou None se ele falhar."""
    itens = entrada.split()
    valores = []
    for item in itens:
        try:
            valores.append(int(item))
        except ValueError:
            return None  # entrada que o interpretador de teste não sabe ler
    try:
        return "".join(linha + "\n" for linha in ir_interp.executar(ir_texto, valores))
    except (ir_interp.ErroExecucao, RecursionError):
        return None  # o interpretador é recursivo: recursão profunda o derruba


def suite_oraculo(comando, verboso: bool):
    print("== Back-end — máquina virtual × interpretador da IR ==")
    aprovados = total = 0
    for fonte in todas_as_entradas():
        entrada = entrada_de(fonte)
        codigo_ir, ir0, _ = rodar(comando, ["--ir", "-O0"], fonte)
        if codigo_ir != 0:
            continue  # programa inválido: não há o que executar
        esperado = interpretar(ir0.decode("utf-8", "surrogateescape"), entrada)
        if esperado is None:
            continue  # sem main, erro de execução ou entrada não inteira
        resultados = {n: rodar(comando, ["--run", n], fonte, entrada) for n in ("-O0", "-O1")}
        if "fora dos limites".encode() in resultados["-O0"][2]:
            # O interpretador não confere limites de vetor; a VM confere. Esse
            # erro é coberto pela suíte de execução (erro_indice_fora.c).
            continue
        total += 1
        falhas = []
        for nivel, (codigo, saida, erro) in resultados.items():
            obtido = saida.decode("utf-8", "surrogateescape")
            if codigo != 0 or obtido != esperado:
                falhas.append(
                    f"{nivel}: código {codigo}, saída {mostrar(saida + erro)}; "
                    f"interpretador: {mostrar(esperado.encode('utf-8', 'surrogateescape'))}"
                )
        rotulo = fonte.relative_to(RAIZ).as_posix()
        if falhas:
            print(f"  FALHA {rotulo}: " + "; ".join(falhas))
        else:
            aprovados += 1
            if verboso:
                print(f"  ok    {rotulo}")
    print(f"  {aprovados}/{total} programas executados igual ao interpretador, em -O0 e -O1")
    return aprovados, total


# -- suíte 3: equivalência C × Python --------------------------------------------------------

MODOS = [
    ["--tokens"],
    ["--ast"],
    ["--symbols"],
    ["--symbols", "-O1"],
    ["--target=asm"],
    ["--target=asm", "-O1"],
    ["--run"],
    ["--run", "-O1"],
]


def suite_equivalencia(comando_py, comando_c, verboso: bool):
    print("== Back-end — equivalência C × Python (tokens, AST, símbolos, código, execução) ==")
    aprovados = total = 0
    for fonte in todas_as_entradas():
        entrada = entrada_de(fonte)
        total += 1
        falhas = [
            " ".join(modo)
            for modo in MODOS
            if rodar(comando_py, modo, fonte, entrada) != rodar(comando_c, modo, fonte, entrada)
        ]
        rotulo = fonte.relative_to(RAIZ).as_posix()
        if falhas:
            print(f"  FALHA {rotulo}: C e Python diferem em " + ", ".join(falhas))
        else:
            aprovados += 1
            if verboso:
                print(f"  ok    {rotulo}")
    print(f"  {aprovados}/{total} entradas idênticas nos {len(MODOS)} modos")
    return aprovados, total


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    grupo = parser.add_mutually_exclusive_group()
    grupo.add_argument("--python", action="store_true", help="só a versão Python")
    grupo.add_argument("--c", action="store_true", help="só a versão C")
    parser.add_argument("--update", action="store_true", help="regrava os golden de execução")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args(argv)
    sys.stdout.reconfigure(encoding="utf-8")

    comando_py = [sys.executable, str(MINIC_PY)]
    comando_c = None
    if not args.python:
        if not compilar_c():
            return 1
        comando_c = [str(binario_c())]

    resultados = []
    implementacoes = []
    if not args.c:
        implementacoes.append(("Python", comando_py))
    if comando_c:
        implementacoes.append(("C", comando_c))
    for nome, comando in implementacoes:
        resultados.append(suite_execucao(nome, comando, args.verbose, args.update and nome == "Python"))
    resultados.append(suite_oraculo(comando_c if args.c else comando_py, args.verbose))
    if comando_c and not args.c:
        resultados.append(suite_equivalencia(comando_py, comando_c, args.verbose))

    aprovados = sum(a for a, _ in resultados)
    total = sum(t for _, t in resultados)
    print(f"\nTotal: {aprovados}/{total}")
    return 0 if aprovados == total else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
