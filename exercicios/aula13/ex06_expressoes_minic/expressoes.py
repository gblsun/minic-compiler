"""Exercício 06 — AST de expressões MINIC respeitando precedência.

Lê uma ou mais expressões separadas por `;` e imprime a AST de cada uma, em
notação prefixa totalmente parentizada. O parser (com as ações que constroem a
árvore) está em expr.py; aqui fica só a CLI.

    a + b * c;     ->  (+ a (* b c))

Com `--pos`, cada folha sai com a posição de origem: `(+ a@1:1 (* b@1:5 c@1:9))`.

Uso:  python expressoes.py [--pos] <arquivo | ->
"""

import sys

import expr as E
from expr import F


def main(argv: list[str]) -> int:
    F.configurar_saida()
    pos = "--pos" in argv[1:]
    args = [a for a in argv[1:] if a != "--pos"]
    if len(args) != 1:
        print("uso: python expressoes.py [--pos] <arquivo | ->", file=sys.stderr)
        return F.ERRO_USO
    fl, codigo = F.abrir(args[0])
    if fl is None:
        return codigo
    parser = E.ExprParser(fl)
    try:
        while not fl.verifica("EOF"):
            arvore = parser.parse_expr()
            fl.exige("SEMICOLON", "operador ou ';'")
            print(E.sexpr(arvore, pos))
    except F.ErroSintaxe as erro:
        print(erro, file=sys.stderr)
        return F.ERRO_SINTAXE
    return F.OK


if __name__ == "__main__":
    sys.exit(main(sys.argv))
