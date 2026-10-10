"""Tabela de símbolos do MINIC: escopos léxicos aninhados (aula 14).

A tabela é uma **pilha de escopos**. Cada escopo é um dicionário nome →
`Simbolo`; abrir um bloco empilha um escopo novo, fechar o bloco desempilha.
As operações são as da Seção 6.2 da especificação:

- `inserir(simbolo)` no escopo atual (devolve o símbolo anterior, se o nome já
  estava declarado **neste** escopo — é assim que a duplicidade é detectada);
- `buscar(nome)`, do escopo mais interno para o mais externo, o que dá o
  sombreamento de graça: a primeira declaração encontrada é a visível;
- `buscar_no_escopo_atual(nome)`;
- `abrir_escopo()` / `fechar_escopo()`.

O módulo não sabe nada de AST nem de tipos de expressão: ele só guarda
declarações. Quem decide o que é erro é o analisador semântico
(`semantico.py`), que usa a tabela.
"""

from dataclasses import dataclass, field

# Categorias da Seção 6.1 da especificação (CONSTANTE não existe no MINIC
# implementado: a gramática não tem `const`).
VARIAVEL = "variável"
PARAMETRO = "parâmetro"
VETOR = "vetor"
FUNCAO = "função"


@dataclass
class Simbolo:
    """Uma entrada da tabela: tudo o que se sabe de um nome declarado."""

    nome: str
    categoria: str
    # Tipo base: int, float, bool ou char (para função, o tipo de retorno,
    # que também pode ser void).
    tipo: str
    linha: int
    coluna: int
    # Vetores (declarados com `int v[3]` ou parâmetro `int v[]`).
    eh_vetor: bool = False
    tamanho: int | None = None  # None quando desconhecido (parâmetro vetor)
    # Funções: lista ordenada de (tipo, eh_vetor) dos parâmetros.
    params: list = field(default_factory=list)
    # Variáveis globais são coletadas antes de visitar os corpos (para a
    # duplicidade citar a declaração anterior certa), mas só ficam visíveis a
    # partir do ponto em que são declaradas: "declarar antes de usar"
    # (Seção 2 da especificação). Ver Analisador._coletar_globais.
    visivel: bool = True

    def descricao(self) -> str:
        """Como o símbolo aparece na mensagem de duplicidade: `(tipo int)`."""
        if self.categoria == FUNCAO:
            return f"função que retorna {self.tipo}"
        if self.eh_vetor:
            return f"vetor de {self.tipo}"
        return f"tipo {self.tipo}"


class TabelaSimbolos:
    def __init__(self):
        self._escopos: list[dict[str, Simbolo]] = [{}]  # [0] é o escopo global

    @property
    def nivel(self) -> int:
        """Nível léxico atual: 0 é o global."""
        return len(self._escopos) - 1

    def abrir_escopo(self):
        self._escopos.append({})

    def fechar_escopo(self):
        self._escopos.pop()

    def inserir(self, simbolo: Simbolo) -> Simbolo | None:
        """Declara no escopo atual.

        Se o nome já existe neste escopo, **não** substitui: devolve o símbolo
        anterior, e quem chamou reporta a duplicidade. Manter o primeiro evita
        que o erro mude o significado dos usos seguintes (cascata).
        """
        atual = self._escopos[-1]
        anterior = atual.get(simbolo.nome)
        if anterior is not None:
            return anterior
        atual[simbolo.nome] = simbolo
        return None

    def buscar_no_escopo_atual(self, nome: str) -> Simbolo | None:
        return self._escopos[-1].get(nome)

    def buscar(self, nome: str) -> Simbolo | None:
        """Declaração visível mais interna (sombreamento), ou None."""
        for escopo in reversed(self._escopos):
            simbolo = escopo.get(nome)
            if simbolo is not None and simbolo.visivel:
                return simbolo
        return None
