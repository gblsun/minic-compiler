"""Gera docs/testes-semanticos.md: cada teste da etapa 3 com objetivo, código e resultado.

    python tests/gerar_catalogo_semantico.py           # regrava o catálogo
    python tests/gerar_catalogo_semantico.py --checar  # falha se estiver desatualizado (CI)

As orientações da AP 2 pedem "testes propostos com os respectivos
resultados". O catálogo é montado a partir dos próprios arquivos de teste — as
entradas `.c`, os `.gabarito` oficiais e os golden de
`tests/semantic_expected/` —, então o que está documentado é exatamente o que
a suíte confere. Só o objetivo de cada caso próprio é escrito à mão, em
OBJETIVOS abaixo.
"""

import re
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
OFICIAIS = RAIZ / "ref" / "testes-oficiais" / "testes-semanticos-20"
PROPRIOS = RAIZ / "tests" / "semantic_inputs"
ESPERADO = RAIZ / "tests" / "semantic_expected"
SAIDA = RAIZ / "docs" / "testes-semanticos.md"

OBJETIVOS = {
    "aceito_programa_completo": "Programa válido que usa todo o núcleo da linguagem: global com inicializador, funções, vetor, `for`, `while` com `break`/`continue`, promoção `int → float`, `char → int`, `read`, `print` de expressão e de cadeia, `%`.",
    "aceito_recursao_mutua": "Recursão mútua: `par` chama `impar`, que é definida depois. Só passa porque as assinaturas são coletadas antes dos corpos (passagem A). Também cobre `if`/`else` com `return` nos dois ramos (um deles dentro de bloco).",
    "aceito_spec_13_1_programa_valido": "O programa válido completo da Seção 13.1 da especificação.",
    "aceito_spec_19_1_fatorial": "Programa de referência 19.1 da especificação (fatorial recursivo). O corpo termina com `return` depois de um `if` sem `else`, então não há queda.",
    "aceito_spec_19_2_entrada_vetor_repeticao": "Programa de referência 19.2 da especificação: `read` em elemento de vetor dentro de `for`.",
    "erro_break_continue_return_fora": "SEM010 nas três formas: `return` no topo do arquivo, `break` e `continue` fora de laço. Os mesmos comandos dentro do `while` são aceitos.",
    "erro_cascata_e_multiplos": "Vários erros independentes no mesmo programa, em ordem de posição, e a supressão de cascata: `(ausente + 1) * true` gera só o SEM001, sem o SEM004 derivado. `if (n = 1)` mostra que a atribuição tem o tipo do destino.",
    "erro_categoria": "SEM013 e SEM014, uso de um nome fora da sua categoria: chamar variável, indexar escalar, função como valor, atribuir a vetor inteiro e a função, `print` e `read` de vetor inteiro.",
    "erro_chamadas": "SEM007 com argumentos a menos e a mais; SEM008 com vetor esperado e escalar recebido, e com `float` para `int`; SEM012 nos contextos de argumento, operando, condição e argumento de `print`; SEM009 no `return` de uma chamada `float` em função `int`.",
    "erro_char_e_conversoes": "Conversões: `char → int` é aceita (inicializador, índice, aritmética); `int → char`, `char → float` e `int → bool` não são.",
    "erro_colunas_apos_utf8": "Colunas contadas em caracteres: acentos numa cadeia, num literal `'é'` e num comentário antes do erro na mesma linha. Python e C têm de apontar a mesma coluna.",
    "erro_destinos": "SEM013 para cada tipo de destino inválido: literal real, lógico e de caractere, chamada e expressão.",
    "erro_divisao_por_zero": "SEM015: divisor constante zero em `/` e `%`, inteiro e real. `10 / 2` é aceito.",
    "erro_escopos_e_duplicidade": "SEM002 global, entre parâmetros, entre parâmetro e local do corpo, e entre funções; sombreamento válido em bloco interno; nome que sai de escopo (SEM001); global usada antes da declaração (SEM001).",
    "erro_lexico_antes_da_semantica": "Erro léxico: a análise semântica não roda, a mensagem vai para stderr e o código de saída é 2.",
    "erro_operadores": "SEM004 para cada família de operador: aritmético, `%`, lógico, `!`, `-` unário, relacional e igualdade.",
    "erro_retornos": "SEM009 nas três formas (valor em `void`, `return;` em não `void`, tipo incompatível) e SEM011 com cada explicação: laço `while`, corpo sem `return`, ramo verdadeiro do `if`. A promoção `int → float` no `return` é aceita.",
    "erro_sintatico_antes_da_semantica": "Erro sintático: a análise semântica não roda (o `y` não declarado não é reportado), a mensagem vai para stderr e o código de saída é 3.",
    "erro_vetor_tamanho_e_limites": "SEM016: tamanho de vetor zero e negativo; índice constante fora dos limites (acima e negativo), na escrita e na leitura. `b[2]` num vetor de 3 é aceito.",
}


def bloco(texto: str, linguagem: str = "") -> str:
    return f"```{linguagem}\n{texto.rstrip()}\n```"


def codigos(saida: str) -> str:
    achados = re.findall(r"^(SEM\d{3})", saida, re.M)
    return ", ".join(achados) if achados else "—"


def gerar() -> str:
    partes = [
        "# Testes da etapa 3 — objetivo, código e resultado",
        "",
        "> Gerado por [`tests/gerar_catalogo_semantico.py`](../tests/gerar_catalogo_semantico.py)",
        "> a partir dos próprios arquivos de teste. Não edite à mão: rode o script.",
        "",
        "As orientações da AP 2 pedem os **testes propostos com os respectivos",
        "resultados**. Este catálogo mostra cada teste e a saída que as duas",
        "implementações produzem, idêntica byte a byte. As linhas do relatório são",
        "separadas por CRLF na saída real; aqui aparecem como linhas normais.",
        "",
        "Como conferir: `python tests/run_semantic_tests.py` (as duas versões e a",
        "equivalência) ou os scripts oficiais da AP 2 (ver",
        "[entrega-ap2.md](entrega-ap2.md)).",
        "",
        "## Resumo",
        "",
        "| Teste | Resultado | Diagnósticos |",
        "|---|---|---|",
    ]

    oficiais = sorted(OFICIAIS.glob("*.c"))
    proprios = sorted(PROPRIOS.glob("*.c"))
    for caminho in oficiais:
        saida = caminho.with_suffix(".gabarito").read_text(encoding="utf-8").replace("\r\n", "\n")
        resultado = "aceito" if "programa aceito" in saida else "rejeitado"
        partes.append(f"| [oficial {caminho.stem}](#{ancora('oficial ' + caminho.stem)}) | {resultado} | {codigos(saida)} |")
    for caminho in proprios:
        stdout = (ESPERADO / f"{caminho.stem}.stdout.txt").read_text(encoding="utf-8").replace("\r\n", "\n")
        codigo = (ESPERADO / f"{caminho.stem}.exit.txt").read_text(encoding="utf-8").strip()
        resultado = {"0": "aceito", "2": "erro léxico", "3": "erro sintático", "4": "rejeitado"}[codigo]
        partes.append(f"| [{caminho.stem}](#{ancora(caminho.stem)}) | {resultado} | {codigos(stdout)} |")

    partes += [
        "",
        "## Casos inválidos obrigatórios da especificação (Seção 13.2)",
        "",
        "| Caso exigido | Onde está coberto |",
        "|---|---|",
        "| variável não declarada | oficial 12; `erro_cascata_e_multiplos`, `erro_escopos_e_duplicidade` |",
        "| declaração duplicada | oficial 13; `erro_escopos_e_duplicidade` |",
        "| tipos incompatíveis | oficiais 11 e 17; `erro_char_e_conversoes`, `erro_operadores` |",
        "| chamada com número incorreto de argumentos | oficial 16; `erro_chamadas` |",
        "| `break` fora de laço | `erro_break_continue_return_fora` |",
        "| retorno incompatível | oficial 19; `erro_retornos` |",
        "| acesso de vetor com índice não inteiro | oficial 15 |",
        "",
        "Os casos léxicos e sintáticos da mesma lista são cobertos pelas suítes das",
        "etapas 1 e 2 (`tests/inputs/`, `tests/parser_inputs/` e os 50 casos oficiais",
        "do parser).",
        "",
        "## Casos oficiais da AP 2",
        "",
        "Entrada e gabarito de `ref/testes-oficiais/testes-semanticos-20/`. A",
        "explicação de cada caso é a do",
        "[GABARITO.md](../ref/testes-oficiais/testes-semanticos-20/GABARITO.md).",
    ]
    for caminho in oficiais:
        saida = caminho.with_suffix(".gabarito").read_text(encoding="utf-8").replace("\r\n", "\n")
        partes += [
            "",
            f"### oficial {caminho.stem}",
            "",
            bloco(caminho.read_text(encoding="utf-8"), "c"),
            "",
            "Saída (stdout):",
            "",
            bloco(saida, "text"),
        ]

    partes += ["", "## Casos próprios", "", "Entradas de `tests/semantic_inputs/`, golden de `tests/semantic_expected/`."]
    for caminho in proprios:
        nome = caminho.stem
        stdout = (ESPERADO / f"{nome}.stdout.txt").read_text(encoding="utf-8").replace("\r\n", "\n")
        stderr = (ESPERADO / f"{nome}.stderr.txt").read_text(encoding="utf-8")
        codigo = (ESPERADO / f"{nome}.exit.txt").read_text(encoding="utf-8").strip()
        partes += [
            "",
            f"### {nome}",
            "",
            f"**Objetivo.** {OBJETIVOS[nome]}",
            "",
            bloco(caminho.read_text(encoding="utf-8"), "c"),
            "",
        ]
        if stdout:
            partes += ["Saída (stdout):", "", bloco(stdout, "text"), ""]
        if stderr:
            partes += ["Saída de erro (stderr):", "", bloco(stderr, "text"), ""]
        partes.append(f"Código de saída: `{codigo}`.")
    return "\n".join(partes) + "\n"


def ancora(titulo: str) -> str:
    """Âncora no estilo do GitHub: minúsculas, espaço vira hífen, sem pontuação."""
    titulo = titulo.lower()
    titulo = re.sub(r"[^\w\- ]", "", titulo)
    return titulo.replace(" ", "-")


def main(argv) -> int:
    faltando = sorted({p.stem for p in PROPRIOS.glob("*.c")} - set(OBJETIVOS))
    if faltando:
        print(f"caso próprio sem objetivo em OBJETIVOS: {', '.join(faltando)}", file=sys.stderr)
        return 1
    texto = gerar()
    if "--checar" in argv:
        atual = SAIDA.read_text(encoding="utf-8") if SAIDA.exists() else ""
        if atual.replace("\r\n", "\n") != texto:
            print("docs/testes-semanticos.md desatualizado: rode python tests/gerar_catalogo_semantico.py",
                  file=sys.stderr)
            return 1
        print("catálogo de testes atualizado")
        return 0
    SAIDA.write_text(texto, encoding="utf-8", newline="\n")
    print(f"gravado {SAIDA.relative_to(RAIZ)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
