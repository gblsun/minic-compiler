#!/usr/bin/env bash
# Compila e testa o analisador léxico em C contra os casos de
# tests/inputs/*.mc, comparando com o esperado gravado em tests/expected/.
#
# Adaptado do script test_scanner_c.sh fornecido pela disciplina (ver
# ../../ref/scripts/test_scanner_c.sh, mantido lá sem edição como material de
# referência) para o formato de saída e a estrutura de pastas realmente
# usados neste repositório:
#   - a saída do lexer é texto simples (um token por linha em stdout, um
#     erro léxico por linha em stderr), não JSONL;
#   - o esperado de cada caso fica em tests/expected/<nome>.{stdout,stderr,exit}.txt,
#     não em <entrada>.expected.jsonl.
#
# Uso:
#   ./test_scanner_c.sh [pasta-com-os-.c] [tests_dir]
#
# Por padrão compila todos os .c deste diretório (lexer.c, main.c, ...) e usa
# a pasta tests/ na raiz do repo. Espera-se que o binário resultante aceite
# `./scanner <arquivo.mc>`, imprima os tokens no formato de docs/tokens.md em
# stdout, os erros léxicos no formato da Seção 12 em stderr, e retorne o
# código de saída da Seção 11.1 (0 sem erro léxico, 2 com erro léxico).
#
# Compila apenas lexer.c + main.c (os fontes do scanner). Os fontes do parser
# da etapa 2 moram na mesma pasta e têm o seu próprio script, test_parser_c.sh.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${1:-$SCRIPT_DIR}"
TESTS_DIR="${2:-$SCRIPT_DIR/../../tests}"
BINARY="$SCRIPT_DIR/scanner"
INPUTS_DIR="$TESTS_DIR/inputs"
EXPECTED_DIR="$TESTS_DIR/expected"

TOTAL=0
PASS=0
FAIL=0

# Só os fontes do SCANNER: a pasta também tem os fontes do parser (ast.c,
# parser.c, parser_main.c), e compilar tudo junto daria dois `main` no mesmo
# executável. Quem testa o parser é ../../tests/run_parser_tests.py e
# test_parser_c.sh.
sources=("$SRC_DIR/lexer.c" "$SRC_DIR/main.c")
for src in "${sources[@]}"; do
    [[ -f "$src" ]] || { echo "ERRO: fonte do scanner não encontrado: $src" >&2; exit 2; }
done

echo '== Compilando o analisador léxico =='
gcc -Wall -Wextra -std=c11 "${sources[@]}" -o "$BINARY" || { echo 'ERRO: a compilação falhou.' >&2; exit 2; }
echo "Executável gerado: $BINARY"

run_case() {
    local input="$1" name
    name="$(basename "$input" .mc)"
    local expected_stdout="$EXPECTED_DIR/$name.stdout.txt"
    local expected_stderr="$EXPECTED_DIR/$name.stderr.txt"
    local expected_exit="$EXPECTED_DIR/$name.exit.txt"

    TOTAL=$((TOTAL + 1))
    printf '\n================================================================\n'
    printf 'Caso: %s\n' "$name"
    printf 'Comando: %s %s\n' "$BINARY" "$input"

    if [[ ! -f "$expected_stdout" || ! -f "$expected_stderr" || ! -f "$expected_exit" ]]; then
        echo "AVISO: saída esperada ausente para '$name' (rode tests/run_tests.py --update depois de conferir a saída à mão)."
        FAIL=$((FAIL + 1))
        return
    fi

    local stderr_tmp actual_stdout actual_stderr actual_exit
    stderr_tmp="$(mktemp)"
    actual_stdout="$("$BINARY" "$input" 2>"$stderr_tmp")"
    actual_exit=$?
    actual_stderr="$(cat "$stderr_tmp")"
    rm -f "$stderr_tmp"

    local want_stdout want_stderr want_exit
    want_stdout="$(cat "$expected_stdout")"
    want_stderr="$(cat "$expected_stderr")"
    want_exit="$(tr -d '[:space:]' < "$expected_exit")"

    local ok=1
    if [[ "$actual_stdout" != "$want_stdout" ]]; then
        echo 'FALHA: stdout (tokens) diverge do esperado.'
        diff <(printf '%s\n' "$want_stdout") <(printf '%s\n' "$actual_stdout")
        ok=0
    fi
    if [[ "$actual_stderr" != "$want_stderr" ]]; then
        echo 'FALHA: stderr (erros léxicos) diverge do esperado.'
        diff <(printf '%s\n' "$want_stderr") <(printf '%s\n' "$actual_stderr")
        ok=0
    fi
    if [[ "$actual_exit" != "$want_exit" ]]; then
        echo "FALHA: exit code $actual_exit != esperado $want_exit"
        ok=0
    fi

    if [[ "$ok" == 1 ]]; then
        echo 'RESULTADO: OK'
        PASS=$((PASS + 1))
    else
        echo 'RESULTADO: FALHOU'
        FAIL=$((FAIL + 1))
    fi
}

[[ -d "$INPUTS_DIR" ]] || { echo "ERRO: pasta de testes não encontrada em '$INPUTS_DIR'." >&2; exit 2; }

mapfile -t inputs < <(find "$INPUTS_DIR" -maxdepth 1 -type f -name '*.mc' | sort)
(( ${#inputs[@]} > 0 )) || { echo "ERRO: nenhum arquivo .mc encontrado em '$INPUTS_DIR'." >&2; exit 2; }

for input in "${inputs[@]}"; do
    run_case "$input"
done

printf '\n================================================================\n'
printf 'Resumo: %d OK, %d falha(s), %d caso(s) verificados.\n' "$PASS" "$FAIL" "$TOTAL"
(( FAIL == 0 ))
