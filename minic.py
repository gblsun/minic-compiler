#!/usr/bin/env python3
"""Ponto de entrada do compilador MINIC (até a análise semântica), como a AP 2 pede.

    python minic.py codigo.c

O código de verdade mora em `src/python/` (`lexer.py`, `parser.py`,
`semantico.py` e `tabela_simbolos.py`, ligados por `minic.py`); este arquivo
existe só para que o comando acima funcione a partir da raiz do repositório,
que é o que o script `testes_semanticos_py.sh` do professor chama.
"""

import importlib.util
import sys
from pathlib import Path

# Carrega src/python/minic.py pelo caminho do arquivo, e não pelo nome
# `minic` (que é este mesmo arquivo), como o parser.py da raiz faz.
_ARQUIVO = Path(__file__).resolve().parent / "src" / "python" / "minic.py"
_spec = importlib.util.spec_from_file_location("minic_cli", _ARQUIVO)
_modulo = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_modulo)

if __name__ == "__main__":
    sys.exit(_modulo.main(sys.argv))
