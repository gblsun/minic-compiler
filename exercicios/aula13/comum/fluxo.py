"""Ponte entre os exercícios da aula 13 e o scanner do projeto.

O scanner **não** é copiado: este módulo importa `src/python/lexer.py` (o lexer
da etapa 1) e oferece por cima dele um fluxo de tokens com lookahead — a
interface `next_token/lookahead` que os exercícios pedem.

Também concentra o que todos os exercícios repetem: leitura do arquivo (ou da
entrada padrão, com `-`), configuração da saída em UTF-8 e o formato dos
diagnósticos, que é o mesmo do projeto:

    Erro léxico na linha L, coluna C: ...
    Erro de sintaxe na linha L, coluna C: esperado X; encontrado Y.
    Erro semântico na linha L, coluna C: ...

Códigos de saída (iguais em todos os exercícios, em Python e em C):
0 = ok, 1 = erro de uso/arquivo, 2 = erro léxico, 3 = erro de sintaxe,
4 = erro semântico.
"""

import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(RAIZ / "src" / "python"))

from lexer import Lexer  # noqa: E402  (depende do ajuste de sys.path acima)

OK, ERRO_USO, ERRO_LEXICO, ERRO_SINTAXE, ERRO_SEMANTICO = 0, 1, 2, 3, 4


def configurar_saida():
    """UTF-8 nas duas saídas, devolvendo intactos bytes inválidos da entrada."""
    sys.stdout.reconfigure(encoding="utf-8", errors="surrogateescape")
    sys.stderr.reconfigure(encoding="utf-8", errors="surrogateescape")


def ler_fonte(caminho: str) -> str:
    """Lê o arquivo (ou a entrada padrão, se `caminho` for "-").

    Lança OSError se o arquivo não puder ser aberto.
    """
    if caminho == "-":
        dados = sys.stdin.buffer.read()
    else:
        dados = Path(caminho).read_bytes()
    # Mesmo tratamento de fim de linha do lexer em C, que lê bytes crus:
    # "\r" é espaço em branco para os dois, então não precisa ser removido.
    return dados.decode("utf-8", errors="surrogateescape")


def tokenizar(fonte: str):
    """Devolve `(tokens, erros_lexicos)`; o último token é sempre EOF."""
    return Lexer(fonte).tokenize()


def descrever(token) -> str:
    """Como um token aparece nas mensagens de erro: `'x'` ou `fim da entrada`."""
    if token.type == "EOF":
        return "fim da entrada"
    return f"'{token.lexeme}'"


def erro_semantico(linha: int, coluna: int, mensagem: str) -> str:
    return f"Erro semântico na linha {linha}, coluna {coluna}: {mensagem}."


class ErroSintaxe(Exception):
    """Interrompe a produção atual; carrega a mensagem já formatada."""

    def __init__(self, token, esperado: str):
        self.token = token
        self.esperado = esperado
        super().__init__(
            f"Erro de sintaxe na linha {token.line}, coluna {token.column}: "
            f"esperado {esperado}; encontrado {descrever(token)}."
        )


class Fluxo:
    """Fluxo de tokens com lookahead (TokenStream)."""

    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0

    def atual(self):
        return self.espiar(0)

    def espiar(self, k: int):
        """Lookahead: o k-ésimo token à frente, sem consumir (EOF no fim)."""
        i = self.pos + k
        return self.tokens[i] if i < len(self.tokens) else self.tokens[-1]

    def proximo(self):
        """next_token: consome e devolve o token atual."""
        token = self.atual()
        if token.type != "EOF":
            self.pos += 1
        return token

    def verifica(self, *tipos: str) -> bool:
        return self.atual().type in tipos

    def aceita(self, *tipos: str):
        """Consome o token se for de um dos tipos; senão devolve None."""
        return self.proximo() if self.verifica(*tipos) else None

    def exige(self, tipo: str, esperado: str):
        """Consome um token do tipo pedido ou lança ErroSintaxe."""
        if self.verifica(tipo):
            return self.proximo()
        raise ErroSintaxe(self.atual(), esperado)


def abrir(argv_arquivo: str):
    """Lê e tokeniza; devolve `(Fluxo, codigo)`.

    `codigo` é OK, ou ERRO_USO/ERRO_LEXICO (já com as mensagens impressas em
    stderr) — nesse caso o Fluxo é None e o exercício deve encerrar com ele.
    """
    try:
        fonte = ler_fonte(argv_arquivo)
    except OSError as exc:
        print(f"erro ao abrir '{argv_arquivo}': {exc.strerror}", file=sys.stderr)
        return None, ERRO_USO
    tokens, erros = tokenizar(fonte)
    if erros:
        for erro in erros:
            print(erro, file=sys.stderr)
        return None, ERRO_LEXICO
    return Fluxo(tokens), OK
