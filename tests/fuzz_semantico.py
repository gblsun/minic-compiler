"""Teste diferencial do pipeline semântico: Python × C em programas gerados.

    python tests/fuzz_semantico.py                       # só Python (oráculos), 600 programas
    python tests/fuzz_semantico.py --c ./minic           # Python × C, 600 programas, semente 2026
    python tests/fuzz_semantico.py --c ./minic -n 2000 --semente 7
    python tests/fuzz_semantico.py --c ./minic --salvar /tmp/falhas

Os testes fixos (tests/run_semantic_tests.py) conferem casos escolhidos à mão.
Este script complementa com volume: gera programas MINIC aleatórios — com
nomes repetidos de propósito (para provocar não declarado, duplicidade e
sombreamento), tipos misturados, vetores, chamadas com aridade e tipos
errados, destinos não atribuíveis, `break` fora de laço, funções sem retorno
— e exige, para **cada** programa:

1. Python e C devolvem exatamente o mesmo stdout, stderr e código de saída;
2. nenhum dos dois quebra: código de saída sempre em {0, 2, 3, 4} e stderr
   vazio quando a análise semântica roda (sem traceback do Python, sem
   relatório de AddressSanitizer/UBSan quando o C é compilado com
   `-fsanitize`, como faz o CI).

A geração é determinística (semente fixa), então uma falha é reproduzível.
"""

import argparse
import random
import subprocess
import sys
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
MINIC_PY = RAIZ / "minic.py"

TIPOS = ["int", "float", "bool", "char"]
# Poucos nomes, para que colisões (duplicidade, sombreamento, uso de função
# como variável) aconteçam com frequência.
NOMES = ["a", "b", "x", "v", "f", "g", "soma", "n"]
OPS_BINARIOS = ["+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "||"]


class Gerador:
    def __init__(self, rng: random.Random):
        self.r = rng

    def nome(self):
        return self.r.choice(NOMES)

    def literal(self):
        escolha = self.r.randrange(8)
        if escolha == 0:
            return str(self.r.choice([0, 1, 2, 3, 5, 10, 99999999999999999999]))
        if escolha == 1:
            return self.r.choice(["0.0", "1.5", "2.0", "0.25"])
        if escolha == 2:
            return self.r.choice(["true", "false"])
        if escolha == 3:
            return self.r.choice(["'a'", "'\\n'", "'é'", "'0'"])
        if escolha == 4:
            return self.r.choice(['"texto"', '"ação"', '""'])
        return str(self.r.randrange(4))

    def expressao(self, profundidade=0):
        if profundidade >= 3:
            return self.r.choice([self.literal, self.nome])()
        escolha = self.r.randrange(10)
        if escolha <= 1:
            return self.literal()
        if escolha <= 3:
            return self.nome()
        if escolha == 4:
            return f"{self.nome()}[{self.expressao(profundidade + 1)}]"
        if escolha == 5:
            args = ", ".join(self.expressao(profundidade + 1) for _ in range(self.r.randrange(4)))
            return f"{self.nome()}({args})"
        if escolha == 6:
            return f"{self.r.choice(['-', '!'])}{self.expressao(profundidade + 1)}"
        if escolha == 7:
            return f"({self.destino()} = {self.expressao(profundidade + 1)})"
        op = self.r.choice(OPS_BINARIOS)
        esquerda = self.expressao(profundidade + 1)
        direita = self.expressao(profundidade + 1)
        # Parênteses às vezes sim, às vezes não: exercita a precedência do
        # parser e a reescrita das expressões nas mensagens.
        if self.r.random() < 0.5:
            return f"({esquerda} {op} {direita})"
        return f"{esquerda} {op} {direita}"

    def destino(self):
        escolha = self.r.randrange(10)
        if escolha <= 5:
            return self.nome()
        if escolha <= 7:
            return f"{self.nome()}[{self.expressao(2)}]"
        # Destinos inválidos, aceitos pelo parser no pipeline semântico.
        return self.r.choice([self.literal(), f"{self.nome()}()", f"({self.nome()} + 1)"])

    def declaracao(self):
        tipo = self.r.choice(TIPOS)
        nome = self.nome()
        escolha = self.r.randrange(4)
        if escolha == 0:
            return f"{tipo} {nome}[{self.r.choice(['3', '1', '0', '2'])}];"
        if escolha == 1:
            return f"{tipo} {nome} = {self.expressao(1)};"
        return f"{tipo} {nome};"

    def comando(self, recuo, profundidade=0):
        p = "    " * recuo
        escolha = self.r.randrange(14 if profundidade < 2 else 9)
        if escolha <= 2:
            return f"{p}{self.destino()} = {self.expressao()};"
        if escolha == 3:
            return f"{p}{self.nome()}({', '.join(self.expressao(1) for _ in range(self.r.randrange(3)))});"
        if escolha == 4:
            return f"{p}return{'' if self.r.random() < 0.2 else ' ' + self.expressao()};"
        if escolha == 5:
            return f"{p}print({self.expressao()});"
        if escolha == 6:
            alvo = self.nome() if self.r.random() < 0.6 else f"{self.nome()}[{self.expressao(2)}]"
            return f"{p}read({alvo});"
        if escolha == 7:
            return f"{p}{self.r.choice(['break', 'continue'])};"
        if escolha == 8:
            return f"{p}{self.declaracao()}"
        if escolha == 9:
            corpo = self.comando(recuo + 1, profundidade + 1)
            texto = f"{p}if ({self.expressao()})\n{corpo}"
            if self.r.random() < 0.5:
                texto += f"\n{p}else\n{self.comando(recuo + 1, profundidade + 1)}"
            return texto
        if escolha == 10:
            return f"{p}while ({self.expressao()})\n{self.comando(recuo + 1, profundidade + 1)}"
        if escolha == 11:
            return (
                f"{p}for ({self.destino()} = {self.expressao(2)}; {self.expressao(1)}; "
                f"{self.nome()} = {self.expressao(2)})\n{self.comando(recuo + 1, profundidade + 1)}"
            )
        return self.bloco(recuo, profundidade + 1)

    def bloco(self, recuo, profundidade=0):
        p = "    " * recuo
        itens = [self.comando(recuo + 1, profundidade) for _ in range(self.r.randrange(1, 6))]
        return f"{p}{{\n" + "\n".join(itens) + f"\n{p}}}"

    def funcao(self):
        tipo = self.r.choice(TIPOS + ["void"])
        params = []
        for _ in range(self.r.randrange(4)):
            sufixo = "[]" if self.r.random() < 0.25 else ""
            params.append(f"{self.r.choice(TIPOS)} {self.nome()}{sufixo}")
        return f"{tipo} {self.nome()}({', '.join(params)})\n{self.bloco(0)}"

    def programa(self):
        partes = [self.declaracao() for _ in range(self.r.randrange(3))]
        partes += [self.funcao() for _ in range(self.r.randrange(1, 5))]
        self.r.shuffle(partes)
        return "\n".join(partes) + "\n"


class GeradorValido:
    """Gera programas **bem tipados**: as duas implementações têm de aceitá-los.

    Acompanha escopos, tipos, vetores (com tamanho) e assinaturas como o
    analisador faz, e só emite o que as regras de docs/semantica.md permitem:
    expressão do tipo pedido (com promoção int → float e char → int onde vale),
    índice literal sempre dentro do vetor, divisor nunca é o literal 0,
    `break`/`continue` só em laço, sombreamento só em bloco interno, e toda
    função não void termina com `return` (o único `return` dela, para que a
    mutação "remove o return" produza exatamente um SEM011).
    """

    def __init__(self, rng: random.Random):
        self.r = rng
        self.contador = 0

    def novo_nome(self, prefixo):
        self.contador += 1
        return f"{prefixo}{self.contador}"

    # -- ambiente ----------------------------------------------------------

    def visiveis(self):
        tudo = {}
        for escopo in self.escopos:
            tudo.update(escopo)
        return tudo

    def variaveis(self, tipo=None, vetor=False):
        return [
            (nome, info)
            for nome, info in self.visiveis().items()
            if info[1] == vetor and (tipo is None or info[0] == tipo)
        ]

    def declarar(self, nome, tipo, vetor=False, tamanho=None):
        self.escopos[-1][nome] = (tipo, vetor, tamanho)

    # -- expressões --------------------------------------------------------

    def literal(self, tipo):
        if tipo == "int":
            return str(self.r.randrange(10))
        if tipo == "float":
            return self.r.choice(["0.5", "1.0", "2.25", "0.0"])
        if tipo == "bool":
            return self.r.choice(["true", "false"])
        return self.r.choice(["'a'", "'z'", "'\\n'", "'é'"])

    def indice(self, tamanho, d):
        idx = self.expr("int", d + 1)
        literal = idx.isdigit() or (idx.startswith("-") and idx[1:].isdigit())
        if literal and tamanho is not None:
            return str(self.r.randrange(tamanho))
        return idx

    def chamada(self, tipo, d):
        """Chamada a uma função que devolve `tipo` (None = qualquer uma), ou None."""
        candidatas = [f for f in self.funcoes if tipo is None or f[1] == tipo]
        self.r.shuffle(candidatas)
        for nome, _, params in candidatas:
            args = self.argumentos(params, d)
            if args is not None:
                return f"{nome}({', '.join(args)})"
        return None

    def argumentos(self, params, d):
        args = []
        for tipo, vetor in params:
            if vetor:
                vetores = self.variaveis(tipo, vetor=True)
                if not vetores:
                    return None
                args.append(self.r.choice(vetores)[0])
            else:
                args.append(self.expr(tipo, d + 1))
        return args

    def expr(self, tipo, d=0):
        """Expressão cujo tipo é aceito onde se espera `tipo`."""
        opcoes = [lambda: self.literal(tipo)]
        escalares = self.variaveis(tipo)
        if escalares:
            opcoes.append(lambda: self.r.choice(escalares)[0])
        vetores = self.variaveis(tipo, vetor=True)
        if vetores:
            def elemento():
                nome, info = self.r.choice(vetores)
                return f"{nome}[{self.indice(info[2], d)}]"
            opcoes.append(elemento)
        if d < 3:
            opcoes.append(lambda: self.chamada(tipo, d) or self.literal(tipo))
            if tipo == "int":
                opcoes += [
                    lambda: f"({self.expr('int', d + 1)} {self.r.choice(['+', '-', '*'])} {self.expr('int', d + 1)})",
                    lambda: f"({self.expr('int', d + 1)} {self.r.choice(['/', '%'])} {self.divisor(d)})",
                    lambda: f"-{self.expr('int', d + 1)}",
                    lambda: f"({self.expr('char', d + 1)} + 1)",
                ]
                inteiros = self.variaveis("int")
                if inteiros:
                    opcoes.append(lambda: f"({self.r.choice(inteiros)[0]} = {self.expr('int', d + 1)})")
            elif tipo == "float":
                opcoes += [
                    lambda: self.expr("int", d + 1),  # promoção int -> float
                    lambda: f"({self.expr('float', d + 1)} {self.r.choice(['+', '-', '*'])} {self.expr('float', d + 1)})",
                    lambda: f"({self.expr('float', d + 1)} / {self.divisor(d)})",
                    lambda: f"-{self.expr('float', d + 1)}",
                ]
            elif tipo == "bool":
                numerico = lambda: self.expr(self.r.choice(["int", "float", "char"]), d + 1)  # noqa: E731
                opcoes += [
                    lambda: f"({numerico()} {self.r.choice(['<', '<=', '>', '>=', '==', '!='])} {numerico()})",
                    lambda: f"({self.expr('bool', d + 1)} {self.r.choice(['&&', '||', '==', '!='])} {self.expr('bool', d + 1)})",
                    lambda: f"!{self.expr('bool', d + 1)}",
                ]
        return self.r.choice(opcoes)()

    def divisor(self, d):
        divisor = self.expr("int", d + 1)
        return "1" if divisor == "0" else divisor

    # -- comandos ----------------------------------------------------------

    def comando(self, recuo, laco, d, permite_decl):
        p = "    " * recuo
        opcoes = ["atrib", "atrib", "print", "chamada", "if", "while", "bloco", "read"]
        if self.variaveis("int"):
            opcoes.append("for")
        if laco:
            opcoes += ["break", "continue"]
        if permite_decl:
            opcoes += ["decl", "decl"]
        if d >= 3:
            opcoes = [o for o in opcoes if o not in ("if", "while", "for", "bloco")] or ["print"]
        escolha = self.r.choice(opcoes)

        if escolha == "decl":
            tipo = self.r.choice(TIPOS)
            nome = self.nome_para_declarar()
            if self.r.random() < 0.25:
                tamanho = self.r.randrange(1, 6)
                self.declarar(nome, tipo, True, tamanho)
                return f"{p}{tipo} {nome}[{tamanho}];"
            self.declarar(nome, tipo)  # antes do inicializador, como em C
            if self.r.random() < 0.5:
                return f"{p}{tipo} {nome} = {self.expr(tipo)};"
            return f"{p}{tipo} {nome};"
        if escolha in ("atrib", "read"):
            alvos = [(n, i[0], None) for n, i in self.variaveis()]
            alvos += [(n, i[0], i[2]) for n, i in self.variaveis(vetor=True)]
            if not alvos:
                return f"{p}print({self.expr('int')});"
            nome, tipo, tamanho = self.r.choice(alvos)
            vetor = self.visiveis()[nome][1]
            alvo = f"{nome}[{self.indice(tamanho, d)}]" if vetor else nome
            if escolha == "read":
                return f"{p}read({alvo});"
            return f"{p}{alvo} = {self.expr(tipo)};"
        if escolha == "print":
            return f"{p}print({self.expr(self.r.choice(TIPOS))});"
        if escolha == "chamada":
            texto = self.chamada(None, 0)
            return f"{p}{texto};" if texto else f"{p}print(\"sem chamada\");"
        if escolha == "if":
            texto = f"{p}if ({self.expr('bool')})\n{self.comando(recuo + 1, laco, d + 1, False)}"
            if self.r.random() < 0.5:
                texto += f"\n{p}else\n{self.comando(recuo + 1, laco, d + 1, False)}"
            return texto
        if escolha == "while":
            return f"{p}while ({self.expr('bool')})\n{self.comando(recuo + 1, True, d + 1, False)}"
        if escolha == "for":
            i = self.r.choice(self.variaveis("int"))[0]
            return (
                f"{p}for ({i} = {self.expr('int', 1)}; {self.expr('bool', 1)}; {i} = {self.expr('int', 1)})\n"
                f"{self.comando(recuo + 1, True, d + 1, False)}"
            )
        if escolha in ("break", "continue"):
            return f"{p}{escolha};"
        return self.bloco(recuo, laco, d + 1)

    def nome_para_declarar(self):
        """Nome novo, ou — em bloco interno — um nome de fora (sombreamento)."""
        # escopos = [global, função, bloco, ...]: só a partir do primeiro bloco
        # interno o nome de fora pode ser redeclarado.
        if len(self.escopos) > 2 and self.r.random() < 0.4:
            de_fora = [n for n in self.visiveis() if n not in self.escopos[-1]]
            if de_fora:
                return self.r.choice(de_fora)
        return self.novo_nome("v")

    def bloco(self, recuo, laco, d):
        p = "    " * recuo
        self.escopos.append({})
        itens = [self.comando(recuo + 1, laco, d, True) for _ in range(self.r.randrange(1, 5))]
        self.escopos.pop()
        return f"{p}{{\n" + "\n".join(itens) + f"\n{p}}}"

    # -- programa ----------------------------------------------------------

    def programa(self):
        """Devolve (texto, funções), em que cada função é um dict usado pela mutação."""
        self.escopos = [{}]
        self.funcoes = []
        for _ in range(self.r.randrange(1, 5)):
            params = []
            for _ in range(self.r.randrange(4)):
                params.append((self.r.choice(TIPOS), self.r.random() < 0.25))
            retorno = self.r.choice(TIPOS + ["void"])
            self.funcoes.append((self.novo_nome("fn"), retorno, params))

        globais = []
        for _ in range(self.r.randrange(3)):
            tipo, nome = self.r.choice(TIPOS), self.novo_nome("g")
            if self.r.random() < 0.3:
                tamanho = self.r.randrange(1, 6)
                self.declarar(nome, tipo, True, tamanho)
                globais.append(f"{tipo} {nome}[{tamanho}];")
            else:
                self.declarar(nome, tipo)
                globais.append(f"{tipo} {nome} = {self.literal(tipo)};")

        funcoes = []
        for nome, retorno, params in self.funcoes:
            self.escopos.append({})
            nomes_params = []
            for tipo, vetor in params:
                # Parâmetro pode sombrear uma global (escopos diferentes).
                pnome = self.novo_nome("p")
                if self.escopos[0] and self.r.random() < 0.2:
                    candidato = self.r.choice(list(self.escopos[0]))
                    if candidato not in self.escopos[-1]:
                        pnome = candidato
                self.declarar(pnome, tipo, vetor, None)
                nomes_params.append(f"{tipo} {pnome}{'[]' if vetor else ''}")
            corpo = [self.comando(1, False, 1, True) for _ in range(self.r.randrange(1, 6))]
            final = f"    return {self.expr(retorno)};" if retorno != "void" else None
            self.escopos.pop()
            funcoes.append(
                {
                    "nome": nome,
                    "retorno": retorno,
                    "params": params,
                    "cabecalho": f"{retorno} {nome}({', '.join(nomes_params)})",
                    "corpo": corpo,
                    "final": final,
                }
            )
        return globais, funcoes


def montar(globais, funcoes) -> str:
    partes = list(globais)
    for f in funcoes:
        linhas = list(f["corpo"])
        if f["final"]:
            linhas.append(f["final"])
        partes.append(f"{f['cabecalho']} {{\n" + "\n".join(linhas) + "\n}")
    return "\n".join(partes) + "\n"


def mutar(rng: random.Random, globais, funcoes):
    """Injeta exatamente um erro semântico; devolve (programa, código esperado).

    Cada mutação é autocontida (usa nomes `mut_*`, que o gerador nunca cria)
    para que o erro injetado seja o único do programa.
    """
    funcoes = [dict(f, corpo=list(f["corpo"])) for f in funcoes]
    alvo = rng.choice(funcoes)
    sem_vetor = [f for f in funcoes if not any(v for _, v in f["params"])]
    void_sem_vetor = [f for f in sem_vetor if f["retorno"] == "void"]
    com_retorno = [f for f in funcoes if f["final"]]

    literal_errado = {"int": "true", "float": "true", "bool": "1", "char": "2.5"}
    literal_certo = {"int": "1", "float": "1.5", "bool": "true", "char": "'c'"}
    retorno_errado = {"int": "2.5", "float": "true", "bool": "1", "char": "2.5"}

    opcoes = {
        "SEM001": "mut_inexistente = 1;",
        "SEM002": "{ int mut_d; float mut_d; }",
        "SEM003": "{ int mut_n; mut_n = 2.5; }",
        "SEM004": "{ int mut_o; mut_o = true * 2; }",
        "SEM005": "while (1) ;",
        "SEM006": "{ int mut_v[3]; mut_v[1.5] = 0; }",
        "SEM010": "break;",
        "SEM013": "3 = 4;",
        "SEM014": "{ int mut_s; mut_s(); }",
        "SEM015": "{ int mut_z; mut_z = 1 / 0; }",
        "SEM016": "{ int mut_w[2]; mut_w[5] = 1; }",
    }
    f = rng.choice(funcoes)
    args = ", ".join("1" for _ in range(len(f["params"]) + 1))
    opcoes["SEM007"] = f"{f['nome']}({args});"
    # SEM008 e SEM012 precisam de uma função com o formato certo; se o programa
    # não tem, uma função auxiliar válida (mut_*) é acrescentada.
    com_params = [f for f in sem_vetor if f["params"]]
    if not com_params:
        auxiliar = {
            "nome": "mut_f",
            "retorno": "int",
            "params": [("int", False), ("float", False)],
            "cabecalho": "int mut_f(int mut_a, float mut_b)",
            "corpo": [],
            "final": "    return mut_a;",
        }
        funcoes.append(auxiliar)
        com_params = [auxiliar]
    f = rng.choice(com_params)
    errado = rng.randrange(len(f["params"]))
    args = [
        (literal_errado if i == errado else literal_certo)[t]
        for i, (t, _) in enumerate(f["params"])
    ]
    opcoes["SEM008"] = f"{f['nome']}({', '.join(args)});"
    if not void_sem_vetor:
        auxiliar = {
            "nome": "mut_acao",
            "retorno": "void",
            "params": [],
            "cabecalho": "void mut_acao()",
            "corpo": [],
            "final": None,
        }
        funcoes.append(auxiliar)
        void_sem_vetor = [auxiliar]
    f = rng.choice(void_sem_vetor)
    args = ", ".join(literal_certo[t] for t, _ in f["params"])
    opcoes["SEM012"] = f"{{ int mut_r; mut_r = {f['nome']}({args}); }}"
    opcoes["SEM009"] = None
    opcoes["SEM011"] = None if com_retorno else "pular"
    if opcoes["SEM011"] == "pular":
        del opcoes["SEM011"]

    codigo = rng.choice(sorted(opcoes))
    if codigo == "SEM009":
        if alvo["final"]:
            alvo["final"] = f"    return {retorno_errado[alvo['retorno']]};"
        else:
            alvo["final"] = "    return 1;"  # void retornando valor
    elif codigo == "SEM011":
        rng.choice(com_retorno)["final"] = None
    else:
        alvo["corpo"].insert(rng.randrange(len(alvo["corpo"]) + 1), "    " + opcoes[codigo])
    return montar(globais, funcoes), codigo


def rodar(comando, entrada: Path):
    p = subprocess.run(comando + [str(entrada)], capture_output=True, cwd=RAIZ, timeout=60)
    return p.returncode, p.stdout, p.stderr


def problemas_de(nome, resultado):
    codigo, _, stderr = resultado
    erros = []
    if codigo not in (0, 2, 3, 4):
        erros.append(f"{nome}: código de saída {codigo}")
    if codigo in (0, 4) and stderr:
        erros.append(f"{nome}: stderr não vazio: {stderr[:300]!r}")
    return erros


ACEITO = "Análise semântica concluída: 0 erros; programa aceito.".encode()
UM_ERRO = "Análise semântica concluída: 1 erro; programa rejeitado.".encode()


def oraculo(modo, esperado, resultado):
    """Confere a resposta conhecida dos modos `valido` e `mutacao`."""
    codigo, stdout, _ = resultado
    if modo == "valido" and (codigo, stdout) != (0, ACEITO):
        return [f"programa bem tipado deveria ser aceito; saiu exit {codigo}: {stdout[:400]!r}"]
    if modo == "mutacao":
        linhas = stdout.split(b"\r\n")
        if codigo != 4 or len(linhas) != 2 or linhas[1] != UM_ERRO or not linhas[0].startswith(esperado.encode()):
            return [f"esperava exatamente um {esperado}; saiu exit {codigo}: {stdout[:400]!r}"]
    return []


def main(argv):
    sys.stdout.reconfigure(encoding="utf-8")
    cli = argparse.ArgumentParser(description="Teste diferencial Python × C do pipeline semântico.")
    cli.add_argument("--c", help="executável do minic em C (sem ele, só a versão Python e os oráculos)")
    cli.add_argument("-n", type=int, default=600, help="quantos programas gerar (padrão 600)")
    cli.add_argument("--semente", type=int, default=2026)
    cli.add_argument("--salvar", help="diretório para guardar os programas que falharem")
    args = cli.parse_args(argv[1:])

    comando_py = [sys.executable, str(MINIC_PY)]
    comando_c = [str(Path(args.c).resolve())] if args.c else None
    rng = random.Random(args.semente)
    aleatorio, valido = Gerador(rng), GeradorValido(rng)

    contagem = {"aleatorio": 0, "valido": 0, "mutacao": 0}
    codigos_mutados = {}
    falhas = 0
    with tempfile.TemporaryDirectory() as tmp:
        for i in range(args.n):
            # Um terço de cada modo: aleatório (só equivalência e robustez),
            # válido (tem de ser aceito) e mutação (exatamente um erro conhecido).
            modo = ("aleatorio", "valido", "mutacao")[i % 3]
            esperado = None
            if modo == "aleatorio":
                fonte = aleatorio.programa()
            else:
                globais, funcoes = valido.programa()
                if modo == "valido":
                    fonte = montar(globais, funcoes)
                else:
                    fonte, esperado = mutar(rng, globais, funcoes)
                    codigos_mutados[esperado] = codigos_mutados.get(esperado, 0) + 1
            contagem[modo] += 1
            arquivo = Path(tmp) / f"prog_{i:04d}.c"
            arquivo.write_text(fonte, encoding="utf-8", newline="\n")

            py = rodar(comando_py, arquivo)
            erros = problemas_de("python", py) + oraculo(modo, esperado, py)
            if comando_c:
                c = rodar(comando_c, arquivo)
                erros += problemas_de("c", c)
                if py != c:
                    erros.append("saídas diferentes entre Python e C")
                    for rotulo, k in (("exit", 0), ("stdout", 1), ("stderr", 2)):
                        if py[k] != c[k]:
                            erros.append(f"  {rotulo}: python={py[k]!r:.300}\n  {' ' * len(rotulo)}  c=     {c[k]!r:.300}")
            if erros:
                falhas += 1
                print(f"[FALHA] programa {i} ({modo}, semente {args.semente})")
                for e in erros:
                    print(f"     {e}")
                if args.salvar:
                    destino = Path(args.salvar)
                    destino.mkdir(parents=True, exist_ok=True)
                    (destino / arquivo.name).write_text(fonte, encoding="utf-8", newline="\n")

    print(
        f"\nProgramas gerados: {args.n} — {contagem['aleatorio']} aleatórios, "
        f"{contagem['valido']} bem tipados (devem ser aceitos), "
        f"{contagem['mutacao']} com um erro injetado"
    )
    print("Erros injetados: " + ", ".join(f"{k}×{v}" for k, v in sorted(codigos_mutados.items())))
    alvo = "Python × C" if comando_c else "Python (sem C)"
    print(f"Resultado ({alvo}): {args.n - falhas}/{args.n} programas sem falha")
    return 0 if falhas == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
