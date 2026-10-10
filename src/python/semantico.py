"""Analisador semântico do MINIC (etapa 3): nomes, escopos, tipos e contextos.

Recebe a AST de um programa **sintaticamente válido** e devolve a lista de
diagnósticos semânticos (vazia = programa aceito). Segue a apostila da aula 15
e o gabarito da "AP 2 - Analisador Semântico"
(ref/testes-oficiais/testes-semanticos-20/GABARITO.md), que fixa o texto das
mensagens; docs/semantica.md descreve todas as regras e decisões.

Duas passagens sobre a AST (seção 2 da apostila):

- **A — coleta**: todas as funções (assinaturas) e variáveis globais entram no
  escopo global antes de qualquer corpo ser visitado. Funções ficam visíveis
  no programa inteiro (recursão direta e mútua); variáveis globais só a partir
  do ponto em que são declaradas ("declarar antes de usar").
- **B — verificação**: visita declarações, corpos e comandos na ordem do
  código, abrindo e fechando escopos, sintetizando o tipo de cada expressão e
  checando os contextos (condição, laço, retorno, atribuibilidade). No fim de
  cada função não `void`, uma análise simples de fluxo decide se o corpo pode
  terminar sem `return`.

Tipos são pares `(base, eh_vetor)`. `ERRO` é o tipo sentinela de um nó que já
gerou diagnóstico: quem o recebe não reclama de novo (evita cascata); `VOID` é
o "tipo" de uma chamada a função `void`, que só é aceito como comando.

Os diagnósticos são ordenados por posição de origem (linha, coluna) e, no
empate, pela ordem em que foram encontrados.
"""

from dataclasses import dataclass

import minic_ast as A
from tabela_simbolos import FUNCAO, PARAMETRO, VARIAVEL, VETOR, Simbolo, TabelaSimbolos

INT = ("int", False)
FLOAT = ("float", False)
BOOL = ("bool", False)
CHAR = ("char", False)
STRING = ("string", False)
VOID = ("void", False)
ERRO = ("erro", False)

# Tipo de cada categoria de literal da AST (`real` é o nome do token FLOAT).
TIPO_LITERAL = {"int": INT, "real": FLOAT, "bool": BOOL, "char": CHAR, "string": STRING}

# Como cada literal é descrito quando aparece como destino de atribuição.
DESCRICAO_LITERAL = {
    "int": "o literal inteiro",
    "real": "o literal real",
    "bool": "o literal lógico",
    "char": "o literal de caractere",
    "string": "a cadeia",
}

# Precedência dos operadores binários (Seção 4.3 da especificação), usada só
# para reescrever expressões nas mensagens com os parênteses necessários.
PRECEDENCIA = {
    "||": 1,
    "&&": 2,
    "==": 3,
    "!=": 3,
    "<": 4,
    ">": 4,
    "<=": 4,
    ">=": 4,
    "+": 5,
    "-": 5,
    "*": 6,
    "/": 6,
    "%": 6,
}

# Maior valor que um tamanho/índice constante pode assumir nas mensagens. É o
# LLONG_MAX do C: a versão em C converte com strtoll, que satura nesse valor,
# e as duas implementações precisam imprimir o mesmo número.
MAIOR_CONSTANTE = 2**63 - 1


@dataclass
class Diagnostico:
    codigo: str
    linha: int
    coluna: int
    mensagem: str

    def __str__(self):
        return f"{self.codigo} — linha {self.linha}, coluna {self.coluna}: {self.mensagem}"


# -- tipos -------------------------------------------------------------------


def nome_tipo(tipo) -> str:
    base, eh_vetor = tipo
    return f"vetor de {base}" if eh_vetor else base


def numerico(tipo) -> bool:
    """int, float e char (que é promovido a int, Seção 5 da especificação)."""
    return not tipo[1] and tipo[0] in ("int", "float", "char")


def inteiro(tipo) -> bool:
    return not tipo[1] and tipo[0] in ("int", "char")


def compativel(valor, esperado) -> bool:
    """Um valor do tipo `valor` pode ser usado onde se espera `esperado`?

    Ponto único da regra de compatibilidade (atribuição, inicialização,
    argumento, retorno e índice), como pede a seção 4 da apostila: tipos
    iguais, mais as duas conversões implícitas da especificação — int → float
    e char → int. Vetores nunca são compatíveis aqui (não há atribuição de
    vetor inteiro; parâmetro vetor é tratado à parte na chamada).
    """
    if valor[1] or esperado[1]:
        return False
    if valor == esperado:
        return True
    return (valor, esperado) in ((INT, FLOAT), (CHAR, INT))


def constante_inteira(no):
    """Valor de um literal inteiro, com sinal opcional (`-1`), ou None."""
    negativo = False
    if isinstance(no, A.Unary) and no.op == "-":
        negativo = True
        no = no.operand
    if not (isinstance(no, A.Lit) and no.kind == "int"):
        return None
    valor = min(int(no.text), MAIOR_CONSTANTE)
    return -valor if negativo else valor


def zero_constante(no) -> bool:
    if not isinstance(no, A.Lit):
        return False
    if no.kind == "int":
        return int(no.text) == 0
    if no.kind == "real":
        return float(no.text) == 0.0
    return False


# -- expressões como texto e posição de início -------------------------------


def texto(no) -> str:
    """Reescreve uma expressão como código-fonte, para citá-la na mensagem.

    A AST não guarda parênteses; aqui eles são recolocados só onde a
    precedência exige, com um espaço em volta de cada operador binário
    (`1 < 2`, `(a + b) * c`).
    """
    if isinstance(no, A.Lit):
        return no.text
    if isinstance(no, A.Id):
        return no.name
    if isinstance(no, A.Index):
        return f"{_texto_posfixo(no.base)}[{texto(no.index)}]"
    if isinstance(no, A.Call):
        args = ", ".join(texto(a) for a in no.args)
        return f"{_texto_posfixo(no.callee)}({args})"
    if isinstance(no, A.Unary):
        operando = texto(no.operand)
        if isinstance(no.operand, (A.Binary, A.Assign)):
            operando = f"({operando})"
        return f"{no.op}{operando}"
    if isinstance(no, A.Binary):
        nivel = PRECEDENCIA[no.op]
        esquerda = texto(no.left)
        if isinstance(no.left, A.Assign) or (
            isinstance(no.left, A.Binary) and PRECEDENCIA[no.left.op] < nivel
        ):
            esquerda = f"({esquerda})"
        direita = texto(no.right)
        # `<=` à direita: os binários são associativos à esquerda, então
        # `a - (b - c)` precisa dos parênteses e `(a - b) - c` não.
        if isinstance(no.right, A.Assign) or (
            isinstance(no.right, A.Binary) and PRECEDENCIA[no.right.op] <= nivel
        ):
            direita = f"({direita})"
        return f"{esquerda} {no.op} {direita}"
    if isinstance(no, A.Assign):
        return f"{texto(no.target)} = {texto(no.value)}"
    raise TypeError(f"não é expressão: {type(no).__name__}")


def _texto_posfixo(no) -> str:
    """Base de `[...]` ou `(...)`: só um primário dispensa parênteses."""
    if isinstance(no, (A.Binary, A.Unary, A.Assign)):
        return f"({texto(no)})"
    return texto(no)


def inicio(no) -> tuple[int, int]:
    """Posição do primeiro token da expressão.

    O nó `Binary` guarda a posição do operador; uma mensagem sobre a
    expressão inteira (como "Argumento 1 ... (expressão “1 < 2”)") aponta
    para onde ela começa — o operando mais à esquerda.
    """
    while True:
        if isinstance(no, A.Binary):
            no = no.left
        elif isinstance(no, A.Assign):
            no = no.target
        elif isinstance(no, A.Index):
            no = no.base
        elif isinstance(no, A.Call):
            no = no.callee
        else:
            return no.line, no.column


# -- análise de fluxo (retorno em todos os caminhos) -------------------------


def completa(comando) -> bool:
    """O comando pode terminar normalmente (seguir para o próximo)?

    Regra conservadora da seção 10 da apostila: só `return` (e os desvios
    `break`/`continue`) interrompem; `if` sem `else` sempre pode cair pelo ramo
    ausente; laços podem executar zero vezes, então nunca garantem retorno.
    """
    if isinstance(comando, (A.Return, A.Break, A.Continue)):
        return False
    if isinstance(comando, A.If):
        if comando.otherwise is None:
            return True
        return completa(comando.then) or completa(comando.otherwise)
    if isinstance(comando, A.Block):
        return all(completa(item) for item in comando.items)
    return True


def motivo_da_queda(comando) -> str:
    """Explica por que `comando` (que `completa`) deixa o fim do corpo alcançável."""
    if isinstance(comando, A.If):
        condicao = texto(comando.cond)
        if comando.otherwise is not None and completa(comando.then):
            return f"o ramo em que “{condicao}” é verdadeiro alcança o fim do corpo"
        return f"o ramo em que “{condicao}” é falso alcança o fim do corpo"
    if isinstance(comando, A.While):
        return f"o laço while da linha {comando.line} pode terminar e alcançar o fim do corpo"
    if isinstance(comando, A.For):
        return f"o laço for da linha {comando.line} pode terminar e alcançar o fim do corpo"
    if isinstance(comando, A.Block) and comando.items:
        return motivo_da_queda(comando.items[-1])
    return "o fim do corpo é alcançado sem return"


# -- o analisador ------------------------------------------------------------


class Analisador:
    def __init__(self):
        self.tabela = TabelaSimbolos()
        self.diagnosticos: list[Diagnostico] = []
        self.funcao: Simbolo | None = None  # função cujo corpo está sendo visitado
        self.lacos = 0  # profundidade de laços (contexto de break/continue)

    def analisar(self, programa: A.Program) -> list[Diagnostico]:
        self._coletar_globais(programa)
        for item in programa.decls:
            if isinstance(item, A.Function):
                self._funcao(item)
            elif isinstance(item, A.VarDecl):
                self._variavel_global(item)
            else:
                # Comando solto no topo (o parser aceita, por causa do caso
                # oficial 22 da etapa 2): verificado no escopo global, fora de
                # função e de laço.
                self._comando(item)
        # sorted é estável: no empate de posição, vale a ordem de descoberta.
        return sorted(self.diagnosticos, key=lambda d: (d.linha, d.coluna))

    def _erro(self, codigo: str, posicao: tuple[int, int], mensagem: str):
        self.diagnosticos.append(Diagnostico(codigo, posicao[0], posicao[1], mensagem))

    # -- declarações -----------------------------------------------------------

    def _declarar(self, simbolo: Simbolo, posicao: tuple[int, int]):
        anterior = self.tabela.inserir(simbolo)
        if anterior is not None:
            self._erro(
                "SEM002",
                posicao,
                f"“{simbolo.nome}” já declarado neste escopo; declaração anterior na "
                f"linha {anterior.linha}, coluna {anterior.coluna} ({anterior.descricao()}).",
            )
        return anterior is None

    def _coletar_globais(self, programa: A.Program):
        """Passagem A: assinaturas de funções e nomes globais, em ordem de código."""
        for item in programa.decls:
            if isinstance(item, A.Function):
                params = [(p.type, p.is_array) for p in item.params]
                simbolo = Simbolo(
                    item.name, FUNCAO, item.return_type, item.line, item.column, params=params
                )
                self._declarar(simbolo, (item.line, item.column))
            elif isinstance(item, A.VarDecl):
                simbolo = self._simbolo_de_variavel(item)
                simbolo.visivel = False  # passa a valer em _variavel_global
                self._declarar(simbolo, (item.line, item.column))

    def _simbolo_de_variavel(self, decl: A.VarDecl) -> Simbolo:
        if decl.size is None:
            return Simbolo(decl.name, VARIAVEL, decl.type, decl.line, decl.column)
        tamanho = constante_inteira(decl.size)
        if tamanho is not None and tamanho <= 0:
            tamanho = None
        return Simbolo(
            decl.name, VETOR, decl.type, decl.line, decl.column, eh_vetor=True, tamanho=tamanho
        )

    def _variavel_global(self, decl: A.VarDecl):
        simbolo = self.tabela.buscar_no_escopo_atual(decl.name)
        # Só o dono do nome se torna visível; uma redeclaração (já reportada na
        # passagem A) não substitui a primeira.
        if simbolo is not None and (simbolo.linha, simbolo.coluna) == (decl.line, decl.column):
            simbolo.visivel = True
        self._verificar_declaracao(decl)

    def _variavel_local(self, decl: A.VarDecl):
        self._declarar(self._simbolo_de_variavel(decl), (decl.line, decl.column))
        self._verificar_declaracao(decl)

    def _verificar_declaracao(self, decl: A.VarDecl):
        """Tamanho de vetor e inicializador (o nome já está no escopo, como em C)."""
        if decl.size is not None:
            tamanho = constante_inteira(decl.size)
            if not (isinstance(decl.size, A.Lit) and tamanho is not None and tamanho > 0):
                self._erro(
                    "SEM016",
                    inicio(decl.size),
                    f"Tamanho do vetor “{decl.name}” deve ser um literal inteiro positivo; "
                    f"recebeu “{texto(decl.size)}”.",
                )
        if decl.init is not None:
            tipo = self._valor(decl.init, "inicializador")
            self._exigir_compatibilidade(tipo, (decl.type, False), decl.name, decl.init)

    def _exigir_compatibilidade(self, tipo, esperado, destino: str, expressao):
        if tipo == ERRO or esperado == ERRO or compativel(tipo, esperado):
            return
        self._erro(
            "SEM003",
            inicio(expressao),
            f"Não é possível atribuir {nome_tipo(tipo)} a {nome_tipo(esperado)} sem conversão "
            f"permitida (destino “{destino}”; expressão “{texto(expressao)}”).",
        )

    def _funcao(self, funcao: A.Function):
        self.tabela.abrir_escopo()
        for param in funcao.params:
            categoria = VETOR if param.is_array else PARAMETRO
            self._declarar(
                Simbolo(
                    param.name,
                    categoria,
                    param.type,
                    param.line,
                    param.column,
                    eh_vetor=param.is_array,
                ),
                (param.line, param.column),
            )
        # O contexto de retorno vem da própria definição, e não da tabela: se o
        # nome for duplicado (já reportado), o corpo é verificado mesmo assim.
        anterior = self.funcao
        self.funcao = Simbolo(funcao.name, FUNCAO, funcao.return_type, funcao.line, funcao.column)
        # O corpo usa o mesmo escopo dos parâmetros: redeclarar um parâmetro no
        # nível de fora do corpo é duplicidade, como em C.
        for item in funcao.body.items:
            self._item_de_bloco(item)
        self.funcao = anterior
        self.tabela.fechar_escopo()

        if funcao.return_type != "void" and completa(funcao.body):
            self._erro(
                "SEM011",
                (funcao.line, funcao.column),
                f"A função “{funcao.name}” pode terminar sem retornar {funcao.return_type}; "
                f"{motivo_da_queda(funcao.body)}.",
            )

    # -- comandos ----------------------------------------------------------------

    def _item_de_bloco(self, item):
        if isinstance(item, A.VarDecl):
            self._variavel_local(item)
        else:
            self._comando(item)

    def _comando(self, comando):
        if isinstance(comando, A.Block):
            self.tabela.abrir_escopo()
            for item in comando.items:
                self._item_de_bloco(item)
            self.tabela.fechar_escopo()
        elif isinstance(comando, A.ExprStmt):
            if comando.expr is not None:
                self._tipo(comando.expr)  # chamada void é válida como comando
        elif isinstance(comando, A.If):
            self._condicao(comando.cond, "if")
            self._comando(comando.then)
            if comando.otherwise is not None:
                self._comando(comando.otherwise)
        elif isinstance(comando, A.While):
            self._condicao(comando.cond, "while")
            self._laco(comando.body)
        elif isinstance(comando, A.For):
            if comando.init is not None:
                self._tipo(comando.init)
            if comando.cond is not None:
                self._condicao(comando.cond, "for")
            if comando.step is not None:
                self._tipo(comando.step)
            self._laco(comando.body)
        elif isinstance(comando, A.Return):
            self._retorno(comando)
        elif isinstance(comando, (A.Break, A.Continue)):
            if self.lacos == 0:
                palavra = "break" if isinstance(comando, A.Break) else "continue"
                self._erro(
                    "SEM010",
                    (comando.line, comando.column),
                    f"{palavra} só pode ocorrer dentro de um laço.",
                )
        elif isinstance(comando, A.Print):
            tipo = self._valor(comando.arg, "argumento de print")
            if tipo[1]:
                self._erro(
                    "SEM014",
                    inicio(comando.arg),
                    f"print não aceita o vetor “{texto(comando.arg)}” inteiro; "
                    "imprima um elemento por vez.",
                )
        elif isinstance(comando, A.Read):
            self._destino(comando.target, "leitura")
        else:
            raise TypeError(f"comando desconhecido: {type(comando).__name__}")

    def _laco(self, corpo):
        self.lacos += 1
        self._comando(corpo)
        self.lacos -= 1

    def _condicao(self, expressao, comando: str):
        tipo = self._valor(expressao, "condição")
        if tipo != ERRO and tipo != BOOL:
            self._erro(
                "SEM005",
                inicio(expressao),
                f"Condição de {comando} deve ter tipo bool; recebeu {nome_tipo(tipo)} "
                f"(expressão “{texto(expressao)}”).",
            )

    def _retorno(self, comando: A.Return):
        if self.funcao is None:
            self._erro(
                "SEM010",
                (comando.line, comando.column),
                "return só pode ocorrer dentro de uma função.",
            )
            if comando.expr is not None:
                self._tipo(comando.expr)
            return

        nome, esperado = self.funcao.nome, self.funcao.tipo
        if comando.expr is None:
            if esperado != "void":
                self._erro(
                    "SEM009",
                    (comando.line, comando.column),
                    f"Retorno sem valor na função “{nome}”, que deve retornar {esperado}.",
                )
            return

        if esperado == "void":
            self._tipo(comando.expr)
            self._erro(
                "SEM009",
                inicio(comando.expr),
                f"A função “{nome}” é void e não pode retornar valor "
                f"(expressão “{texto(comando.expr)}”).",
            )
            return

        tipo = self._valor(comando.expr, "valor de retorno")
        if tipo != ERRO and not compativel(tipo, (esperado, False)):
            recebido = nome_tipo(tipo)
            self._erro(
                "SEM009",
                inicio(comando.expr),
                f"Retorno {recebido} incompatível com o tipo {esperado} da função “{nome}”; "
                f"conversão implícita de {recebido} para {esperado} não permitida.",
            )

    # -- expressões --------------------------------------------------------------

    def _valor(self, expressao, contexto: str):
        """Tipo de uma expressão usada como valor: chamada void é SEM012."""
        tipo = self._tipo(expressao)
        if tipo == VOID:
            self._erro(
                "SEM012",
                inicio(expressao),
                f"Função “{expressao.callee.name}” não produz valor (retorno void) e não "
                f"pode ser usada como {contexto}.",
            )
            return ERRO
        return tipo

    def _tipo(self, no):
        """Síntese do tipo de uma expressão (seção 5 da apostila)."""
        if isinstance(no, A.Lit):
            return TIPO_LITERAL[no.kind]
        if isinstance(no, A.Id):
            return self._identificador(no)
        if isinstance(no, A.Index):
            return self._indexacao(no)
        if isinstance(no, A.Call):
            return self._chamada(no)
        if isinstance(no, A.Unary):
            return self._unario(no)
        if isinstance(no, A.Binary):
            return self._binario(no)
        if isinstance(no, A.Assign):
            return self._atribuicao(no)
        raise TypeError(f"não é expressão: {type(no).__name__}")

    def _resolver(self, no: A.Id) -> Simbolo | None:
        simbolo = self.tabela.buscar(no.name)
        if simbolo is None:
            self._erro(
                "SEM001",
                (no.line, no.column),
                f"Identificador “{no.name}” não declarado neste escopo.",
            )
        return simbolo

    def _identificador(self, no: A.Id):
        simbolo = self._resolver(no)
        if simbolo is None:
            return ERRO
        if simbolo.categoria == FUNCAO:
            self._erro(
                "SEM014",
                (no.line, no.column),
                f"A função “{no.name}” só pode ser usada em chamada.",
            )
            return ERRO
        return (simbolo.tipo, simbolo.eh_vetor)

    def _indexacao(self, no: A.Index):
        base_tipo = ERRO
        simbolo = None
        if isinstance(no.base, A.Id):
            simbolo = self._resolver(no.base)
            if simbolo is not None:
                if simbolo.eh_vetor:
                    base_tipo = (simbolo.tipo, True)
                else:
                    self._erro(
                        "SEM014",
                        (no.base.line, no.base.column),
                        f"“{simbolo.nome}” não é um vetor e não pode ser indexado; foi "
                        f"declarado como {simbolo.categoria} na linha {simbolo.linha}, "
                        f"coluna {simbolo.coluna}.",
                    )
        else:
            tipo = self._valor(no.base, "operando")
            if tipo[1]:
                base_tipo = tipo
            elif tipo != ERRO:
                self._erro(
                    "SEM014",
                    inicio(no.base),
                    f"A expressão “{texto(no.base)}” não é um vetor e não pode ser indexada.",
                )

        indice = self._valor(no.index, "índice")
        if indice != ERRO and not compativel(indice, INT):
            self._erro(
                "SEM006",
                inicio(no.index),
                f"Índice do vetor “{texto(no.base)}” deve ser int; recebeu {nome_tipo(indice)} "
                f"(expressão “{texto(no.index)}”).",
            )
        elif simbolo is not None and simbolo.eh_vetor and simbolo.tamanho is not None:
            valor = constante_inteira(no.index)
            if valor is not None and not 0 <= valor < simbolo.tamanho:
                self._erro(
                    "SEM016",
                    inicio(no.index),
                    f"Índice “{texto(no.index)}” fora dos limites do vetor “{simbolo.nome}” "
                    f"(tamanho {simbolo.tamanho}).",
                )

        return (base_tipo[0], False) if base_tipo != ERRO else ERRO

    def _chamada(self, no: A.Call):
        funcao = None
        if not isinstance(no.callee, A.Id):
            if self._tipo(no.callee) != ERRO:
                self._erro(
                    "SEM014",
                    inicio(no.callee),
                    f"A expressão “{texto(no.callee)}” não designa uma função.",
                )
        else:
            simbolo = self._resolver(no.callee)
            if simbolo is not None and simbolo.categoria != FUNCAO:
                self._erro(
                    "SEM014",
                    (no.callee.line, no.callee.column),
                    f"“{simbolo.nome}” não designa uma função; foi declarado como "
                    f"{simbolo.categoria} na linha {simbolo.linha}, coluna {simbolo.coluna}.",
                )
            elif simbolo is not None:
                funcao = simbolo

        tipos = [self._tipo(arg) for arg in no.args]
        if funcao is None:
            for arg, tipo in zip(no.args, tipos):
                self._sem_valor(arg, tipo, "argumento")
            return ERRO

        esperados, recebidos = len(funcao.params), len(no.args)
        if esperados != recebidos:
            plural = "" if esperados == 1 else "s"
            self._erro(
                "SEM007",
                inicio(no),
                f"“{funcao.nome}” espera {esperados} argumento{plural}, "
                f"mas recebeu {recebidos}.",
            )
            for arg, tipo in zip(no.args, tipos):
                self._sem_valor(arg, tipo, "argumento")
        else:
            for posicao, (arg, tipo, param) in enumerate(
                zip(no.args, tipos, funcao.params), start=1
            ):
                if self._sem_valor(arg, tipo, "argumento") or tipo == ERRO:
                    continue
                base, eh_vetor = param
                if eh_vetor:
                    aceito = tipo == (base, True)
                else:
                    aceito = compativel(tipo, (base, False))
                if not aceito:
                    self._erro(
                        "SEM008",
                        inicio(arg),
                        f"Argumento {posicao} de “{funcao.nome}”: esperado "
                        f"{nome_tipo(param)}, recebido {nome_tipo(tipo)} "
                        f"(expressão “{texto(arg)}”).",
                    )

        return VOID if funcao.tipo == "void" else (funcao.tipo, False)

    def _sem_valor(self, expressao, tipo, contexto: str) -> bool:
        """Reporta SEM012 se `tipo` vier de uma chamada void; diz se reportou."""
        if tipo != VOID:
            return False
        self._erro(
            "SEM012",
            inicio(expressao),
            f"Função “{expressao.callee.name}” não produz valor (retorno void) e não "
            f"pode ser usada como {contexto}.",
        )
        return True

    def _unario(self, no: A.Unary):
        tipo = self._valor(no.operand, "operando")
        if tipo == ERRO:
            return ERRO
        if no.op == "-":
            if numerico(tipo):
                return FLOAT if tipo == FLOAT else INT
            exigencia = "operando numérico"
        else:  # "!"
            if tipo == BOOL:
                return BOOL
            exigencia = "operando bool"
        self._erro(
            "SEM004",
            (no.line, no.column),
            f"Operador {no.op} requer {exigencia}; recebeu {nome_tipo(tipo)}.",
        )
        return ERRO

    def _binario(self, no: A.Binary):
        esquerda = self._valor(no.left, "operando")
        direita = self._valor(no.right, "operando")
        if esquerda == ERRO or direita == ERRO:
            return ERRO

        op = no.op
        if op in ("+", "-", "*", "/"):
            valido = numerico(esquerda) and numerico(direita)
            resultado = FLOAT if FLOAT in (esquerda, direita) else INT
            exigencia = "operandos numéricos"
        elif op == "%":
            valido = inteiro(esquerda) and inteiro(direita)
            resultado = INT
            exigencia = "operandos int"
        elif op in ("<", ">", "<=", ">="):
            valido = numerico(esquerda) and numerico(direita)
            resultado = BOOL
            exigencia = "operandos numéricos"
        elif op in ("==", "!="):
            valido = (numerico(esquerda) and numerico(direita)) or (
                esquerda == BOOL and direita == BOOL
            )
            resultado = BOOL
            exigencia = "operandos de tipos compatíveis"
        else:  # "&&", "||"
            valido = esquerda == BOOL and direita == BOOL
            resultado = BOOL
            exigencia = "operandos bool"

        if not valido:
            self._erro(
                "SEM004",
                (no.line, no.column),
                f"Operador {op} requer {exigencia}; recebeu {nome_tipo(esquerda)} "
                f"e {nome_tipo(direita)}.",
            )
            return ERRO

        if op in ("/", "%") and zero_constante(no.right):
            operacao = "Divisão" if op == "/" else "Resto de divisão"
            self._erro(
                "SEM015",
                inicio(no.right),
                f"{operacao} por zero: o divisor é a constante “{texto(no.right)}”.",
            )
        return resultado

    def _atribuicao(self, no: A.Assign):
        destino = self._destino(no.target, "atribuição")
        valor = self._valor(no.value, "expressão de atribuição")
        self._exigir_compatibilidade(valor, destino, texto(no.target), no.value)
        return destino

    def _destino(self, alvo, comando: str):
        """Tipo de um destino de atribuição/leitura, se ele for atribuível (valor-L)."""
        if isinstance(alvo, A.Index):
            return self._tipo(alvo)
        if isinstance(alvo, A.Id):
            simbolo = self._resolver(alvo)
            if simbolo is None:
                return ERRO
            if simbolo.categoria == FUNCAO:
                motivo = f"a função “{alvo.name}” não designa uma variável ou elemento de vetor."
            elif simbolo.eh_vetor:
                motivo = (
                    f"“{alvo.name}” é um vetor, e só seus elementos "
                    f"(“{alvo.name}[i]”) podem receber valores."
                )
            else:
                return (simbolo.tipo, False)
            self._erro(
                "SEM013",
                (alvo.line, alvo.column),
                f"Destino de {comando} não é atribuível; {motivo}",
            )
            return ERRO

        if isinstance(alvo, A.Lit):
            descricao = f"{DESCRICAO_LITERAL[alvo.kind]} “{alvo.text}”"
        else:
            self._tipo(alvo)  # erros próprios da expressão (nome não declarado, ...)
            if isinstance(alvo, A.Call):
                descricao = f"a chamada “{texto(alvo)}”"
            else:
                descricao = f"a expressão “{texto(alvo)}”"
        self._erro(
            "SEM013",
            inicio(alvo),
            f"Destino de {comando} não é atribuível; {descricao} não designa uma variável "
            "ou elemento de vetor.",
        )
        return ERRO


def analisar(programa: A.Program) -> list[Diagnostico]:
    """Atalho: diagnósticos semânticos do programa, já ordenados por posição."""
    return Analisador().analisar(programa)


def relatorio(diagnosticos: list[Diagnostico]) -> str:
    """Texto da saída, no formato do gabarito da AP 2.

    Uma linha por diagnóstico e a linha de resumo no fim. As linhas são
    separadas por CRLF e **não** há quebra depois da última: é exatamente o que
    está nos arquivos `.gabarito` oficiais, e o script `testes_semanticos_*.sh`
    os compara byte a byte com `diff`, sem normalizar nada.
    """
    n = len(diagnosticos)
    contagem = f"{n} erro" if n == 1 else f"{n} erros"
    situacao = "programa rejeitado" if n else "programa aceito"
    linhas = [str(d) for d in diagnosticos]
    linhas.append(f"Análise semântica concluída: {contagem}; {situacao}.")
    return "\r\n".join(linhas)
