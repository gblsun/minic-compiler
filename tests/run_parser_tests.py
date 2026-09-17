"""Testa o analisador sintático: 50 casos oficiais + casos próprios + equivalência.

    python tests/run_parser_tests.py              # tudo, nas duas implementações
    python tests/run_parser_tests.py --python     # só a versão Python
    python tests/run_parser_tests.py --c          # só a versão C
    python tests/run_parser_tests.py --update     # regrava os golden dos casos próprios
    python tests/run_parser_tests.py -v           # detalha cada caso

Três suítes, com papéis diferentes:

**1. Casos oficiais** — `ref/testes-oficiais/testes-parser-50/casos/` (material do
professor, não editado):

- **01–25 (ACEITO)**: o parser precisa sair com código 0, imprimir uma AST em
  stdout, nada em stderr, e a AST tem de ser igual à de `ast.esperada.txt`.
- **26–50 (REJEITADO)**: precisa sair com código 3, **não** imprimir AST e
  emitir pelo menos um diagnóstico começando com "Erro sintático".

Por que a comparação normaliza espaços: os arquivos oficiais escrevem o mesmo
construto de duas formas diferentes em casos diferentes (`VarDecl(int x =
Lit(int,42))` no caso 02 e `VarDecl(int i=Lit(int,0))` no caso 10), então
nenhum impressor determinístico bate byte a byte com todos. A normalização
remove espaços fora de lexemas — a estrutura da árvore, que é o que está sendo
avaliado, continua sendo comparada exatamente. Ver
`ref/testes-oficiais/README.md` para o levantamento completo dos defeitos do
pacote. Exceção conhecida: o caso 24 tem a AST esperada com um `)` faltando (o
`While` fecha o `Block` antes do `Return`), então é comparado contra a árvore
corrigida — e o relatório diz isso em voz alta, em vez de esconder.

**2. Casos próprios** — `tests/parser_inputs/*.c`, no mesmo formato golden da
etapa 1 (`tests/parser_expected/<nome>.{stdout,stderr,exit}.txt`), comparados
byte a byte. Cobrem o que os oficiais não exercitam: `for`, `print`, `read`,
`break`, `continue`, literais de caractere e cadeia, declaração múltipla,
parâmetro vetor, `else` pendente, comando vazio, recuperação com vários erros
numa mesma entrada e a interação com erro léxico (código 2).

**3. Equivalência** — a mesma entrada nas duas implementações tem de produzir
stdout, stderr e código de saída **idênticos**, byte a byte. É o requisito de
"Python e C equivalentes" verificado na prática, em todas as entradas das duas
suítes anteriores.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
AQUI = Path(__file__).resolve().parent
CASOS = RAIZ / "ref" / "testes-oficiais" / "testes-parser-50" / "casos"
PROPRIOS = AQUI / "parser_inputs"
ESPERADO = AQUI / "parser_expected"
PARSER_PY = RAIZ / "parser.py"
PARSER_C_FONTE = RAIZ / "parser.c"
# No Windows o gcc acrescenta ".exe" ao nome do executável; guardamos o alvo
# sem extensão (como no Linux) e descobrimos depois qual arquivo foi gerado.
PARSER_C_ALVO = RAIZ / "src" / "c" / "parser"

# Caso 24: AST oficial com parêntese faltando. O texto abaixo é o mesmo
# arquivo com o `)` que falta acrescentado — a estrutura que o próprio caso
# descreve (o `Return` pertence ao corpo da função, depois do `While`).
CORRECOES = {
    "24": (
        "Program(Function(int main() Block(VarDecl(int i=Lit(int,0)), "
        "While(Binary(||,Binary(<,Id(i),Lit(int,3)),Binary(==,Id(i),Lit(int,9))),"
        "Block(ExprStmt(Assign(Id(i),Binary(+,Id(i),Lit(int,1)))))), "
        "Return(Id(i)))))"
    )
}


def normalizar(texto: str) -> str:
    """Remove espaços em branco que estão fora de lexemas entre aspas.

    Preserva o conteúdo de literais de caractere e de cadeia (onde um espaço é
    significativo) e descarta todo o resto: assim `If(c, t, e)` e `If(c,t,e)`
    passam a ser o mesmo texto, mas `Lit(string,"a b")` não é achatado.
    """
    saida = []
    aspas = None
    for ch in texto:
        if aspas:
            saida.append(ch)
            if ch == aspas:
                aspas = None
            continue
        if ch in "\"'":
            aspas = ch
            saida.append(ch)
            continue
        if not ch.isspace():
            saida.append(ch)
    return "".join(saida)


def binario_c() -> Path:
    """Caminho do executável gerado (com ou sem `.exe`)."""
    com_exe = PARSER_C_ALVO.with_suffix(".exe")
    return com_exe if com_exe.exists() else PARSER_C_ALVO


def compilar_parser_c() -> bool:
    """Compila o parser em C como unidade única, igual ao script do professor."""
    PARSER_C_ALVO.parent.mkdir(parents=True, exist_ok=True)
    comando = [
        "gcc",
        "-Wall",
        "-Wextra",
        "-std=c11",
        str(PARSER_C_FONTE),
        "-o",
        str(PARSER_C_ALVO),
    ]
    processo = subprocess.run(comando, capture_output=True, text=True, cwd=RAIZ)
    if processo.returncode != 0:
        print("falha ao compilar o parser em C:", file=sys.stderr)
        print(processo.stdout + processo.stderr, file=sys.stderr)
        return False
    if processo.stderr.strip():
        print("avisos do compilador:", file=sys.stderr)
        print(processo.stderr, file=sys.stderr)
    return True


def rodar(comando: list[str], entrada: Path):
    """Roda o parser como processo à parte, como um usuário rodaria."""
    processo = subprocess.run(
        comando + [str(entrada)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        cwd=RAIZ,
    )
    return processo.returncode, processo.stdout, processo.stderr


# -- suíte 1: casos oficiais ---------------------------------------------


def esperado_do_caso(caso: Path) -> tuple[str, str]:
    """Devolve `(status, ast_esperada)` — status é ACEITO ou REJEITADO."""
    resultado = (caso / "resultado.esperado.txt").read_text(encoding="utf-8")
    status = resultado.strip().splitlines()[0].strip()
    ast = (caso / "ast.esperada.txt").read_text(encoding="utf-8").strip()
    numero = caso.name[:2]
    if numero in CORRECOES:
        ast = CORRECOES[numero]
    return status, ast


def verificar_oficial(caso: Path, comando: list[str]) -> tuple[bool, str]:
    status, ast_esperada = esperado_do_caso(caso)
    codigo, stdout, stderr = rodar(comando, caso / "codigo.c")
    stdout, stderr = stdout.strip(), stderr.strip()

    if status == "ACEITO":
        if codigo != 0:
            return False, f"esperava aceitar (código 0), saiu {codigo}: {stderr[:80]}"
        if stderr:
            return False, f"stderr deveria estar vazio, veio: {stderr[:80]}"
        if normalizar(stdout) != normalizar(ast_esperada):
            return False, f"AST diferente\n     esperada: {ast_esperada}\n     obtida:   {stdout}"
        return True, "AST conforme"

    if codigo != 3:
        return False, f"esperava rejeitar com código 3, saiu {codigo}"
    if stdout:
        return False, f"não deveria imprimir AST, imprimiu: {stdout[:60]}"
    if not re.search(r"^Erro sintático na linha \d+, coluna \d+:", stderr, re.M):
        return False, f"diagnóstico fora do formato esperado: {stderr[:80]}"
    return True, stderr.splitlines()[0]


def suite_oficial(nome: str, comando: list[str], verboso: bool) -> tuple[int, int]:
    print(f"\n== 50 casos oficiais — {nome} " + "=" * max(0, 36 - len(nome)))
    casos = sorted(CASOS.iterdir())
    ok = 0
    for caso in casos:
        passou, explicacao = verificar_oficial(caso, comando)
        ok += passou
        marca = "OK  " if passou else "FALHA"
        nota = " (AST oficial corrigida)" if caso.name[:2] in CORRECOES else ""
        linha = f"[{marca}] {caso.name[:2]} {caso.name[3:]}{nota}"
        print(linha if passou and not verboso else f"{linha}\n     {explicacao}")
    print(f"\n{nome} — oficiais: {ok}/{len(casos)}")
    return ok, len(casos)


# -- suíte 2: casos próprios (golden files) ------------------------------


def golden(nome: str):
    return (
        ESPERADO / f"{nome}.stdout.txt",
        ESPERADO / f"{nome}.stderr.txt",
        ESPERADO / f"{nome}.exit.txt",
    )


def suite_propria(nome: str, comando: list[str], verboso: bool, update: bool):
    entradas = sorted(PROPRIOS.glob("*.c"))
    print(f"\n== casos próprios — {nome} " + "=" * max(0, 41 - len(nome)))
    ok = 0
    for entrada in entradas:
        codigo, stdout, stderr = rodar(comando, entrada)
        p_out, p_err, p_exit = golden(entrada.stem)

        if update:
            p_out.write_text(stdout, encoding="utf-8")
            p_err.write_text(stderr, encoding="utf-8")
            p_exit.write_text(f"{codigo}\n", encoding="utf-8")
            print(f"[gravado] {entrada.name}")
            ok += 1
            continue

        if not (p_out.exists() and p_err.exists() and p_exit.exists()):
            print(f"[FALHA] {entrada.name}: golden ausente — rode com --update")
            continue

        problemas = []
        if stdout != p_out.read_text(encoding="utf-8"):
            problemas.append("stdout diverge")
        if stderr != p_err.read_text(encoding="utf-8"):
            problemas.append("stderr diverge")
        esperado_codigo = int(p_exit.read_text(encoding="utf-8").strip())
        if codigo != esperado_codigo:
            problemas.append(f"exit {codigo} != {esperado_codigo}")

        if problemas:
            print(f"[FALHA] {entrada.name}: {'; '.join(problemas)}")
        else:
            ok += 1
            linha = f"[OK  ] {entrada.name}"
            print(f"{linha}\n     exit={codigo}" if verboso else linha)

    print(f"\n{nome} — próprios: {ok}/{len(entradas)}")
    return ok, len(entradas)


# -- suíte 3: equivalência entre as duas implementações ------------------


def suite_equivalencia(comando_py: list[str], comando_c: list[str], verboso: bool):
    entradas = [caso / "codigo.c" for caso in sorted(CASOS.iterdir())]
    entradas += sorted(PROPRIOS.glob("*.c"))
    print("\n== equivalência Python <-> C " + "=" * 34)
    ok = 0
    for entrada in entradas:
        py = rodar(comando_py, entrada)
        c = rodar(comando_c, entrada)
        if py == c:
            ok += 1
            if verboso:
                print(f"[OK  ] {entrada.parent.name}/{entrada.name}")
        else:
            print(f"[FALHA] {entrada.parent.name}/{entrada.name}")
            for rotulo, indice in (("exit", 0), ("stdout", 1), ("stderr", 2)):
                if py[indice] != c[indice]:
                    print(f"     {rotulo}: python={py[indice]!r:.120} c={c[indice]!r:.120}")
    print(f"\nEquivalência: {ok}/{len(entradas)} entradas com saída idêntica")
    return ok, len(entradas)


def main(argv: list[str]) -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    cli = argparse.ArgumentParser(description="Testes do parser do MINIC.")
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
    comando_py = [sys.executable, str(PARSER_PY)]

    if args.update:
        # A versão Python é a autoridade dos golden files, como na etapa 1.
        suite_propria("Python", comando_py, args.verbose, update=True)
        print("\nGolden regravados. Revise o `git diff` de tests/parser_expected/.")
        return 0

    resultados = []
    if rodar_py:
        resultados.append(suite_oficial("Python", comando_py, args.verbose))
        resultados.append(suite_propria("Python", comando_py, args.verbose, False))
    comando_c = None
    if rodar_c:
        if not compilar_parser_c():
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
    print(
        "Nota: o caso oficial 24 é comparado contra a AST corrigida "
        "(o arquivo do pacote tem um ')' faltando)."
    )
    return 0 if total_ok == total else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
