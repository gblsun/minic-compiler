"""Benchmarks -O0 × -O1 do compilador MINIC (Seção 15 da especificação).

    python benchmarks/rodar.py                 # mede e imprime a tabela
    python benchmarks/rodar.py --escrever      # e regrava docs/benchmarks.md

Para cada programa de `benchmarks/*.c`, compila com o `minic.c` (unidade
única) e mede, em -O0 e em -O1:

- **instruções executadas** pela máquina virtual (`--run --stats`): a métrica
  principal, porque é determinística — não depende da máquina nem da carga;
- **tamanho do código**: instruções da MVM geradas (`--target=asm`);
- **tempo** de execução da versão em C (melhor de 5 rodadas, inclui iniciar
  o processo): ilustrativo, varia de máquina para máquina.

E confere que a saída do programa é **idêntica** nos dois níveis: um
benchmark que mudasse o resultado não mediria otimização, mediria um defeito.
"""

import argparse
import platform
import subprocess
import sys
import time
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
PASTA = Path(__file__).resolve().parent
RELATORIO = RAIZ / "docs" / "benchmarks.md"
ALVO = RAIZ / "src" / "c" / "minic"
RODADAS = 5


def compilar() -> Path:
    subprocess.run(
        ["gcc", "-O2", "-Wall", "-Wextra", str(RAIZ / "minic.c"), "-o", str(ALVO)],
        check=True,
        cwd=RAIZ,
    )
    com_exe = ALVO.with_suffix(".exe")
    return com_exe if com_exe.exists() else ALVO


def medir(minic: Path, fonte: Path, nivel: str) -> dict:
    p = subprocess.run(
        [str(minic), "--run", nivel, "--stats", str(fonte)], capture_output=True, check=True
    )
    instrucoes = int(p.stderr.decode("utf-8").rsplit(":", 1)[1])
    asm = subprocess.run(
        [str(minic), "--target=asm", nivel, str(fonte)], capture_output=True, check=True
    ).stdout.decode("utf-8")
    tamanho = sum(1 for linha in asm.splitlines() if linha.startswith("  "))
    tempos = []
    for _ in range(RODADAS):
        inicio = time.perf_counter()
        subprocess.run([str(minic), "--run", nivel, str(fonte)], capture_output=True, check=True)
        tempos.append(time.perf_counter() - inicio)
    return {"saida": p.stdout, "instrucoes": instrucoes, "tamanho": tamanho, "tempo": min(tempos)}


def reducao(antes: float, depois: float) -> str:
    return f"{100 * (antes - depois) / antes:.1f}%" if antes else "—"


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--escrever", action="store_true", help=f"regrava {RELATORIO.name}")
    args = parser.parse_args(argv)
    sys.stdout.reconfigure(encoding="utf-8")

    minic = compilar()
    linhas = []
    total0 = total1 = 0
    for fonte in sorted(PASTA.glob("*.c")):
        o0, o1 = medir(minic, fonte, "-O0"), medir(minic, fonte, "-O1")
        if o0["saida"] != o1["saida"]:
            print(f"ERRO: {fonte.name} imprime coisas diferentes em -O0 e -O1", file=sys.stderr)
            return 1
        total0 += o0["instrucoes"]
        total1 += o1["instrucoes"]
        resultado = o0["saida"].decode("utf-8").split()
        linhas.append(
            f"| [`{fonte.name}`](../benchmarks/{fonte.name}) | {' '.join(resultado)} "
            f"| {o0['instrucoes']:,} | {o1['instrucoes']:,} "
            f"| **{reducao(o0['instrucoes'], o1['instrucoes'])}** "
            f"| {o0['tamanho']} → {o1['tamanho']} "
            f"| {o0['tempo'] * 1000:.0f} → {o1['tempo'] * 1000:.0f} |".replace(",", ".")
        )
    tabela = [
        "| Programa | Saída | Instruções executadas -O0 | -O1 | Redução | Tamanho do código (instruções) | Tempo (ms, C) |",
        "| -------- | ----- | ------------------------: | --: | ------: | -----------------------------: | ------------: |",
        *linhas,
        f"| **total** | | **{total0:,}** | **{total1:,}** | **{reducao(total0, total1)}** | | |".replace(",", "."),
    ]
    print("\n".join(tabela))

    if args.escrever:
        ambiente = f"{platform.system()} {platform.machine()}, Python {platform.python_version()}"
        modelo = (PASTA / "relatorio.modelo.md").read_text(encoding="utf-8")
        texto = modelo.replace("{{TABELA}}", "\n".join(tabela)).replace("{{AMBIENTE}}", ambiente)
        RELATORIO.write_text(texto, encoding="utf-8", newline="\n")
        print(f"\n{RELATORIO.relative_to(RAIZ)} atualizado.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
