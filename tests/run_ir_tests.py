"""Testa a IR (-O0) e o otimizador (-O1): 30 casos oficiais + próprios + equivalência.

    python tests/run_ir_tests.py              # tudo, nas duas implementações
    python tests/run_ir_tests.py --python     # só a versão Python
    python tests/run_ir_tests.py --c          # só a versão C
    python tests/run_ir_tests.py --update     # regrava os golden dos casos próprios
    python tests/run_ir_tests.py -v           # detalha cada caso

**1. Casos oficiais** — `ref/testes-oficiais/testes-ir-30/`. O README do
pacote diz que os gabaritos são "uma IR canônica de referência, não uma
exigência de igualdade byte a byte": comparam-se estrutura e semântica,
normalizando temporários e rótulos. É o que esta suíte faz:

- **P01–P10** (válidos, -O0): código de saída 0; a IR, com temporários,
  rótulos e sufixos de sombreamento (`x#1`) renumerados por ordem de
  aparição, é comparada com a IR do gabarito — `idêntica` quando bate linha a
  linha, `equivalente` quando não bate mas, executada no interpretador
  (`tests/ir_interp.py`), imprime o mesmo que a IR do gabarito. Também a IR
  -O1 tem de imprimir o mesmo.
- **E01–E10** (inválidos): código de saída igual à categoria do gabarito
  (2 léxico, 3 sintático, 4 semântico) e nenhuma IR em stdout.
- **O01–O10** (otimização): a IR -O1 imprime o mesmo que a -O0 **e** que a
  IR otimizada do gabarito, e tem menos instruções que a -O0 (O09, redução de
  força, é opcional pelo próprio gabarito: lá só se exige equivalência).

**2. Casos próprios** — `tests/ir_inputs/*.c` (entrada de `read` opcional em
`<nome>.in`), golden em `tests/ir_expected/<nome>.O0.ir` e `.O1.ir`, byte a
byte, mais a mesma checagem semântica: -O0 e -O1 imprimem o mesmo.

**3. Equivalência** — C e Python produzem stdout, stderr e código de saída
idênticos com `--ir -O0` e `--ir -O1`, em todas as entradas acima e nas das
suítes semântica e do parser; e, em cada programa válido, a IR -O1 executa
igual à -O0.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

import ir_interp

RAIZ = Path(__file__).resolve().parent.parent
AQUI = Path(__file__).resolve().parent
OFICIAIS = RAIZ / "ref" / "testes-oficiais" / "testes-ir-30" / "testes"
PROPRIOS = AQUI / "ir_inputs"
ESPERADO = AQUI / "ir_expected"
OUTRAS_ENTRADAS = [
    AQUI / "semantic_inputs",
    AQUI / "parser_inputs",
    RAIZ / "ref" / "testes-oficiais" / "testes-semanticos-20",
    RAIZ / "ref" / "testes-oficiais" / "testes-parser-50" / "casos",
]
MINIC_PY = RAIZ / "minic.py"
MINIC_C_FONTE = RAIZ / "minic.c"
# Sem extensão, como no Linux; no Windows o gcc acrescenta ".exe".
MINIC_C_ALVO = RAIZ / "src" / "c" / "minic"


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
    if processo.stderr.strip():
        print("avisos do compilador:", file=sys.stderr)
        print(processo.stderr, file=sys.stderr)
    return True


def rodar(comando: list[str], opcoes: list[str], entrada: Path) -> tuple[int, bytes, bytes]:
    processo = subprocess.run(comando + opcoes + [str(entrada)], capture_output=True, cwd=RAIZ)
    return processo.returncode, processo.stdout, processo.stderr


def executar(texto: str, entradas=()):
    """(linhas impressas, erro de execução ou None) — os dois contam na comparação."""
    try:
        return ir_interp.executar(texto, entradas), None
    except ir_interp.ErroExecucao as exc:
        return None, str(exc)


def contar_instrucoes(texto: str) -> int:
    return sum(1 for linha in texto.splitlines() if linha.startswith("  "))


# -- normalização ------------------------------------------------------------------

NOME = re.compile(r"'(?:\\.|[^'\\])*'|\"(?:\\.|[^\"\\])*\"|[A-Za-z_@][A-Za-z0-9_]*(?:#[A-Za-z0-9_]+)?")


def normalizar(texto: str) -> list[str]:
    """IR com temporários, rótulos e sufixos renumerados por função e por ordem."""
    saida = []
    for linha in texto.splitlines():
        linha = linha.rstrip()
        if not linha:
            continue
        if linha.startswith("func "):
            mapas = {"t": {}, "L": {}, "#": {}}
        else:
            linha = NOME.sub(lambda m: _renomear(m.group(0), mapas), linha)
        saida.append(linha)
    return saida


def _renomear(token: str, mapas: dict) -> str:
    if token[0] in "'\"":
        return token
    if re.fullmatch(r"t[0-9]+", token):
        mapa, prefixo = mapas["t"], "t"
    elif token.startswith("L") and (token.startswith("L_") or token[1:].isdigit()):
        mapa, prefixo = mapas["L"], "L"
    elif "#" in token:
        base = token.split("#")[0]
        mapa = mapas["#"].setdefault(base, {})
        prefixo = base + "#"
    else:
        return token
    if token not in mapa:
        mapa[token] = f"{prefixo}{len(mapa) + 1}"
    return mapa[token]


# -- gabaritos do pacote oficial ---------------------------------------------------------


def bloco(texto: str, comeco: str) -> str:
    """Linhas de IR depois da linha que começa com `comeco`, até o próximo comentário."""
    linhas, dentro = [], False
    for linha in texto.splitlines():
        if linha.startswith(comeco):
            dentro = True
            continue
        if not dentro:
            continue
        if linha.startswith(("func ", "endfunc", "  ")) or re.fullmatch(r"\w+:", linha):
            linhas.append(linha)
        else:
            break
    if linhas and not linhas[0].startswith("func "):
        linhas = ["func main"] + linhas + ["endfunc"]
    return "".join(linha + "\n" for linha in linhas)


def codigo_esperado(gabarito: str):
    m = re.search(r"código de saída (\d+)", gabarito)
    return int(m.group(1)) if m else 0


# -- suíte 1: casos oficiais ------------------------------------------------------------------


def suite_oficial(nome: str, comando: list[str], verboso: bool) -> tuple[int, int]:
    print(f"== IR — casos oficiais ({nome}) ==")
    aprovados = total = 0
    contagem = {"idêntica": 0, "equivalente": 0}
    for fonte in sorted(OFICIAIS.glob("*/*/*.c")):
        total += 1
        caso = fonte.stem
        gabarito = fonte.with_suffix(".txt").read_text(encoding="utf-8")
        esperado = codigo_esperado(gabarito)
        cod0, out0, err0 = rodar(comando, ["--ir", "-O0"], fonte)
        cod1, out1, _ = rodar(comando, ["--ir", "-O1"], fonte)
        ir0, ir1 = out0.decode("utf-8"), out1.decode("utf-8")
        falha, detalhe = None, ""

        if caso.startswith("E"):
            if cod0 != esperado:
                falha = f"código de saída {cod0}, esperado {esperado}"
            elif out0.startswith(b"func") or b"\nfunc " in out0:
                falha = "emitiu IR para um programa inválido"
            else:
                primeira = (err0 or out0).decode("utf-8").splitlines()[0]
                detalhe = f"rejeitado (código {cod0}): {primeira}"
        elif cod0 != 0 or cod1 != 0:
            falha = f"código de saída {cod0}/{cod1}: {err0.decode('utf-8').strip()}"
        elif caso.startswith("P"):
            referencia = bloco(gabarito, "IR canônica esperada")
            exec_ref, exec0, exec1 = executar(referencia), executar(ir0), executar(ir1)
            forma = "idêntica" if normalizar(ir0) == normalizar(referencia) else "equivalente"
            if exec0 != exec_ref:
                falha = f"-O0 executa {exec0}, gabarito executa {exec_ref}"
            elif exec1 != exec0:
                falha = f"-O1 executa {exec1}, -O0 executa {exec0}"
            else:
                contagem[forma] += 1
                detalhe = f"IR {forma} ao gabarito; imprime {' '.join(exec0[0])}"
        else:  # O
            referencia = bloco(gabarito, "IR otimizada")
            exec_ref, exec0, exec1 = executar(referencia), executar(ir0), executar(ir1)
            n0, n1 = contar_instrucoes(ir0), contar_instrucoes(ir1)
            if exec1 != exec0:
                falha = f"-O1 executa {exec1}, -O0 executa {exec0}"
            elif exec1 != exec_ref:
                falha = f"-O1 executa {exec1}, gabarito -O1 executa {exec_ref}"
            elif n1 >= n0:
                falha = f"-O1 não reduziu a IR ({n0} → {n1} instruções)"
            else:
                detalhe = f"{n0} → {n1} instruções; imprime {' '.join(exec1[0])}"

        if falha is None:
            aprovados += 1
            if verboso:
                print(f"  ok    {caso}: {detalhe}")
        else:
            print(f"  FALHA {caso}: {falha}")
    print(
        f"  {aprovados}/{total} aprovados "
        f"(P: {contagem['idêntica']} com IR idêntica ao gabarito, "
        f"{contagem['equivalente']} equivalentes)"
    )
    return aprovados, total


# -- suíte 2: casos próprios ---------------------------------------------------------------------


def entradas_de(fonte: Path) -> list[int]:
    arquivo = fonte.with_suffix(".in")
    return [int(x) for x in arquivo.read_text().split()] if arquivo.exists() else []


def suite_propria(nome: str, comando: list[str], verboso: bool, update: bool) -> tuple[int, int]:
    print(f"== IR — casos próprios ({nome}) ==")
    aprovados = total = 0
    for fonte in sorted(PROPRIOS.glob("*.c")):
        total += 1
        falhas = []
        saidas = {}
        for nivel in ("O0", "O1"):
            codigo, out, err = rodar(comando, ["--ir", f"-{nivel}"], fonte)
            if codigo != 0 or err:
                falhas.append(f"-{nivel}: código {codigo}: {err.decode('utf-8').strip()}")
                continue
            saidas[nivel] = out
            golden = ESPERADO / f"{fonte.stem}.{nivel}.ir"
            if update:
                golden.write_bytes(out)
            elif not golden.exists():
                falhas.append(f"sem golden {golden.name} (rode com --update)")
            elif golden.read_bytes() != out:
                falhas.append(f"-{nivel} difere de {golden.name}")
        if len(saidas) == 2:
            entradas = entradas_de(fonte)
            exec0 = executar(saidas["O0"].decode("utf-8"), entradas)
            exec1 = executar(saidas["O1"].decode("utf-8"), entradas)
            if exec0 != exec1:
                falhas.append(f"-O1 executa {exec1}, -O0 executa {exec0}")
            elif exec0[1] is not None:
                falhas.append(f"erro de execução: {exec0[1]}")
        if falhas:
            print(f"  FALHA {fonte.name}: " + "; ".join(falhas))
        else:
            aprovados += 1
            if verboso:
                n0 = contar_instrucoes(saidas["O0"].decode("utf-8"))
                n1 = contar_instrucoes(saidas["O1"].decode("utf-8"))
                print(f"  ok    {fonte.name}: {n0} → {n1} instruções")
    print(f"  {aprovados}/{total} aprovados" + (" (golden regravados)" if update else ""))
    return aprovados, total


# -- suíte 3: equivalência C × Python e -O0 × -O1 -------------------------------------------------


def todas_as_entradas() -> list[Path]:
    fontes = sorted(OFICIAIS.glob("*/*/*.c")) + sorted(PROPRIOS.glob("*.c"))
    for pasta in OUTRAS_ENTRADAS:
        fontes += sorted(pasta.rglob("*.c"))
    return fontes


def suite_equivalencia(comando_py, comando_c, verboso: bool) -> tuple[int, int]:
    titulo = "C × Python e -O0 × -O1" if comando_c else "-O0 × -O1"
    print(f"== IR — equivalência {titulo} ==")
    aprovados = total = validos = 0
    for fonte in todas_as_entradas():
        total += 1
        falhas = []
        saidas = {}
        for nivel in ("-O0", "-O1"):
            py = rodar(comando_py, ["--ir", nivel], fonte)
            if comando_c:
                c = rodar(comando_c, ["--ir", nivel], fonte)
                if py != c:
                    falhas.append(f"{nivel}: C e Python diferem")
            saidas[nivel] = py
        if saidas["-O0"][0] == 0:
            validos += 1
            entradas = entradas_de(fonte) if fonte.parent == PROPRIOS else []
            ir0, ir1 = (saidas[n][1].decode("utf-8") for n in ("-O0", "-O1"))
            if "read" in ir0 and not entradas:
                entradas = [3, -2, 7, 0, 5] * 20
            exec0, exec1 = executar(ir0, entradas), executar(ir1, entradas)
            if exec0 != exec1:
                falhas.append(f"-O1 executa {exec1}, -O0 executa {exec0}")
        rotulo = fonte.relative_to(RAIZ).as_posix()
        if falhas:
            print(f"  FALHA {rotulo}: " + "; ".join(falhas))
        else:
            aprovados += 1
            if verboso:
                print(f"  ok    {rotulo}")
    print(f"  {aprovados}/{total} aprovados ({validos} programas válidos executados nos dois níveis)")
    return aprovados, total


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    grupo = parser.add_mutually_exclusive_group()
    grupo.add_argument("--python", action="store_true", help="só a versão Python")
    grupo.add_argument("--c", action="store_true", help="só a versão C")
    parser.add_argument("--update", action="store_true", help="regrava os golden próprios")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args(argv)
    # As mensagens têm acentos e setas; no console do Windows o padrão é cp1252.
    sys.stdout.reconfigure(encoding="utf-8")

    comando_py = [sys.executable, str(MINIC_PY)]
    implementacoes = []
    if not args.c:
        implementacoes.append(("Python", comando_py))
    comando_c = None
    if not args.python:
        if not compilar_c():
            return 1
        comando_c = [str(binario_c())]
        implementacoes.append(("C", comando_c))

    resultados = []
    for nome, comando in implementacoes:
        resultados.append(suite_oficial(nome, comando, args.verbose))
        resultados.append(suite_propria(nome, comando, args.verbose, args.update and nome == "Python"))
    if args.c:
        resultados.append(suite_equivalencia(comando_c, None, args.verbose))
    else:
        resultados.append(suite_equivalencia(comando_py, comando_c, args.verbose))

    aprovados = sum(a for a, _ in resultados)
    total = sum(t for _, t in resultados)
    print(f"\nTotal: {aprovados}/{total}")
    return 0 if aprovados == total else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
