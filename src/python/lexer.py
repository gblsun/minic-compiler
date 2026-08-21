"""Analisador léxico da linguagem MINIC.

Implementação manual (sem `re`) conforme docs/tokens.md e a Seção 3 de
ref/especificacao-completa-minic.pdf.

A ideia geral é simples: percorremos o código-fonte caractere a caractere,
com um "cursor" que sabe em que posição (linha e coluna) estamos, e a cada
passo decidimos que tipo de token está começando ali só olhando para o
caractere atual (e, quando precisa, para o próximo). Não usamos expressões
regulares de propósito — o objetivo da disciplina é entender como um lexer
funciona por dentro, então preferimos escrever o reconhecimento token a
token na mão.
"""

from dataclasses import dataclass, field

# Só esses tipos de token carregam um "atributo" (o lexema em si) que muda de
# ocorrência para ocorrência. Os demais (palavras reservadas, símbolos, etc.)
# são sempre o mesmo texto fixo, então não faz sentido repetir o lexema na
# saída — o próprio nome do token já diz tudo.
TOKENS_COM_ATRIBUTO = {"IDENT", "INT", "FLOAT", "CHAR", "STRING"}

# Palavras reservadas da linguagem: se um identificador escaneado bater com
# uma chave deste dicionário, ele deixa de ser IDENT e vira a palavra-chave
# correspondente. É por isso que identificadores e palavras-chave são
# reconhecidos pela mesma função (_scan_identifier_or_keyword) — sintaticamente
# eles têm exatamente a mesma forma, a diferença só aparece depois de ler a
# palavra inteira.
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

# Sequências de escape válidas dentro de char/string (Seção 3.5 da spec).
# Qualquer coisa fora daqui (ex.: "\q") é erro léxico — ver _read_escape.
ESCAPES = {"n": "\n", "t": "\t", "\\": "\\", "'": "'", '"': '"'}

# Operadores de dois caracteres. Precisam ser conferidos ANTES dos de um
# caractere só, senão "==" seria lido como dois ASSIGN separados em vez de
# um único EQ. Essa é a técnica clássica de "maximal munch": sempre tentar
# consumir o token mais longo possível primeiro.
TWO_CHAR_SYMBOLS = {
    "==": "EQ",
    "!=": "NEQ",
    "<=": "LE",
    ">=": "GE",
    "&&": "AND",
    "||": "OR",
}

# Símbolos de um único caractere (operadores, delimitadores, pontuação).
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
    """Um token reconhecido pelo lexer.

    `line`/`column` guardam onde o token COMEÇA no código-fonte (1-indexado,
    como um editor de texto mostraria), o que é essencial para as mensagens
    de erro do parser mais pra frente. `value` guarda o valor já convertido
    (ex.: o `int` de "42", não a string "42") — hoje ele só é realmente usado
    internamente, quem imprime o token usa `lexeme` via `__str__`.
    """

    type: str
    lexeme: str
    line: int
    column: int
    value: object = None

    def __str__(self):
        # Formato pedido pela especificação: tokens "com atributo" aparecem
        # como <TOKEN, lexema>; os demais aparecem só com o nome do token,
        # já que o lexema seria sempre igual ao próprio tipo (ex.: SEMICOLON
        # é sempre ";").
        if self.type in TOKENS_COM_ATRIBUTO:
            return f"<{self.type}, {self.lexeme}>"
        return self.type


@dataclass
class LexError:
    """Um erro léxico, no formato exigido pela Seção 12 da especificação."""

    message: str
    line: int
    column: int

    def __str__(self):
        return f"Erro léxico na linha {self.line}, coluna {self.column}: {self.message}."


class Lexer:
    """Varre o código-fonte inteiro e produz a lista de tokens e de erros.

    O lexer NÃO para no primeiro erro: ele registra o erro e continua
    tentando reconhecer o resto do arquivo, para que `main.py` consiga
    reportar todos os problemas léxicos de uma vez (em vez de obrigar o
    usuário a corrigir um erro por vez e rodar de novo).
    """

    def __init__(self, source: str):
        self.source = source
        self.pos = 0  # índice absoluto do próximo caractere a ler em `source`
        self.line = 1
        self.column = 1
        self.tokens: list[Token] = []
        self.errors: list[LexError] = []

    def tokenize(self):
        """Ponto de entrada: consome o código inteiro e devolve (tokens, erros)."""
        while True:
            self._skip_whitespace_and_comments()
            if self._at_end():
                break
            self._scan_token()
        # Um token EOF explícito no final ajuda etapas futuras do compilador
        # (o parser, por exemplo) a saber onde o fluxo de tokens acaba sem
        # precisar checar `len(tokens)` toda hora. `main.py` filtra esse
        # token antes de imprimir, já que ele não faz parte da saída visível.
        self.tokens.append(Token("EOF", "", self.line, self.column))
        return self.tokens, self.errors

    # -- cursor -----------------------------------------------------------
    # Este bloco é a "camada de baixo nível" do lexer: ler caracteres do
    # código-fonte um a um, sempre mantendo `line`/`column` em dia. Todo o
    # resto do arquivo é construído em cima só destas quatro operações.

    def _at_end(self):
        return self.pos >= len(self.source)

    def _peek(self, offset: int = 0) -> str:
        """Olha um caractere à frente sem consumi-lo (lookahead).

        Devolve "\\0" quando a posição pedida está fora do arquivo, assim
        quem chama pode comparar com um caractere normal sem precisar checar
        limites o tempo todo (ex.: `_peek() == "'"` funciona mesmo perto do
        fim do arquivo).
        """
        idx = self.pos + offset
        if idx >= len(self.source):
            return "\0"
        return self.source[idx]

    def _advance(self) -> str:
        """Consome e devolve o caractere atual, avançando o cursor.

        É aqui que linha/coluna são atualizadas: uma quebra de linha reinicia
        a coluna e incrementa a linha; qualquer outro caractere só anda uma
        coluna para a direita.
        """
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
        """Pula espaços, tabs, quebras de linha e comentários antes do próximo token.

        Isso roda em loop porque pode haver várias dessas coisas em sequência
        (ex.: um comentário seguido de espaços seguido de outro comentário);
        só paramos quando encontramos algo que de fato inicia um token.
        """
        while not self._at_end():
            ch = self._peek()
            if ch in " \t\r\n":
                self._advance()
                continue
            if ch == "/" and self._peek(1) == "/":
                # Comentário de linha: consome tudo até a próxima quebra de
                # linha (sem consumir a própria quebra, que o loop de cima
                # trata na próxima iteração).
                while not self._at_end() and self._peek() != "\n":
                    self._advance()
                continue
            if ch == "/" and self._peek(1) == "*":
                self._skip_block_comment()
                continue
            break

    def _skip_block_comment(self):
        """Consome um comentário `/* ... */` (sem suporte a aninhamento, como manda a spec)."""
        start_line, start_col = self.line, self.column
        self._advance()  # "/"
        self._advance()  # "*"
        while not self._at_end():
            if self._peek() == "*" and self._peek(1) == "/":
                self._advance()
                self._advance()
                return
            self._advance()
        # Chegamos ao fim do arquivo sem achar o "*/" que fecha o comentário.
        self._add_error("comentário de bloco não terminado", start_line, start_col)

    # -- dispatch -----------------------------------------------------------

    def _scan_token(self):
        """Decide, a partir do primeiro caractere, qual tipo de token começa aqui.

        Esse é o "roteador" principal do lexer: cada categoria léxica tem uma
        característica no primeiro caractere que já entrega a que ela pertence
        (letra/`_` → identificador ou palavra-chave; dígito → número; aspas →
        string/char; qualquer outra coisa → símbolo/operador).
        """
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
        """Lê um identificador inteiro (maximal munch) e só depois decide o tipo.

        Consumimos letras, dígitos e `_` enquanto der, formando o maior lexema
        possível, e só então conferimos se ele é uma palavra reservada. Fazer
        essa checagem cedo demais (ex.: parar em "in" achando que é `int`)
        quebraria identificadores como `inteiro` ou `index`.
        """
        chars = [first]
        while not self._at_end() and (self._peek().isalnum() or self._peek() == "_"):
            chars.append(self._advance())
        lexeme = "".join(chars)
        token_type = KEYWORDS.get(lexeme, "IDENT")
        self._add_token(token_type, lexeme, line, column, lexeme)

    def _scan_number(self, first: str, line: int, column: int):
        """Lê um número inteiro ou de ponto flutuante.

        A parte fracionária só é consumida se houver um dígito logo depois do
        ponto (`self._peek(1).isdigit()`). Isso evita, por exemplo, tratar
        "3." como início de um float incompleto: nesse caso o "3" vira um
        INT normal e o "." sobra para ser tratado (e provavelmente rejeitado,
        já que "." sozinho não é um símbolo reconhecido) na próxima chamada.
        """
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
        """Lê uma cadeia entre aspas duplas, processando escapes pelo caminho.

        Strings em MINIC não atravessam quebra de linha: se a linha acabar
        antes de fecharmos as aspas, é erro ("cadeia não terminada"), mesmo
        que o arquivo continue depois. Isso evita que um `"` esquecido engula
        o resto do arquivo inteiro como se fosse uma única string gigante.
        """
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
        """Lê um literal de caractere `'x'`.

        Char é o caso mais delicado do lexer porque a spec exige exatamente
        UM caractere entre aspas simples — nem zero, nem dois ou mais — então
        precisamos diferenciar três situações de erro além do caso feliz:

        1. `''` (vazio) ou aspas nunca fechadas na mesma linha/arquivo.
        2. Exatamente um caractere (ou um escape) seguido de `'` → sucesso.
        3. Mais de um caractere antes do `'` de fechamento → "tamanho inválido"
           (achamos o fechamento, só que tarde demais).
        """
        if self._at_end() or self._peek() in ("\n", "'"):
            if self._peek() == "'":
                # `''`: as aspas fecham imediatamente, ou seja, zero
                # caracteres dentro — tamanho inválido, não "vazio".
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
            # Caso feliz: um caractere (ou um escape, que conta como um só),
            # fechado corretamente.
            self._advance()
            self._add_token("CHAR", ch_value, line, column, ch_value)
            return

        # Sobrou mais coisa antes do fechamento (ex.: 'ab') — ou o arquivo
        # acabou/quebrou linha sem fechar. Drenamos o resto para descobrir
        # qual dos dois casos é, e só então decidimos a mensagem de erro.
        while not self._at_end() and self._peek() not in ("'", "\n"):
            self._advance()
        if not self._at_end() and self._peek() == "'":
            self._advance()
            self._add_error("caractere com tamanho inválido", line, column)
        else:
            self._add_error("literal de caractere não terminado", line, column)

    def _read_escape(self) -> str:
        """Interpreta o caractere logo após uma `\\` (já consumida por quem chamou).

        Devolve o valor "traduzido" do escape (ex.: `\\n` vira uma quebra de
        linha de verdade). Se a sequência não existir em `ESCAPES`, registra
        um erro léxico mas ainda assim consome o caractere inválido — assim o
        lexer não trava num loop infinito tentando reler o mesmo caractere, e
        consegue seguir escaneando o resto do arquivo.
        """
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
        """Reconhece operadores e delimitadores, tentando sempre o mais longo primeiro.

        Primeiro testamos o par `ch + próximo caractere` contra os símbolos de
        dois caracteres (maximal munch, mesma ideia do EQ vs. ASSIGN+ASSIGN
        citada lá em cima); só se isso falhar caímos para os símbolos de um
        caractere. Se nem isso bater, o caractere não pertence ao alfabeto da
        linguagem e viramos um erro léxico.
        """
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
    """Atalho conveniente para `Lexer(source).tokenize()`, usado pelos testes/UI."""
    return Lexer(source).tokenize()
