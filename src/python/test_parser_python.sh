#!/usr/bin/env bash
# Roda o script de teste do parser fornecido pela disciplina, do jeito que o
# professor vai rodar, mas com os caminhos deste repositório.
#
# O script original (ref/scripts/testar_parser_python.sh, mantido lá sem
# edição) é chamado assim, segundo o enunciado:
#
#   bash testar_parser_python.sh ./testes-parser-50 ./parser.py
#
# Aqui os testes oficiais ficam em ref/testes-oficiais/testes-parser-50/ e o
# ponto de entrada é ./parser.py na raiz, então este wrapper só amarra as duas
# pontas — a lógica de comparação continua sendo a do professor, sem cópia nem
# alteração.
#
# Uso:
#   bash src/python/test_parser_python.sh
#
# COMO LER O RESULTADO (importante): o script oficial compara a saída do parser
# byte a byte com casos/NN/ast.esperada.txt, e o pacote de testes tem dois
# problemas conhecidos que limitam o resultado possível:
#
#   - nos 25 casos inválidos (26–50), o "esperado" é a frase "NÃO HÁ AST: o
#     parser deve rejeitar a entrada." — nenhum parser imprime isso, então
#     esses casos aparecem SEMPRE como FALHOU, por construção do script;
#   - nos 25 casos válidos, o espaçamento da notação é inconsistente entre
#     arquivos (o mesmo construto aparece nos dois estilos), então um impressor
#     canônico casa exatamente com 16 deles e difere dos outros 9 só em espaço
#     em branco.
#
# Resultado esperado hoje: 16 aprovados, 34 reprovados e **25 erros sintáticos
# detectados** (esse último número é o que mostra que todos os casos inválidos
# foram rejeitados). Para a verificação real — comparação da AST normalizando
# espaços, exit codes e equivalência entre Python e C — use:
#
#   python tests/run_parser_tests.py
#
# Ver ref/testes-oficiais/README.md para o levantamento completo.
#
# NOTA sobre o contador "Erros sintáticos": o script oficial conta os erros com
# um `grep -Ei 'erro[[:space:]_-]*(sint[aá]tico|de sintaxe)'`. A variante com
# "á" só casa em locale UTF-8, por isso os diagnósticos usam "Erro de sintaxe"
# (ASCII), que casa em qualquer locale. O LC_ALL exportado abaixo ficou só
# como garantia para a saída acentuada.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT" || exit 1

export LC_ALL="${LC_ALL:-C.UTF-8}"

TESTS_DIR="${1:-ref/testes-oficiais/testes-parser-50}"
PARSER="${2:-./parser.py}"

# O script oficial exige bit de execução no parser (`[[ -x "$PARSER" ]]`).
if [[ ! -x "$PARSER" ]]; then
    chmod +x "$PARSER"
fi

bash ref/scripts/testar_parser_python.sh "$TESTS_DIR" "$PARSER"
