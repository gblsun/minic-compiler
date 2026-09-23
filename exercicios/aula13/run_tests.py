"""Roda os casos de teste dos 10 exercícios da aula 13, em Python e em C.

    python exercicios/aula13/run_tests.py              # tudo
    python exercicios/aula13/run_tests.py ex01 ex04    # só alguns exercícios
    python exercicios/aula13/run_tests.py --python     # só a versão Python
    python exercicios/aula13/run_tests.py --c          # só a versão C
    python exercicios/aula13/run_tests.py --update     # regrava os .esperado a partir do Python
    python exercicios/aula13/run_tests.py -v           # mostra o diff das falhas

Cada caso fica em `exNN_*/casos/`:

- `<nome>.entrada`  — o arquivo passado ao programa;
- `<nome>.args`     — opcional: argumentos extras (uma linha, ex.: `--trace`);
- `<nome>.esperado` — stdout, stderr e código de saída esperados, no formato

      <stdout>
      --- stderr ---
      <stderr>
      --- exit: N ---

As duas versões são comparadas com **o mesmo** `.esperado`, então um caso só
passa nas duas se elas concordarem byte a byte — é o teste diferencial que a
atividade pede. O C é compilado com `gcc -Wall -Wextra -std=c11`; os avisos do
compilador contam como falha.
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

AQUI = Path(__file__).resolve().parent
RAIZ = AQUI.parents[1]
BUILD = AQUI / "build"
LEXER_C = RAIZ / "src" / "c" / "lexer.c"
FLUXO_C = AQUI / "comum" / "fluxo.c"

# nome -> (diretório, script Python, fontes C além do lexer/fluxo)
EXERCICIOS = {
    "ex01": ("ex01_calculadora", "calculadora.py", ["calculadora.c"]),
    "ex02": ("ex02_grafo_dependencias", "grafo.py", ["grafo.c"]),
    "ex03": ("ex03_ast_s_atribuida", "ast_s.py", ["ast_s.c"]),
    "ex04": ("ex04_tipo_herdado", "declaracoes.py", ["declaracoes.c"]),
    "ex05": ("ex05_escopos", "escopos.py", ["escopos.c"]),
    "ex06": ("ex06_expressoes_minic", "expressoes.py", ["expr.c", "expressoes.c"]),
    "ex07": ("ex07_comandos", "comandos.py", ["../ex06_expressoes_minic/expr.c", "comandos.c"]),
    "ex08": ("ex08_tipos", "tipos.py", ["../ex06_expressoes_minic/expr.c", "tipos.c"]),
    "ex09": ("ex09_erros", "erros.py", ["../ex06_expressoes_minic/expr.c", "erros.c"]),
    "ex10": ("ex10_pipeline", "parser.py", ["scanner.c", "ast.c", "parser.c", "main.c"]),
}
# O ex02 não lê código MINIC (lê a descrição de um grafo), então não usa o lexer;
# o ex10 usa o lexer por meio do próprio módulo scanner.c, e não do comum/fluxo.c.
SEM_LEXER = {"ex02"}
SEM_FLUXO = {"ex02", "ex10"}


def formatar(stdout: str, stderr: str, codigo: int) -> str:
    return f"{stdout}--- stderr ---\n{stderr}--- exit: {codigo} ---\n"


def compilar(nome: str) -> tuple[Path | None, str]:
    """Compila o exercício; devolve (executável, mensagem de erro)."""
    pasta, _, fontes = EXERCICIOS[nome]
    BUILD.mkdir(exist_ok=True)
    exe = BUILD / nome
    arquivos = [str((AQUI / pasta / f).resolve()) for f in fontes]
    if nome not in SEM_FLUXO:
        arquivos.append(str(FLUXO_C))
    if nome not in SEM_LEXER:
        arquivos.append(str(LEXER_C))
    cmd = ["gcc", "-Wall", "-Wextra", "-std=c11", "-o", str(exe), *arquivos]
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode != 0 or p.stderr.strip():
        return None, p.stderr.strip() or "falha na compilação"
    if not exe.exists() and exe.with_suffix(".exe").exists():
        exe = exe.with_suffix(".exe")
    return exe, ""


def casos(nome: str):
    pasta = AQUI / EXERCICIOS[nome][0] / "casos"
    return sorted(pasta.glob("*.entrada"))


def extras(entrada: Path) -> list[str]:
    args = entrada.with_suffix(".args")
    return args.read_text(encoding="utf-8").split() if args.exists() else []


def diff(esperado: str, obtido: str) -> str:
    import difflib

    linhas = difflib.unified_diff(
        esperado.splitlines(), obtido.splitlines(), "esperado", "obtido", lineterm=""
    )
    return "\n".join("      " + l for l in linhas)


def main() -> int:
    cli = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    cli.add_argument("exercicios", nargs="*", help="ex01 ... ex10 (padrão: todos)")
    cli.add_argument("--python", action="store_true", help="só a versão Python")
    cli.add_argument("--c", action="store_true", help="só a versão C")
    cli.add_argument("--update", action="store_true", help="regrava os .esperado (pelo Python)")
    cli.add_argument("-v", "--verbose", action="store_true", help="mostra o diff das falhas")
    opts = cli.parse_args()

    nomes = opts.exercicios or list(EXERCICIOS)
    desconhecidos = [n for n in nomes if n not in EXERCICIOS]
    if desconhecidos:
        cli.error(f"exercício desconhecido: {', '.join(desconhecidos)}")

    usar_py = not opts.c or opts.python
    usar_c = (not opts.python or opts.c) and not opts.update
    if usar_c and shutil.which("gcc") is None:
        print("AVISO: gcc não encontrado — a versão C não foi testada.")
        usar_c = False

    total = aprovados = 0
    for nome in nomes:
        pasta, script, _ = EXERCICIOS[nome]
        print(f"\n== {pasta} " + "=" * max(0, 50 - len(pasta)))
        exe = None
        if usar_c:
            exe, erro = compilar(nome)
            total += 1
            if exe is None:
                print(f"[FALHA] compilação C\n{erro}")
            else:
                aprovados += 1
        for entrada in casos(nome):
            argv = [*extras(entrada), str(entrada.relative_to(AQUI / pasta))]
            esperado_arq = entrada.with_suffix(".esperado")
            linguagens = []
            if usar_py:
                linguagens.append(("py", [sys.executable, str(AQUI / pasta / script)]))
            if usar_c and exe is not None:
                linguagens.append(("c ", [str(exe)]))
            for rotulo, base in linguagens:
                obtido = subprocess_em(pasta, base + argv)
                if opts.update:
                    esperado_arq.write_text(obtido, encoding="utf-8", newline="\n")
                    print(f"[gravado] {entrada.stem}")
                    continue
                total += 1
                esperado = (
                    esperado_arq.read_text(encoding="utf-8") if esperado_arq.exists() else None
                )
                if esperado == obtido:
                    aprovados += 1
                    print(f"[OK   {rotulo}] {entrada.stem}")
                else:
                    motivo = "sem .esperado" if esperado is None else "saída diferente"
                    print(f"[FALHA {rotulo}] {entrada.stem} ({motivo})")
                    if opts.verbose and esperado is not None:
                        print(diff(esperado, obtido))

    if opts.update:
        return 0
    print(f"\nTotal: {aprovados}/{total} verificações aprovadas.")
    return 0 if aprovados == total else 1


def subprocess_em(pasta: str, comando: list[str]) -> str:
    """Roda o comando a partir da pasta do exercício (caminhos de caso relativos)."""
    p = subprocess.run(comando, capture_output=True, cwd=AQUI / pasta)
    decodifica = lambda b: b.decode("utf-8", errors="replace").replace("\r\n", "\n")
    return formatar(decodifica(p.stdout), decodifica(p.stderr), p.returncode)


if __name__ == "__main__":
    sys.exit(main())
