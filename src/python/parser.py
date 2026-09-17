"""Analisador sintático (parser) da linguagem MINIC + CLI.

Implementação por **descida recursiva**: uma função por não terminal da
gramática de docs/gramatica.md, chamando uma à outra na mesma hierarquia das
produções. Nenhum gerador de parser (yacc/bison/ANTLR) é usado — o objetivo da
disciplina é escrever o reconhecimento à mão, como no lexer.

Uso:

    python parser.py codigo.c          # imprime a AST compacta
    python parser.py --tree codigo.c   # imprime a AST indentada
    python parser.py --tokens codigo.c # imprime só os tokens (depuração)

Contrato de saída (Seções 11.1 e 12 da especificação):

- **stdout**: a AST, quando o programa é sintaticamente válido;
- **stderr**: os diagnósticos (`Erro sintático na linha L, coluna C: ...`);
- **código de saída**: 0 = aceito, 1 = erro de uso, 2 = erro léxico,
  3 = erro sintático.

Duas notas de projeto que a gramática da especificação não resolve sozinha e
que os casos oficiais de teste fixam (ver docs/gramatica.md):

1. No topo do arquivo são aceitos declarações **e** comandos, em qualquer
   ordem — o caso oficial 22 (`int a; int b; a = b = 3;`) exige isso, embora
   `programa ::= declaracao_global* declaracao_funcao* funcao_main` sugira só
   declarações. A exigência de existir uma `main` é semântica (etapa 3), não
   sintática: 13 dos 25 casos válidos não têm `main`.
2. O alvo de uma atribuição é reconhecido analisando a expressão normalmente e
   **reinterpretando** o resultado: se depois dela vier `=`, o nó já construído
   precisa ser um `Id` ou um `Index` (um lvalue).
"""

import sys
from pathlib import Path

import minic_ast as A
from lexer import Lexer

# Os quatro tipos de dado da linguagem (`void` só aparece como tipo de retorno).
TIPOS = ("KW_INT", "KW_FLOAT", "KW_BOOL", "KW_CHAR")

# Nomes "didáticos" dos tokens nas mensagens de erro. São os mesmos rótulos que
# o pacote de testes oficial usa nas pistas dos casos inválidos
# (`pista: esperado PONTO_E_VIRGULA`), o que torna a conferência direta.
NOMES = {
    "SEMICOLON": "PONTO_E_VIRGULA",
    "COMMA": "VIRGULA",
    "LPAREN": "ABRE_PAREN",
    "RPAREN": "FECHA_PAREN",
    "LBRACE": "ABRE_CHAVE",
    "RBRACE": "FECHA_CHAVE",
    "LBRACKET": "ABRE_COLCHETE",
    "RBRACKET": "FECHA_COLCHETE",
    "ASSIGN": "ATRIBUICAO",
    "EOF": "fim do arquivo",
}

# Tokens que não podem, em nenhuma hipótese, iniciar um comando. Servem para
# distinguir "token X inesperado" (um fechamento sobrando, um `else` solto) de
# um erro mais específico vindo da análise da expressão — ver `_item_de_bloco`.
NAO_INICIA_COMANDO = frozenset(
    {"RBRACE", "RPAREN", "RBRACKET", "COMMA", "KW_ELSE", "EOF"}
)

# Mapa token de operador -> lexema, usado para montar os nós Binary/Unary sem
# depender do lexema do token (que é o mesmo, mas fica explícito assim).
OPERADORES = {
    "OR": "||",
    "AND": "&&",
    "EQ": "==",
    "NEQ": "!=",
    "LT": "<",
    "GT": ">",
    "LE": "<=",
    "GE": ">=",
    "PLUS": "+",
    "MINUS": "-",
    "STAR": "*",
    "SLASH": "/",
    "PERCENT": "%",
    "NOT": "!",
}


def nome_token(tipo: str) -> str:
    """Nome de um tipo de token como ele aparece nas mensagens de erro."""
    return NOMES.get(tipo, tipo)


class SyntaxErr:
    """Um erro sintático, no formato da Seção 12 da especificação."""

    def __init__(self, message: str, line: int, column: int):
        self.message = message
        self.line = line
        self.column = column

    def __str__(self):
        return f"Erro sintático na linha {self.line}, coluna {self.column}: {self.message}."


class ParseError(Exception):
    """Sinaliza o erro até o ponto de recuperação (modo pânico).

    Cada função do parser pode "desistir" lançando esta exceção; quem captura
    são os laços de item de bloco / item de topo, que registram o erro,
    sincronizam o cursor e seguem analisando o resto do arquivo. É o que
    permite reportar mais de um erro por execução sem que cada função precise
    devolver código de erro.
    """

    def __init__(self, message: str, line: int, column: int):
        super().__init__(message)
        self.info = SyntaxErr(message, line, column)


class Parser:
    """Consome a lista de tokens do lexer e devolve `(Program, erros)`."""

    def __init__(self, tokens: list):
        self.tokens = tokens
        self.pos = 0
        self.errors: list[SyntaxErr] = []

    # -- cursor sobre a lista de tokens -----------------------------------
    # Mesma ideia do cursor do lexer, um nível acima: lá o cursor andava sobre
    # caracteres, aqui anda sobre tokens.

    def _peek(self, offset: int = 0):
        idx = self.pos + offset
        if idx >= len(self.tokens):
            return self.tokens[-1]  # o EOF, que o lexer garante ser o último
        return self.tokens[idx]

    def _check(self, *tipos: str) -> bool:
        return self._peek().type in tipos

    def _advance(self):
        token = self._peek()
        if token.type != "EOF":
            self.pos += 1
        return token

    def _match(self, *tipos: str):
        """Consome o token atual se ele for de um dos tipos; senão devolve None."""
        if self._check(*tipos):
            return self._advance()
        return None

    def _expect(self, tipo: str, esperado: str | None = None):
        """Consome o token esperado ou aborta com um diagnóstico posicionado."""
        if self._check(tipo):
            return self._advance()
        self._erro(f"esperado {esperado or nome_token(tipo)}")

    def _erro(self, mensagem: str, token=None):
        """Lança o erro apontando para o token onde a análise travou."""
        token = token or self._peek()
        encontrado = nome_token(token.type)
        raise ParseError(
            f"{mensagem}; encontrado {encontrado}", token.line, token.column
        )

    def _registra(self, erro: ParseError):
        self.errors.append(erro.info)

    # -- recuperação de erro (modo pânico) --------------------------------

    def _sincroniza(self):
        """Descarta tokens até um ponto seguro para retomar a análise.

        Sincronizadores: `;` e `}` (fim de comando/bloco, consumidos), e o
        início de uma construção nova (tipo, `if`, `while`, …, que ficam para a
        próxima iteração do laço). Sem isso, um erro no meio de uma expressão
        faria o parser reclamar do mesmo token para sempre; descartando demais,
        ele produziria erros em cascata — daí a lista curta de pontos de parada.
        """
        while not self._check("EOF"):
            tipo = self._peek().type
            if tipo == "SEMICOLON":
                self._advance()
                return
            if tipo == "RBRACE":
                self._advance()
                return
            if tipo in TIPOS or tipo in (
                "KW_VOID",
                "KW_IF",
                "KW_WHILE",
                "KW_FOR",
                "KW_RETURN",
                "KW_BREAK",
                "KW_CONTINUE",
                "KW_PRINT",
                "KW_READ",
                "LBRACE",
            ):
                return
            self._advance()

    # -- programa ---------------------------------------------------------

    def parse(self):
        """Ponto de entrada: analisa o arquivo inteiro."""
        primeiro = self._peek()
        decls = []
        while not self._check("EOF"):
            try:
                item = self._item_de_topo()
            except ParseError as erro:
                self._registra(erro)
                self._sincroniza()
                continue
            # `int a, b = 2;` devolve vários VarDecl de uma vez — a lista é
            # achatada aqui (o mesmo acontece dentro de `_bloco`).
            if isinstance(item, list):
                decls.extend(item)
            else:
                decls.append(item)
        programa = A.Program(primeiro.line, primeiro.column, decls)
        return programa, self.errors

    def _item_de_topo(self):
        """Uma declaração (variável ou função) ou um comando no nível do arquivo."""
        if self._check(*TIPOS, "KW_VOID"):
            return self._declaracao()
        if self._peek().type in NAO_INICIA_COMANDO:
            self._erro("token inesperado no nível do programa")
        return self._comando()

    # -- declarações ------------------------------------------------------

    def _declaracao(self):
        """`tipo id ...` — decide entre função e variável pelo `(` depois do nome.

        As duas começam igual, então o desempate é por lookahead de um token
        depois do identificador. `void` só é válido como tipo de retorno de
        função, e é aqui que isso é cobrado.
        """
        tipo_tok = self._advance()
        nome = self._expect("IDENT")
        if self._check("LPAREN"):
            return self._resto_da_funcao(tipo_tok, nome)
        if tipo_tok.type == "KW_VOID":
            self._erro("void só pode ser tipo de retorno de função; esperado ABRE_PAREN")
        return self._resto_das_variaveis(tipo_tok, nome)

    def _resto_das_variaveis(self, tipo_tok, nome_tok):
        """Lista de declaradores: `int a, b = 2, v[3];`.

        Devolve um único `VarDecl` quando há um declarador só (o caso comum), e
        uma lista deles quando há vários — quem chama (`_item_de_topo`,
        `_item_de_bloco`) se encarrega de achatar a lista.
        """
        declaradores = [self._declarador(tipo_tok, nome_tok)]
        while self._match("COMMA"):
            outro_nome = self._expect("IDENT")
            declaradores.append(self._declarador(tipo_tok, outro_nome))
        self._expect("SEMICOLON")
        return declaradores[0] if len(declaradores) == 1 else declaradores

    def _declarador(self, tipo_tok, nome_tok):
        """`id`, `id = expressao` ou `id [ tamanho ]`."""
        no = A.VarDecl(nome_tok.line, nome_tok.column, tipo_tok.lexeme, nome_tok.lexeme)
        if self._match("LBRACKET"):
            no.size = self._expressao()
            self._expect("RBRACKET")
            return no
        if self._match("ASSIGN"):
            no.init = self._expressao()
        return no

    def _resto_da_funcao(self, tipo_tok, nome_tok):
        """`( parametros? ) bloco` — o `(` ainda não foi consumido."""
        self._expect("LPAREN")
        params = []
        if not self._check("RPAREN"):
            params.append(self._parametro())
            while self._match("COMMA"):
                params.append(self._parametro())
        self._expect("RPAREN", "FECHA_PAREN")
        if not self._check("LBRACE"):
            # Função sem corpo (`int f();`) é erro: a MINIC não tem protótipo.
            self._erro("esperado ABRE_CHAVE")
        corpo = self._bloco()
        return A.Function(
            tipo_tok.line,
            tipo_tok.column,
            tipo_tok.lexeme,
            nome_tok.lexeme,
            params,
            corpo,
        )

    def _parametro(self):
        """`tipo id` ou `tipo id[]` — o tipo é obrigatório."""
        if not self._check(*TIPOS):
            self._erro("esperado KW_<tipo> ou FECHA_PAREN")
        tipo_tok = self._advance()
        nome_tok = self._expect("IDENT")
        is_array = False
        if self._match("LBRACKET"):
            self._expect("RBRACKET")
            is_array = True
        return A.Param(
            tipo_tok.line, tipo_tok.column, tipo_tok.lexeme, nome_tok.lexeme, is_array
        )

    # -- comandos ---------------------------------------------------------

    def _bloco(self):
        """`{ item_bloco* }`, com recuperação por item."""
        abre = self._expect("LBRACE")
        itens = []
        while not self._check("RBRACE", "EOF"):
            try:
                item = self._item_de_bloco()
            except ParseError as erro:
                self._registra(erro)
                self._sincroniza()
                continue
            if isinstance(item, list):
                itens.extend(item)
            else:
                itens.append(item)
        self._expect("RBRACE", "FECHA_CHAVE")
        return A.Block(abre.line, abre.column, itens)

    def _item_de_bloco(self):
        """`declaracao_local | comando`, desempatado pelo token atual."""
        if self._check(*TIPOS):
            tipo_tok = self._advance()
            nome_tok = self._expect("IDENT")
            return self._resto_das_variaveis(tipo_tok, nome_tok)
        if self._peek().type in NAO_INICIA_COMANDO:
            self._erro("token inesperado no início de statement")
        return self._comando()

    def _comando(self):
        """Despacha o comando pelo token atual (FIRST de cada produção)."""
        token = self._peek()
        tipo = token.type

        if tipo in NAO_INICIA_COMANDO:
            # Corpo de construção ausente, como em `while (x < 2) }`: reclamar
            # da falta do comando é mais direto que deixar a análise de
            # expressão falhar mais adiante.
            self._erro("esperado início de statement")

        if tipo == "LBRACE":
            return self._bloco()
        if tipo == "KW_IF":
            return self._comando_if()
        if tipo == "KW_WHILE":
            return self._comando_while()
        if tipo == "KW_FOR":
            return self._comando_for()
        if tipo == "KW_RETURN":
            return self._comando_return()
        if tipo == "KW_BREAK":
            self._advance()
            self._expect("SEMICOLON")
            return A.Break(token.line, token.column)
        if tipo == "KW_CONTINUE":
            self._advance()
            self._expect("SEMICOLON")
            return A.Continue(token.line, token.column)
        if tipo == "KW_PRINT":
            return self._comando_print()
        if tipo == "KW_READ":
            return self._comando_read()
        if tipo == "SEMICOLON":
            # `comando_expressao ::= expressao? ";"` — o comando vazio.
            self._advance()
            return A.ExprStmt(token.line, token.column, None)
        if tipo in TIPOS:
            # Declaração onde só cabe comando (ex.: `if (x) int y;`): a
            # gramática não permite, e avisar explicitamente é mais útil que
            # um "token inesperado" genérico.
            self._erro("declaração não é permitida aqui; esperado início de statement")

        expr = self._expressao()
        self._expect("SEMICOLON")
        return A.ExprStmt(token.line, token.column, expr)

    def _comando_if(self):
        token = self._advance()
        self._expect("LPAREN")
        cond = self._expressao()
        self._expect("RPAREN", "FECHA_PAREN")
        entao = self._comando()
        senao = None
        if self._match("KW_ELSE"):
            # Consumir o `else` assim que ele aparece resolve o "else pendente"
            # ligando-o ao `if` mais próximo, que é a convenção adotada.
            senao = self._comando()
        return A.If(token.line, token.column, cond, entao, senao)

    def _comando_while(self):
        token = self._advance()
        self._expect("LPAREN")
        cond = self._expressao()
        self._expect("RPAREN", "FECHA_PAREN")
        corpo = self._comando()
        return A.While(token.line, token.column, cond, corpo)

    def _comando_for(self):
        token = self._advance()
        self._expect("LPAREN")
        init = None if self._check("SEMICOLON") else self._expressao()
        self._expect("SEMICOLON")
        cond = None if self._check("SEMICOLON") else self._expressao()
        self._expect("SEMICOLON")
        passo = None if self._check("RPAREN") else self._expressao()
        self._expect("RPAREN", "FECHA_PAREN")
        corpo = self._comando()
        return A.For(token.line, token.column, init, cond, passo, corpo)

    def _comando_return(self):
        token = self._advance()
        expr = None if self._check("SEMICOLON") else self._expressao()
        self._expect("SEMICOLON")
        return A.Return(token.line, token.column, expr)

    def _comando_print(self):
        token = self._advance()
        self._expect("LPAREN")
        arg = self._expressao()
        self._expect("RPAREN", "FECHA_PAREN")
        self._expect("SEMICOLON")
        return A.Print(token.line, token.column, arg)

    def _comando_read(self):
        token = self._advance()
        self._expect("LPAREN")
        alvo = self._expressao()
        if not isinstance(alvo, (A.Id, A.Index)):
            self._erro("read espera uma variável ou elemento de vetor")
        self._expect("RPAREN", "FECHA_PAREN")
        self._expect("SEMICOLON")
        return A.Read(token.line, token.column, alvo)

    # -- expressões -------------------------------------------------------
    # Um método por nível de precedência da tabela da Seção 4.3, do mais fraco
    # (atribuição) para o mais forte (primário). Os níveis binários são laços,
    # e não recursão: a gramática da especificação é recursiva à esquerda, o
    # que um parser descendente não pode executar literalmente, e o laço
    # produz exatamente a mesma árvore (associativa à esquerda).

    def _expressao(self):
        return self._atribuicao()

    def _atribuicao(self):
        esquerda = self._logico_ou()
        igual = self._match("ASSIGN")
        if igual is None:
            return esquerda
        if not isinstance(esquerda, (A.Id, A.Index)):
            raise ParseError(
                "lado esquerdo da atribuição não é atribuível; esperado identificador ou elemento de vetor",
                igual.line,
                igual.column,
            )
        # Recursão (e não laço) porque `=` é associativo à direita: `a = b = 3`
        # tem de virar Assign(a, Assign(b, 3)).
        valor = self._atribuicao()
        return A.Assign(esquerda.line, esquerda.column, esquerda, valor)

    def _logico_ou(self):
        return self._binario_esquerda(self._logico_e, "OR")

    def _logico_e(self):
        return self._binario_esquerda(self._igualdade, "AND")

    def _igualdade(self):
        return self._binario_esquerda(self._relacional, "EQ", "NEQ")

    def _relacional(self):
        return self._binario_esquerda(self._aditiva, "LT", "GT", "LE", "GE")

    def _aditiva(self):
        return self._binario_esquerda(self._multiplicativa, "PLUS", "MINUS")

    def _multiplicativa(self):
        return self._binario_esquerda(self._unaria, "STAR", "SLASH", "PERCENT")

    def _binario_esquerda(self, proximo_nivel, *operadores):
        """Laço genérico de um nível binário associativo à esquerda."""
        no = proximo_nivel()
        while self._check(*operadores):
            op = self._advance()
            direita = proximo_nivel()
            no = A.Binary(op.line, op.column, OPERADORES[op.type], no, direita)
        return no

    def _unaria(self):
        op = self._match("MINUS", "NOT")
        if op is not None:
            # Recursão direta: os unários são associativos à direita (`--x`).
            operando = self._unaria()
            return A.Unary(op.line, op.column, OPERADORES[op.type], operando)
        return self._posfixa()

    def _posfixa(self):
        """`primario` seguido de qualquer número de `[...]` e `(...)`."""
        no = self._primario()
        while True:
            if self._match("LBRACKET"):
                indice = self._expressao()
                self._expect("RBRACKET")
                no = A.Index(no.line, no.column, no, indice)
                continue
            if self._match("LPAREN"):
                args = []
                if not self._check("RPAREN"):
                    args.append(self._expressao())
                    while self._match("COMMA"):
                        args.append(self._expressao())
                self._expect("RPAREN", "FECHA_PAREN")
                no = A.Call(no.line, no.column, no, args)
                continue
            return no

    def _primario(self):
        token = self._peek()
        tipo = token.type

        if tipo == "IDENT":
            self._advance()
            return A.Id(token.line, token.column, token.lexeme)
        if tipo == "INT":
            self._advance()
            return A.Lit(token.line, token.column, "int", token.lexeme)
        if tipo == "FLOAT":
            self._advance()
            return A.Lit(token.line, token.column, "real", token.lexeme)
        if tipo in ("KW_TRUE", "KW_FALSE"):
            self._advance()
            return A.Lit(token.line, token.column, "bool", token.lexeme)
        if tipo == "CHAR":
            self._advance()
            # O lexer entrega o valor já decodificado; aqui o literal é
            # reescrito na forma de origem, com aspas e escapes.
            return A.Lit(
                token.line, token.column, "char", f"'{A.escapar(token.lexeme)}'"
            )
        if tipo == "STRING":
            self._advance()
            return A.Lit(
                token.line, token.column, "string", f'"{A.escapar(token.lexeme)}"'
            )
        if tipo == "LPAREN":
            self._advance()
            interna = self._expressao()
            self._expect("RPAREN", "FECHA_PAREN")
            # Os parênteses não geram nó: a hierarquia da árvore já registra o
            # agrupamento que eles pediram.
            return interna

        self._erro("esperado expressão (identificador, literal ou ABRE_PAREN)")


def parse(tokens):
    """Atalho: analisa uma lista de tokens e devolve `(Program, erros)`."""
    return Parser(tokens).parse()


def parse_source(source: str):
    """Do texto à AST: `(programa, erros_lexicos, erros_sintaticos)`.

    Se houver erro léxico, o parser **não** roda: o fluxo de tokens já está
    corrompido e só produziria erros sintáticos em cascata, mascarando a causa
    real (Seção 12 da especificação).
    """
    tokens, erros_lexicos = Lexer(source).tokenize()
    if erros_lexicos:
        return None, erros_lexicos, []
    programa, erros_sintaticos = parse(tokens)
    return programa, [], erros_sintaticos


def main(argv: list[str]) -> int:
    # Mesmo cuidado de main.py: forçar UTF-8 para os acentos das mensagens
    # saírem iguais em qualquer console.
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")

    modo = "sexpr"
    args = []
    for arg in argv[1:]:
        if arg in ("--tree", "-t"):
            modo = "tree"
        elif arg == "--tokens":
            modo = "tokens"
        elif arg in ("--help", "-h"):
            print(__doc__.strip())
            return 0
        else:
            args.append(arg)

    if len(args) != 1:
        print("uso: python parser.py [--tree|--tokens] <arquivo.c>", file=sys.stderr)
        return 1

    caminho = Path(args[0])
    try:
        source = caminho.read_text(encoding="utf-8")
    except OSError as exc:
        print(f"erro ao abrir '{caminho}': {exc}", file=sys.stderr)
        return 1

    tokens, erros_lexicos = Lexer(source).tokenize()

    if modo == "tokens":
        for token in tokens:
            if token.type != "EOF":
                print(token)
        for erro in erros_lexicos:
            print(erro, file=sys.stderr)
        return 2 if erros_lexicos else 0

    if erros_lexicos:
        for erro in erros_lexicos:
            print(erro, file=sys.stderr)
        return 2

    programa, erros = parse(tokens)

    if erros:
        # Entrada rejeitada: nenhuma AST é impressa (o pacote de testes é
        # explícito quanto a isso) e o código de saída é 3.
        for erro in erros:
            print(erro, file=sys.stderr)
        return 3

    print(A.to_tree(programa) if modo == "tree" else A.to_sexpr(programa))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
