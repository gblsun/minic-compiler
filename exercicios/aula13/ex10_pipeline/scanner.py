"""Scanner do mini pipeline (exercício 10).

Não reimplementa nada: usa o lexer da etapa 1 do projeto
(`src/python/lexer.py`) e expõe para o parser um fluxo de tokens com tipo,
lexema e posição (`token.type`, `token.lexeme`, `token.line`,
`token.column`), com `next_token` e `lookahead`.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "src" / "python"))

from lexer import Lexer, LexError, Token  # noqa: E402,F401  (reexportados)


class TokenStream:
    def __init__(self, tokens: list[Token]):
        self._tokens = tokens  # o lexer garante que o último é EOF
        self._pos = 0

    def lookahead(self, k: int = 0) -> Token:
        i = self._pos + k
        return self._tokens[i] if i < len(self._tokens) else self._tokens[-1]

    def next_token(self) -> Token:
        token = self.lookahead()
        if token.type != "EOF":
            self._pos += 1
        return token

    def check(self, *tipos: str) -> bool:
        return self.lookahead().type in tipos

    def accept(self, *tipos: str):
        return self.next_token() if self.check(*tipos) else None


def scan(source: str) -> tuple[TokenStream, list[LexError]]:
    tokens, erros = Lexer(source).tokenize()
    return TokenStream(tokens), erros
