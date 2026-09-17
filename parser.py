#!/usr/bin/env python3
"""Ponto de entrada do analisador sintático do MINIC, como o enunciado pede.

    python parser.py codigo.c

O código de verdade mora em `src/python/` (`parser.py` e `minic_ast.py`, que
reaproveitam o `lexer.py` da etapa 1); este arquivo existe só para que o
comando acima funcione a partir da raiz do repositório, que é como a atividade
manda chamar o parser — e é o que o script `testar_parser_python.sh` do
professor espera encontrar.

Aceita os mesmos argumentos do módulo (`--tree`, `--tokens`, `--help`).
"""

import sys
from pathlib import Path

# Coloca src/python/ na frente do sys.path para que `from parser import main`
# resolva para src/python/parser.py (e não para este arquivo, que roda como
# __main__ e por isso não ocupa o nome "parser" em sys.modules).
sys.path.insert(0, str(Path(__file__).resolve().parent / "src" / "python"))

from parser import main  # noqa: E402  (precisa vir depois do ajuste de sys.path)

if __name__ == "__main__":
    sys.exit(main(sys.argv))
