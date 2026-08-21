"""CLI mínima do analisador léxico MINIC: `python main.py <arquivo.mc>`.

Imprime os tokens no formato <TOKEN, lexema> / TOKEN (ver docs/tokens.md) e os erros
léxicos no formato da Seção 12 da especificação. Código de saída 2 em erro léxico,
seguindo a Seção 11.1.

Este arquivo é só a "casca" de linha de comando: ler o arquivo, chamar o
lexer e formatar a saída. Toda a lógica de reconhecimento de tokens mora em
lexer.py — de propósito, para que o lexer possa ser reaproveitado por outras
interfaces (como a versão Streamlit em app_streamlit.py) sem depender de
nada relacionado a terminal/stdin/stdout.
"""

import sys
from pathlib import Path

from lexer import Lexer


def main(argv: list[str]) -> int:
    # No Windows o console às vezes usa uma codificação diferente de UTF-8
    # por padrão (cp1252/cp850), o que quebraria a impressão de acentos nas
    # mensagens de erro em português. Forçamos UTF-8 explicitamente em
    # stdout/stderr para o programa se comportar igual em qualquer SO.
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")

    # argv[0] é o nome do script; o programa espera exatamente um argumento
    # depois dele, o caminho do arquivo .mc a analisar.
    if len(argv) != 2:
        print("uso: python main.py <arquivo.mc>", file=sys.stderr)
        return 1

    path = Path(argv[1])
    try:
        source = path.read_text(encoding="utf-8")
    except OSError as exc:
        # Arquivo inexistente, sem permissão de leitura, etc. Isso é um erro
        # de uso do programa (código 1), diferente de um erro léxico dentro
        # de um arquivo válido (código 2, tratado mais abaixo).
        print(f"erro ao abrir '{path}': {exc}", file=sys.stderr)
        return 1

    tokens, errors = Lexer(source).tokenize()

    # O token EOF é um detalhe interno do lexer (marca o fim do fluxo de
    # tokens para etapas futuras do compilador) e não faz parte da saída que
    # a especificação pede, então ele é filtrado aqui.
    for token in tokens:
        if token.type != "EOF":
            print(token)

    # Erros léxicos vão para stderr, separados dos tokens (que vão para
    # stdout) — assim dá para redirecionar cada fluxo separadamente, e os
    # testes em tests/run_tests.py comparam os dois de forma independente.
    for error in errors:
        print(error, file=sys.stderr)

    # Seção 11.1 da especificação: 0 quando não há erro léxico, 2 quando há.
    # (1 fica reservado para os erros de uso tratados acima.)
    return 2 if errors else 0


if __name__ == "__main__":
    # sys.exit propaga o código de retorno de main() para o shell/CI,
    # permitindo que scripts de teste verifiquem o exit code do processo.
    sys.exit(main(sys.argv))
