"""Compilador MINIC até a representação intermediária — a CLI do pipeline.

    python minic.py codigo.c             # análise semântica (relatório da AP 2)
    python minic.py --ir codigo.c        # IR de três endereços, sem otimização
    python minic.py --ir -O1 codigo.c    # IR otimizada (-O0 é o padrão)

Encadeia as fases, cada uma no seu módulo: `lexer.py` (tokens) → `parser.py`
(AST) → `semantico.py` (tabela de símbolos e verificação) → `ir.py` (código de
três endereços) → `otimizador.py` (só com -O1). Uma fase só roda se a anterior
terminou sem erro: com erro léxico ou sintático, a AST não existe (ou está
incompleta) e a análise semântica só produziria diagnósticos falsos; com erro
semântico, não há IR.

Saída e códigos (Seção 11.1 da especificação):

- com `--ir`, programa válido: a IR em **stdout** (linhas com LF), código 0;
- programa válido sem `--ir`, ou com erros semânticos: o relatório da análise
  semântica em **stdout** — os diagnósticos, um por linha, e o resumo "Análise semântica
  concluída: N erros; programa aceito/rejeitado." (formato da AP 2); código 0
  se aceito, 4 se rejeitado;
- erro léxico: as mensagens do lexer em stderr, código 2;
- erro sintático: as mensagens do parser em stderr, código 3;
- uso inválido: código 1.
"""

import sys
from pathlib import Path

# Os módulos irmãos são achados pelo diretório deste arquivo, e não pelo
# diretório de onde o comando foi chamado.
sys.path.insert(0, str(Path(__file__).resolve().parent))

import ir  # noqa: E402
import otimizador  # noqa: E402
import semantico  # noqa: E402
from lexer import Lexer  # noqa: E402
from parser import parse  # noqa: E402

USO = "uso: python minic.py [--ir] [-O0|-O1] <arquivo.c>"


def main(argv: list[str]) -> int:
    # newline="\n": as mensagens de erro saem com LF em qualquer sistema (no
    # Windows o modo texto trocaria por CRLF), como na versão em C.
    sys.stderr.reconfigure(encoding="utf-8", errors="surrogateescape", newline="\n")

    args = argv[1:]
    if args in (["--help"], ["-h"]):
        print(__doc__.strip())
        return 0
    modo_ir = "--ir" in args
    otimizar = False
    arquivos = []
    for arg in args:
        if arg in ("-O0", "-O1"):
            otimizar = arg == "-O1"
        elif arg != "--ir":
            arquivos.append(arg)
    if len(arquivos) != 1 or arquivos[0].startswith("-"):
        print(USO, file=sys.stderr)
        return 1

    caminho = Path(arquivos[0])
    try:
        source = caminho.read_text(encoding="utf-8", errors="surrogateescape")
    except OSError as exc:
        print(f"erro ao abrir '{caminho}': {exc}", file=sys.stderr)
        return 1

    tokens, erros_lexicos = Lexer(source).tokenize()
    if erros_lexicos:
        for erro in erros_lexicos:
            print(erro, file=sys.stderr)
        return 2

    # destino_generico: `3 = n;` passa pelo parser e vira SEM013 aqui.
    programa, erros_sintaticos = parse(tokens, destino_generico=True)
    if erros_sintaticos:
        for erro in erros_sintaticos:
            print(erro, file=sys.stderr)
        return 3

    diagnosticos = semantico.analisar(programa)
    if modo_ir and not diagnosticos:
        codigo_ir = ir.gerar(programa)
        if otimizar:
            otimizador.otimizar(codigo_ir)
        sys.stdout.buffer.write(ir.formatar(codigo_ir).encode("utf-8", "surrogateescape"))
        sys.stdout.flush()
        return 0

    # Bytes direto no buffer: o relatório já traz os CRLF do gabarito, e o
    # modo texto do Windows trocaria cada "\n" por "\r\n" mais uma vez.
    sys.stdout.buffer.write(semantico.relatorio(diagnosticos).encode("utf-8"))
    sys.stdout.flush()
    return 4 if diagnosticos else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
