"""Analisador léxico da linguagem MINIC.

Implementação manual (sem `re`) conforme docs/tokens.md e a Seção 3 de
ref/especificacao-completa-minic.pdf.
"""

from dataclasses import dataclass, field

TOKENS_COM_ATRIBUTO = {"IDENT", "INT", "FLOAT", "CHAR", "STRING"}

KEYWORDS = {
    "int": "KW_INT",
    "float": "KW_FLOAT",
    "bool": "KW_BOOL",
    "char": "KW_CHAR",
    "void": "KW_VOID",
    "if": "KW_IF",
    "else": "KW_ELSE",
    "while": "KW_WHILE",
    "for": "KW_FOR",
    "return": "KW_RETURN",
    "break": "KW_BREAK",
    "continue": "KW_CONTINUE",
    "true": "KW_TRUE",
    "false": "KW_FALSE",
    "print": "KW_PRINT",
    "read": "KW_READ",
}

ESCAPES = {"n": "\n", "t": "\t", "\\": "\\", "'": "'", '"': '"'}

TWO_CHAR_SYMBOLS = {
    "==": "EQ",
    "!=": "NEQ",
    "<=": "LE",
    ">=": "GE",
    "&&": "AND",
    "||": "OR",
}

ONE_CHAR_SYMBOLS = {
    "+": "PLUS",
    "-": "MINUS",
    "*": "STAR",
    "/": "SLASH",
    "%": "PERCENT",
    "<": "LT",
    ">": "GT",
    "!": "NOT",
    "=": "ASSIGN",
    "(": "LPAREN",
    ")": "RPAREN",
    "[": "LBRACKET",
    "]": "RBRACKET",
    "{": "LBRACE",
    "}": "RBRACE",
    ";": "SEMICOLON",
    ",": "COMMA",
}


@dataclass
class Token:
    type: str
    lexeme: str
    line: int
    column: int
    value: object = None

    def __str__(self):
        if self.type in TOKENS_COM_ATRIBUTO:
            return f"<{self.type}, {self.lexeme}>"
        return self.type


@dataclass
class LexError:
    message: str
    line: int
    column: int

    def __str__(self):
        return f"Erro léxico na linha {self.line}, coluna {self.column}: {self.message}."


class Lexer:
    def __init__(self, source: str):
        self.source = source
        self.pos = 0
        self.line = 1
        self.column = 1
        self.tokens: list[Token] = []
        self.errors: list[LexError] = []

    def tokenize(self):
        while True:
            self._skip_whitespace_and_comments()
            if self._at_end():
                break
            self._scan_token()
        self.tokens.append(Token("EOF", "", self.line, self.column))
        return self.tokens, self.errors

    # -- cursor -----------------------------------------------------------

    def _at_end(self):
        return self.pos >= len(self.source)

    def _peek(self, offset: int = 0) -> str:
        idx = self.pos + offset
        if idx >= len(self.source):
            return "\0"
        return self.source[idx]

    def _advance(self) -> str:
        ch = self.source[self.pos]
        self.pos += 1
        if ch == "\n":
            self.line += 1
            self.column = 1
        else:
            self.column += 1
        return ch

    def _add_token(self, type_: str, lexeme: str, line: int, column: int, value=None):
        self.tokens.append(Token(type_, lexeme, line, column, value))

    def _add_error(self, message: str, line: int, column: int):
        self.errors.append(LexError(message, line, column))

    # -- espaços e comentários --------------------------------------------

    def _skip_whitespace_and_comments(self):
        while not self._at_end():
            ch = self._peek()
            if ch in " \t\r\n":
                self._advance()
                continue
            if ch == "/" and self._peek(1) == "/":
                while not self._at_end() and self._peek() != "\n":
                    self._advance()
                continue
            if ch == "/" and self._peek(1) == "*":
                self._skip_block_comment()
                continue
            break

    def _skip_block_comment(self):
        start_line, start_col = self.line, self.column
        self._advance()
        self._advance()
        while not self._at_end():
            if self._peek() == "*" and self._peek(1) == "/":
                self._advance()
                self._advance()
                return
            self._advance()
        self._add_error("comentário de bloco não terminado", start_line, start_col)

    # -- dispatch -----------------------------------------------------------

    def _scan_token(self):
        line, column = self.line, self.column
        ch = self._advance()

        if ch.isalpha() or ch == "_":
            self._scan_identifier_or_keyword(ch, line, column)
        elif ch.isdigit():
            self._scan_number(ch, line, column)
        elif ch == '"':
            self._scan_string(line, column)
        elif ch == "'":
            self._scan_char(line, column)
        else:
            self._scan_symbol(ch, line, column)

    def _scan_identifier_or_keyword(self, first: str, line: int, column: int):
        chars = [first]
        while not self._at_end() and (self._peek().isalnum() or self._peek() == "_"):
            chars.append(self._advance())
        lexeme = "".join(chars)
        token_type = KEYWORDS.get(lexeme, "IDENT")
        self._add_token(token_type, lexeme, line, column, lexeme)

    def _scan_number(self, first: str, line: int, column: int):
        chars = [first]
        while not self._at_end() and self._peek().isdigit():
            chars.append(self._advance())

        is_float = False
        if self._peek() == "." and self._peek(1).isdigit():
            is_float = True
            chars.append(self._advance())
            while not self._at_end() and self._peek().isdigit():
                chars.append(self._advance())

        lexeme = "".join(chars)
        if is_float:
            self._add_token("FLOAT", lexeme, line, column, float(lexeme))
        else:
            self._add_token("INT", lexeme, line, column, int(lexeme))

    def _scan_string(self, line: int, column: int):
        chars = []
        terminated = False
        while not self._at_end():
            ch = self._peek()
            if ch == '"':
                self._advance()
                terminated = True
                break
            if ch == "\n":
                break
            if ch == "\\":
                self._advance()
                chars.append(self._read_escape())
                continue
            chars.append(self._advance())

        if not terminated:
            self._add_error("cadeia não terminada", line, column)
            return

        value = "".join(chars)
        self._add_token("STRING", value, line, column, value)

    def _scan_char(self, line: int, column: int):
        if self._at_end() or self._peek() in ("\n", "'"):
            if self._peek() == "'":
                self._advance()
                self._add_error("caractere com tamanho inválido", line, column)
            else:
                self._add_error("literal de caractere não terminado", line, column)
            return

        if self._peek() == "\\":
            self._advance()
            ch_value = self._read_escape()
        else:
            ch_value = self._advance()

        if not self._at_end() and self._peek() == "'":
            self._advance()
            self._add_token("CHAR", ch_value, line, column, ch_value)
            return

        # mais de um caractere antes do fechamento, ou EOF/quebra de linha sem fechar
        while not self._at_end() and self._peek() not in ("'", "\n"):
            self._advance()
        if not self._at_end() and self._peek() == "'":
            self._advance()
            self._add_error("caractere com tamanho inválido", line, column)
        else:
            self._add_error("literal de caractere não terminado", line, column)

    def _read_escape(self) -> str:
        esc = self._peek()
        if esc in ESCAPES:
            self._advance()
            return ESCAPES[esc]
        line, column = self.line, self.column
        self._add_error(f'sequência de escape inválida "\\{esc}"', line, column)
        if not self._at_end() and esc != "\0":
            self._advance()
        return esc

    def _scan_symbol(self, ch: str, line: int, column: int):
        pair = ch + self._peek()
        if pair in TWO_CHAR_SYMBOLS:
            self._advance()
            self._add_token(TWO_CHAR_SYMBOLS[pair], pair, line, column)
            return

        if ch in ONE_CHAR_SYMBOLS:
            self._add_token(ONE_CHAR_SYMBOLS[ch], ch, line, column)
            return

        self._add_error(f'símbolo "{ch}" não reconhecido', line, column)


def tokenize(source: str):
    return Lexer(source).tokenize()
