#!/usr/bin/env bash
# Roda o script de teste do parser em C fornecido pela disciplina, do jeito que
# o professor vai rodar, mas com os caminhos deste repositório.
#
# O script original (ref/scripts/testar_parser_c.sh, mantido lá sem edição) é
# chamado assim, segundo o enunciado:
#
#   bash testar_parser_c.sh ./testes-parser-50 ./parser.c
#
# Ele compila o parser como UMA única unidade de tradução
# (`gcc -Wall -Wextra -std=c11 $PARSER -o parser`) — é por isso que existe o
# parser.c na raiz deste repositório, que inclui os fontes de src/c/. Assim a
# linha de comando do professor funciona sem adaptação nenhuma.
#
# Uso:
#   bash src/c/test_parser_c.sh
#
# COMO LER O RESULTADO: igual ao wrapper da versão Python — 16 aprovados, 34
# reprovados e 25 erros sintáticos detectados é o melhor resultado possível
# neste pacote de testes, por dois defeitos conhecidos dele (frase "NÃO HÁ AST"
# como esperado dos 25 casos inválidos e espaçamento inconsistente nos válidos).
# Ver src/python/test_parser_python.sh e ref/testes-oficiais/README.md para o
# detalhamento, e use `python tests/run_parser_tests.py` para a verificação
# real (AST normalizada, exit codes e equivalência Python <-> C).
#
# NOTA (Windows): o script oficial compila com `-o ./parser` e depois checa
# `[[ -x ./parser ]]`. No Windows o gcc gera `parser.exe`, então essa checagem
# pode falhar dependendo do ambiente; no Git Bash com MinGW funciona porque o
# gcc também deixa o arquivo sem extensão. Se falhar, use
# `python tests/run_parser_tests.py --c`, que compila exatamente do mesmo jeito.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT" || exit 1

export LC_ALL="${LC_ALL:-C.UTF-8}"

TESTS_DIR="${1:-ref/testes-oficiais/testes-parser-50}"
PARSER_SRC="${2:-./parser.c}"

bash ref/scripts/testar_parser_c.sh "$TESTS_DIR" "$PARSER_SRC"
