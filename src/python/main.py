"""CLI mínima do analisador léxico MINIC: `python main.py <arquivo.mc>`.

Imprime os tokens no formato <TOKEN, lexema> / TOKEN (ver docs/tokens.md) e os erros
léxicos no formato da Seção 12 da especificação. Código de saída 2 em erro léxico,
seguindo a Seção 11.1.
"""

import sys
from pathlib import Path

from lexer import Lexer


def main(argv: list[str]) -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")

    if len(argv) != 2:
        print("uso: python main.py <arquivo.mc>", file=sys.stderr)
        return 1

    path = Path(argv[1])
    try:
        source = path.read_text(encoding="utf-8")
    except OSError as exc:
        print(f"erro ao abrir '{path}': {exc}", file=sys.stderr)
        return 1

    tokens, errors = Lexer(source).tokenize()

    for token in tokens:
        if token.type != "EOF":
            print(token)

    for error in errors:
        print(error, file=sys.stderr)

    return 2 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
