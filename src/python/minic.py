"""Compilador MINIC — a CLI do pipeline completo (Seção 11 da especificação).

    python minic.py codigo.c                  # análise semântica (relatório da AP 2)
    python minic.py --check codigo.c          # idem: analisa sem gerar código
    python minic.py --tokens codigo.c         # só os tokens
    python minic.py --ast codigo.c            # a AST, na forma compacta
    python minic.py --symbols codigo.c        # tabela de símbolos, com deslocamentos
    python minic.py --ir [-O0|-O1] codigo.c   # IR de três endereços
    python minic.py --target=asm [-O1] codigo.c   # código da máquina de pilha (MVM)
    python minic.py --run [-O1] [--stats] codigo.c  # gera e executa na MVM

Encadeia as fases, cada uma no seu módulo: `lexer.py` (tokens) → `parser.py`
(AST) → `semantico.py` (tabela de símbolos e verificação) → `ir.py` (código de
três endereços) → `otimizador.py` (só com -O1) → `geracao.py` (código da MVM)
→ `vm.py` (execução). Uma fase só roda se a anterior terminou sem erro.

Sem opção de modo, a saída é o relatório da análise semântica: é o contrato da
AP 2, cujo script chama `minic.py codigo.c` sem opções.

Saída e códigos (Seção 11.1):

- 0: sucesso — tokens, AST, relatório, símbolos, IR, código ou a saída do
  programa em stdout;
- 1: uso inválido;
- 2: erro léxico (stderr);
- 3: erro sintático (stderr);
- 4: erro semântico — o relatório da análise semântica em stdout;
- 5: erro na geração de código (stderr), por exemplo, programa sem `main`;
- 6: erro durante a execução (stderr), por exemplo, divisão por zero.
"""

import sys
from pathlib import Path

# Os módulos irmãos são achados pelo diretório deste arquivo, e não pelo
# diretório de onde o comando foi chamado.
sys.path.insert(0, str(Path(__file__).resolve().parent))

import geracao  # noqa: E402
import ir  # noqa: E402
import minic_ast  # noqa: E402
import otimizador  # noqa: E402
import semantico  # noqa: E402
import vm  # noqa: E402
from lexer import Lexer  # noqa: E402
from parser import parse  # noqa: E402

USO = (
    "uso: python minic.py [--check|--tokens|--ast|--symbols|--ir|--target=asm|--run] "
    "[-O0|-O1] [--stats] <arquivo.c>"
)
MODOS = ("--check", "--tokens", "--ast", "--symbols", "--ir", "--target=asm", "--run")


def _escrever(texto: str):
    # Bytes direto no buffer: o modo texto do Windows trocaria "\n" por "\r\n"
    # (e o relatório da AP 2 já traz os CRLF do gabarito).
    sys.stdout.buffer.write(texto.encode("utf-8", "surrogateescape"))
    sys.stdout.flush()


def main(argv: list[str]) -> int:
    # newline="\n": as mensagens de erro saem com LF em qualquer sistema (no
    # Windows o modo texto trocaria por CRLF), como na versão em C.
    sys.stderr.reconfigure(encoding="utf-8", errors="surrogateescape", newline="\n")

    args = argv[1:]
    if args in (["--help"], ["-h"]):
        print(__doc__.strip())
        return 0
    modo, otimizar, estatisticas, arquivos = "--check", False, False, []
    modos_dados = 0
    for arg in args:
        if arg in MODOS:
            modo = arg
            modos_dados += 1
        elif arg in ("-O0", "-O1"):
            otimizar = arg == "-O1"
        elif arg == "--stats":
            estatisticas = True
        else:
            arquivos.append(arg)
    if modos_dados > 1 or len(arquivos) != 1 or arquivos[0].startswith("-"):
        print(USO, file=sys.stderr)
        return 1

    caminho = Path(arquivos[0])
    try:
        source = caminho.read_text(encoding="utf-8", errors="surrogateescape")
    except OSError as exc:
        print(f"erro ao abrir '{caminho}': {exc}", file=sys.stderr)
        return 1

    tokens, erros_lexicos = Lexer(source).tokenize()
    if modo == "--tokens":
        _escrever("".join(f"{t}\n" for t in tokens if t.type != "EOF"))
    if erros_lexicos:
        for erro in erros_lexicos:
            print(erro, file=sys.stderr)
        return 2
    if modo == "--tokens":
        return 0

    # destino_generico: `3 = n;` passa pelo parser e vira SEM013 na semântica.
    programa, erros_sintaticos = parse(tokens, destino_generico=True)
    if erros_sintaticos:
        for erro in erros_sintaticos:
            print(erro, file=sys.stderr)
        return 3
    if modo == "--ast":
        _escrever(minic_ast.to_sexpr(programa) + "\n")
        return 0

    diagnosticos = semantico.analisar(programa)
    if modo == "--check" or diagnosticos:
        _escrever(semantico.relatorio(diagnosticos))
        return 4 if diagnosticos else 0

    codigo_ir = ir.gerar(programa)
    if otimizar:
        otimizador.otimizar(codigo_ir)
    if modo == "--ir":
        _escrever(ir.formatar(codigo_ir))
        return 0
    if modo == "--symbols":
        _escrever(geracao.simbolos(codigo_ir))
        return 0

    try:
        codigo_vm = geracao.gerar(codigo_ir)
    except geracao.ErroGeracao as erro:
        print(f"Erro na geração de código: {erro}", file=sys.stderr)
        return 5
    if modo == "--target=asm":
        _escrever(geracao.formatar(codigo_vm))
        return 0

    maquina = vm.MaquinaVirtual(codigo_vm)
    try:
        maquina.executar()
    except vm.ErroExecucao as erro:
        sys.stdout.flush()
        print(f"Erro de execução em {erro.funcao}: {erro}", file=sys.stderr)
        return 6
    finally:
        sys.stdout.buffer.flush()
        if estatisticas:
            print(f"instruções executadas: {maquina.executadas}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
