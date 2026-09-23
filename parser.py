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

import importlib.util
import sys
from pathlib import Path

# Carrega src/python/parser.py pelo caminho do arquivo, e não pelo nome
# `parser`: assim não há como o import pegar este mesmo arquivo, um módulo
# `parser` de outra origem (em Python <= 3.9 havia um embutido) ou depender do
# diretório de onde o comando foi executado.
_ARQUIVO = Path(__file__).resolve().parent / "src" / "python" / "parser.py"
_spec = importlib.util.spec_from_file_location("minic_parser", _ARQUIVO)
_modulo = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_modulo)

if __name__ == "__main__":
    sys.exit(_modulo.main(sys.argv))
