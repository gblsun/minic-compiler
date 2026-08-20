"""Executa o lexer Python sobre cada tests/inputs/*.mc e compara com tests/expected/.

Uso:
    python tests/run_tests.py            # roda os testes e compara com o esperado
    python tests/run_tests.py --update   # roda e (re)grava a saída atual como esperada
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LEXER_MAIN = ROOT / "src" / "python" / "main.py"
INPUTS_DIR = Path(__file__).resolve().parent / "inputs"
EXPECTED_DIR = Path(__file__).resolve().parent / "expected"


def run_lexer(input_path: Path):
    result = subprocess.run(
        [sys.executable, str(LEXER_MAIN), str(input_path)],
        capture_output=True,
        encoding="utf-8",
    )
    return result.stdout, result.stderr, result.returncode


def expected_paths(name: str):
    return (
        EXPECTED_DIR / f"{name}.stdout.txt",
        EXPECTED_DIR / f"{name}.stderr.txt",
        EXPECTED_DIR / f"{name}.exit.txt",
    )


def main():
    update = "--update" in sys.argv[1:]
    inputs = sorted(INPUTS_DIR.glob("*.mc"))
    if not inputs:
        print("nenhum teste em tests/inputs/*.mc")
        return 1

    failures = []
    for input_path in inputs:
        name = input_path.stem
        stdout, stderr, exit_code = run_lexer(input_path)
        stdout_path, stderr_path, exit_path = expected_paths(name)

        if update:
            stdout_path.write_text(stdout, encoding="utf-8")
            stderr_path.write_text(stderr, encoding="utf-8")
            exit_path.write_text(f"{exit_code}\n", encoding="utf-8")
            print(f"[gravado] {name}")
            continue

        if not (stdout_path.exists() and stderr_path.exists() and exit_path.exists()):
            failures.append((name, "saída esperada ausente — rode com --update"))
            print(f"[FALHA] {name}: saída esperada ausente")
            continue

        expected_stdout = stdout_path.read_text(encoding="utf-8")
        expected_stderr = stderr_path.read_text(encoding="utf-8")
        expected_exit = int(exit_path.read_text(encoding="utf-8").strip())

        ok = True
        details = []
        if stdout != expected_stdout:
            ok = False
            details.append("stdout diverge")
        if stderr != expected_stderr:
            ok = False
            details.append("stderr diverge")
        if exit_code != expected_exit:
            ok = False
            details.append(f"exit code {exit_code} != esperado {expected_exit}")

        if ok:
            print(f"[OK] {name}")
        else:
            failures.append((name, "; ".join(details)))
            print(f"[FALHA] {name}: {'; '.join(details)}")

    if update:
        return 0

    print()
    if failures:
        print(f"{len(failures)}/{len(inputs)} teste(s) falharam:")
        for name, reason in failures:
            print(f"  - {name}: {reason}")
        return 1

    print(f"{len(inputs)}/{len(inputs)} testes passaram.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
