"""Testa o analisador semântico: 20 casos oficiais + casos próprios + equivalência.

    python tests/run_semantic_tests.py              # tudo, nas duas implementações
    python tests/run_semantic_tests.py --python     # só a versão Python
    python tests/run_semantic_tests.py --c          # só a versão C
    python tests/run_semantic_tests.py --update     # regrava os golden dos casos próprios
    python tests/run_semantic_tests.py -v           # detalha cada caso

Três suítes, como em run_parser_tests.py:

**1. Casos oficiais** — `ref/testes-oficiais/testes-semanticos-20/` (AP 2, não
editados): stdout tem de ser **byte a byte** igual ao `.gabarito`, com os CRLF
entre linhas e sem quebra no fim, exatamente como o script oficial
`testes_semanticos_*.sh` compara (`diff` sem normalização, stdout e stderr
juntos — por isso stderr tem de estar vazio).

**2. Casos próprios** — `tests/semantic_inputs/*.c`, golden em
`tests/semantic_expected/<nome>.{stdout,stderr,exit}.txt`, também byte a byte.
Cobrem as regras que os oficiais não exercitam: operadores (SEM004),
break/continue/return fora de contexto (SEM010), categoria (SEM014), divisão
por zero constante (SEM015), tamanho e limites de vetor (SEM016), `for`,
`print`, `read`, `char`, cascata e os erros léxico/sintático antes da
semântica.

**3. Equivalência** — a mesma entrada nas duas implementações tem de produzir
stdout, stderr e código de saída idênticos. Além das entradas das suítes 1 e
2, roda os 50 casos oficiais do parser e os casos próprios da etapa 2: são
mais 67 programas passando pelo pipeline completo.
"""

import argparse
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
AQUI = Path(__file__).resolve().parent
OFICIAIS = RAIZ / "ref" / "testes-oficiais" / "testes-semanticos-20"
PROPRIOS = AQUI / "semantic_inputs"
ESPERADO = AQUI / "semantic_expected"
CASOS_PARSER = RAIZ / "ref" / "testes-oficiais" / "testes-parser-50" / "casos"
PROPRIOS_PARSER = AQUI / "parser_inputs"
MINIC_PY = RAIZ / "minic.py"
MINIC_C_FONTE = RAIZ / "minic.c"
# Sem extensão, como no Linux; no Windows o gcc acrescenta ".exe".
MINIC_C_ALVO = RAIZ / "src" / "c" / "minic"


def binario_c() -> Path:
    com_exe = MINIC_C_ALVO.with_suffix(".exe")
    return com_exe if com_exe.exists() else MINIC_C_ALVO


def compilar_c() -> bool:
    """Compila o minic.c da raiz como unidade única, como o script oficial."""
    comando = ["gcc", "-Wall", "-Wextra", "-std=c11", str(MINIC_C_FONTE), "-o", str(MINIC_C_ALVO)]
    processo = subprocess.run(comando, capture_output=True, text=True, cwd=RAIZ)
    if processo.returncode != 0:
        print("falha ao compilar o minic.c:", file=sys.stderr)
        print(processo.stdout + processo.stderr, file=sys.stderr)
        return False
    if processo.stderr.strip():
        print("avisos do compilador:", file=sys.stderr)
        print(processo.stderr, file=sys.stderr)
    return True


def rodar(comando: list[str], entrada: Path) -> tuple[int, bytes, bytes]:
    """Roda como processo à parte e devolve **bytes** (os CRLF importam)."""
    processo = subprocess.run(comando + [str(entrada)], capture_output=True, cwd=RAIZ)
    return processo.returncode, processo.stdout, processo.stderr


def mostrar(dados: bytes) -> str:
    return dados.decode("utf-8", errors="replace").replace("\r\n", "⏎\n     ")


# -- suíte 1: casos oficiais -------------------------------------------------


def suite_oficial(nome: str, comando: list[str], verboso: bool) -> tuple[int, int]:
    print(f"\n== 20 casos oficiais — {nome} " + "=" * max(0, 36 - len(nome)))
    entradas = sorted(OFICIAIS.glob("*.c"))
    ok = 0
    for entrada in entradas:
        esperado = entrada.with_suffix(".gabarito").read_bytes()
        codigo, stdout, stderr = rodar(comando, entrada)
        aceito = esperado.endswith("programa aceito.".encode())
        problemas = []
        if stdout != esperado:
            problemas.append(
                f"stdout diverge\n     esperado: {mostrar(esperado)}\n     obtido:   {mostrar(stdout)}"
            )
        if stderr:
            problemas.append(f"stderr deveria estar vazio: {mostrar(stderr)}")
        if codigo != (0 if aceito else 4):
            problemas.append(f"exit {codigo} != {0 if aceito else 4}")
        if problemas:
            print(f"[FALHA] {entrada.name}\n     " + "\n     ".join(problemas))
        else:
            ok += 1
            print(f"[OK  ] {entrada.name}" + (f"  exit={codigo}" if verboso else ""))
    print(f"\n{nome} — oficiais: {ok}/{len(entradas)}")
    return ok, len(entradas)


# -- suíte 2: casos próprios (golden files) ----------------------------------


def golden(nome: str):
    return (
        ESPERADO / f"{nome}.stdout.txt",
        ESPERADO / f"{nome}.stderr.txt",
        ESPERADO / f"{nome}.exit.txt",
    )


def suite_propria(nome: str, comando: list[str], verboso: bool, update: bool):
    entradas = sorted(PROPRIOS.glob("*.c"))
    print(f"\n== casos próprios — {nome} " + "=" * max(0, 41 - len(nome)))
    ESPERADO.mkdir(exist_ok=True)
    ok = 0
    for entrada in entradas:
        codigo, stdout, stderr = rodar(comando, entrada)
        p_out, p_err, p_exit = golden(entrada.stem)

        if update:
            p_out.write_bytes(stdout)
            p_err.write_bytes(stderr)
            p_exit.write_bytes(f"{codigo}\n".encode())
            print(f"[gravado] {entrada.name}")
            ok += 1
            continue

        if not (p_out.exists() and p_err.exists() and p_exit.exists()):
            print(f"[FALHA] {entrada.name}: golden ausente — rode com --update")
            continue

        problemas = []
        if stdout != p_out.read_bytes():
            problemas.append(f"stdout diverge: {mostrar(stdout)}")
        if stderr != p_err.read_bytes():
            problemas.append(f"stderr diverge: {mostrar(stderr)}")
        esperado_codigo = int(p_exit.read_text(encoding="utf-8").strip())
        if codigo != esperado_codigo:
            problemas.append(f"exit {codigo} != {esperado_codigo}")

        if problemas:
            print(f"[FALHA] {entrada.name}\n     " + "\n     ".join(problemas))
        else:
            ok += 1
            print(f"[OK  ] {entrada.name}" + (f"  exit={codigo}" if verboso else ""))

    print(f"\n{nome} — próprios: {ok}/{len(entradas)}")
    return ok, len(entradas)


# -- suíte 3: equivalência entre as duas implementações ----------------------


def suite_equivalencia(comando_py: list[str], comando_c: list[str], verboso: bool):
    entradas = sorted(OFICIAIS.glob("*.c")) + sorted(PROPRIOS.glob("*.c"))
    entradas += [caso / "codigo.c" for caso in sorted(CASOS_PARSER.iterdir())]
    entradas += sorted(PROPRIOS_PARSER.glob("*.c"))
    print("\n== equivalência Python <-> C " + "=" * 34)
    ok = 0
    for entrada in entradas:
        py = rodar(comando_py, entrada)
        c = rodar(comando_c, entrada)
        rotulo = f"{entrada.parent.name}/{entrada.name}"
        if py == c:
            ok += 1
            if verboso:
                print(f"[OK  ] {rotulo}")
        else:
            print(f"[FALHA] {rotulo}")
            for nome, indice in (("exit", 0), ("stdout", 1), ("stderr", 2)):
                if py[indice] != c[indice]:
                    print(f"     {nome}: python={py[indice]!r:.160}\n     {' ' * len(nome)}  c=     {c[indice]!r:.160}")
    print(f"\nEquivalência: {ok}/{len(entradas)} entradas com saída idêntica")
    return ok, len(entradas)


def main(argv: list[str]) -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    cli = argparse.ArgumentParser(description="Testes do analisador semântico do MINIC.")
    cli.add_argument("--python", action="store_true", help="roda só a versão Python")
    cli.add_argument("--c", action="store_true", help="roda só a versão C")
    cli.add_argument(
        "--update",
        action="store_true",
        help="regrava os golden dos casos próprios (sempre a partir da versão Python)",
    )
    cli.add_argument("-v", "--verbose", action="store_true", help="detalha cada caso")
    args = cli.parse_args(argv[1:])

    rodar_py = args.python or not args.c
    rodar_c = args.c or not args.python
    comando_py = [sys.executable, str(MINIC_PY)]

    if args.update:
        suite_propria("Python", comando_py, args.verbose, update=True)
        print("\nGolden regravados. Revise o `git diff` de tests/semantic_expected/.")
        return 0

    resultados = []
    if rodar_py:
        resultados.append(suite_oficial("Python", comando_py, args.verbose))
        resultados.append(suite_propria("Python", comando_py, args.verbose, False))
    comando_c = None
    if rodar_c:
        if not compilar_c():
            return 1
        comando_c = [str(binario_c())]
        resultados.append(suite_oficial("C", comando_c, args.verbose))
        resultados.append(suite_propria("C", comando_c, args.verbose, False))
    if rodar_py and rodar_c:
        resultados.append(suite_equivalencia(comando_py, comando_c, args.verbose))

    total_ok = sum(ok for ok, _ in resultados)
    total = sum(n for _, n in resultados)
    print("\n" + "=" * 62)
    print(f"Total: {total_ok}/{total} verificações conformes.")
    return 0 if total_ok == total else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
