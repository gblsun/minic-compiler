"""Teste diferencial da IR e do otimizador em programas MINIC válidos gerados.

    python tests/fuzz_ir.py                         # só Python, 300 programas, semente 2026
    python tests/fuzz_ir.py --c ./src/c/minic       # Python × C
    python tests/fuzz_ir.py --c ./src/c/minic -n 1000 --semente 7
    python tests/fuzz_ir.py --salvar /tmp/falhas    # guarda os programas que falharem

O `fuzz_semantico.py` gera programas quase sempre **inválidos** (é o que
exercita os diagnósticos). Aqui é o contrário: o gerador só produz programas
bem tipados e que terminam — funções sem recursão, laços com contador que o
corpo não altera, divisões só por constante diferente de zero —, usando de
propósito o que costuma quebrar um otimizador: globais alteradas dentro de
funções, `&&`/`||` com chamada à direita, `break`/`continue`, sombreamento,
variáveis com nome de temporário (`t1`), conversões char/int/float, vetores
passados por parâmetro, constantes que transbordam 32 bits.

Para **cada** programa, exige:

1. a análise semântica aceita (senão é defeito do gerador, e conta como falha);
2. Python e C imprimem exatamente a mesma IR, em -O0 e em -O1;
3. a IR -O1, executada no interpretador (`tests/ir_interp.py`), imprime o
   mesmo que a IR -O0 — o otimizador preservou o comportamento;
4. o programa compilado e executado na máquina virtual (`--run`, em -O0 e
   -O1, em Python e em C) imprime o mesmo que o interpretador — o back-end
   preservou o comportamento (desligável com `--sem-vm`).

A geração é determinística (semente fixa), então uma falha é reproduzível.
"""

import argparse
import random
import subprocess
import sys
import tempfile
from pathlib import Path

import ir_interp

RAIZ = Path(__file__).resolve().parent.parent
MINIC_PY = RAIZ / "minic.py"

TIPOS = ["int", "float", "bool", "char"]
NOMES = ["a", "b", "x", "y", "n", "soma", "t1", "t2", "k"]


class Gerador:
    def __init__(self, rng: random.Random):
        self.r = rng
        self.escopos: list[dict] = []
        self.globais: dict = {}
        self.funcoes: list = []  # (nome, retorno, [(tipo, eh_vetor)])
        self.contadores = 0
        self.profundidade = 0
        self.lacos = 0
        self.retorno = "int"

    # -- nomes ------------------------------------------------------------

    def visiveis(self, tipo: str, vetor: bool = False) -> list[str]:
        vistos, nomes = set(), []
        for escopo in list(reversed(self.escopos)) + [self.globais]:
            for nome, (t, v) in escopo.items():
                if nome not in vistos:
                    vistos.add(nome)
                    if t == tipo and v == vetor:
                        nomes.append(nome)
        return nomes

    def novo_nome(self) -> str:
        atual = self.escopos[-1] if self.escopos else self.globais
        livres = [n for n in NOMES if n not in atual]
        return self.r.choice(livres) if livres else f"v{len(atual)}"

    def contador(self) -> str:
        self.contadores += 1
        return f"i{self.contadores}"

    # -- expressões -------------------------------------------------------

    def literal(self, tipo: str) -> str:
        if tipo == "int":
            return str(self.r.choice([0, 1, 2, 3, 5, 7, 10, 42, 2147483647]))
        if tipo == "float":
            return self.r.choice(["0.5", "1.5", "2.0", "3.25"])
        if tipo == "bool":
            return self.r.choice(["true", "false"])
        return self.r.choice(["'a'", "'z'", "'0'", "'\\n'", "' '"])

    def expr(self, tipo: str, nivel: int = 0) -> str:
        r = self.r
        if nivel > 2 or r.random() < 0.25:
            nomes = self.visiveis(tipo)
            if nomes and r.random() < 0.7:
                return r.choice(nomes)
            return self.literal(tipo)
        n = nivel + 1
        if tipo == "int":
            escolha = r.randrange(9)
            if escolha == 0:
                return f"({self.expr('int', n)} {r.choice('+-*')} {self.expr('int', n)})"
            if escolha == 1:
                return f"({self.expr('int', n)} {r.choice('/%')} {r.choice(['2', '3', '7', '-4'])})"
            if escolha == 2:
                return f"-{self.expr('int', n)}"
            if escolha == 3 and self.visiveis("char"):
                return f"({r.choice(self.visiveis('char'))} + {self.expr('int', n)})"
            if escolha == 4:
                return self.chamada("int", n) or self.expr("int", n)
            if escolha == 5 and self.visiveis("int", True):
                vetor = r.choice(self.visiveis("int", True))
                return f"{vetor}[{r.randrange(3)}]"
            if escolha == 6 and self.visiveis("int"):
                return f"({r.choice(self.visiveis('int'))} = {self.expr('int', n)})"
            return f"({self.expr('int', n)} + {self.expr('int', n)})"
        if tipo == "float":
            escolha = r.randrange(5)
            if escolha == 0:
                return f"({self.expr('float', n)} {r.choice('+-*')} {self.expr('int', n)})"
            if escolha == 1:
                return f"({self.expr('float', n)} / 2.0)"
            if escolha == 2:
                return self.expr("int", n)  # int -> float implícito
            if escolha == 3:
                return self.chamada("float", n) or self.literal("float")
            return f"-{self.expr('float', n)}"
        if tipo == "bool":
            escolha = r.randrange(7)
            if escolha == 0:
                return f"({self.expr('int', n)} {r.choice(['<', '<=', '>', '>=', '==', '!='])} {self.expr('int', n)})"
            if escolha == 1:
                return f"({self.expr('float', n)} < {self.expr('int', n)})"
            if escolha == 2:
                return f"({self.expr('bool', n)} {r.choice(['&&', '||'])} {self.expr('bool', n)})"
            if escolha == 3:
                return f"!{self.expr('bool', n)}"
            if escolha == 4:
                return f"({self.expr('bool', n)} {r.choice(['==', '!='])} {self.expr('bool', n)})"
            if escolha == 5:
                chamada = self.chamada("bool", n)
                if chamada:
                    return f"({self.expr('bool', n)} {r.choice(['&&', '||'])} {chamada})"
            return f"({self.expr('char', n)} < {self.literal('char')})"
        return self.literal("char") if r.random() < 0.5 else self.expr("char", 3)

    def chamada(self, tipo: str, nivel: int):
        candidatas = [f for f in self.funcoes if f[1] == tipo and self.pode_chamar(f[2])]
        if not candidatas:
            return None
        nome, _, params = self.r.choice(candidatas)
        args = []
        for t, vetor in params:
            if vetor:
                args.append(self.r.choice(self.visiveis(t, True)))
            else:
                args.append(self.expr(t, nivel + 1))
        return f"{nome}({', '.join(args)})"

    def pode_chamar(self, params) -> bool:
        return all(not v or self.visiveis(t, True) for t, v in params)

    # -- comandos ---------------------------------------------------------

    def bloco(self, indent: str, n: int) -> list[str]:
        linhas = []
        for _ in range(n):
            linhas += self.comando(indent)
        return linhas

    def comando(self, indent: str) -> list[str]:
        r = self.r
        escolha = r.randrange(12)
        if self.profundidade >= 3:
            escolha = r.choice([0, 1, 2])
        if escolha == 0:
            tipo = r.choice(TIPOS)
            nomes = self.visiveis(tipo)
            if nomes:
                return [f"{indent}{r.choice(nomes)} = {self.expr(tipo)};"]
        if escolha == 1 and self.visiveis("int", True):
            vetor = r.choice(self.visiveis("int", True))
            return [f"{indent}{vetor}[{r.randrange(3)}] = {self.expr('int')};"]
        if escolha in (0, 1, 2, 3):
            return [f"{indent}print({self.expr(r.choice(TIPOS))});"]
        if escolha == 4:
            void = [f for f in self.funcoes if f[1] == "void" and self.pode_chamar(f[2])]
            if void:
                nome, _, params = r.choice(void)
                args = [
                    r.choice(self.visiveis(t, True)) if v else self.expr(t) for t, v in params
                ]
                return [f"{indent}{nome}({', '.join(args)});"]
            return [f"{indent}print({self.expr('int')});"]
        if escolha == 5 and self.lacos:
            tipo = r.choice(["break", "continue"])
            return [f"{indent}if ({self.expr('bool')}) {{", f"{indent}    {tipo};", f"{indent}}}"]
        self.profundidade += 1
        try:
            if escolha in (5, 6):
                linhas = [f"{indent}if ({self.expr('bool')}) {{"]
                linhas += self.sub_bloco(indent + "    ")
                if r.random() < 0.5:
                    linhas += [f"{indent}}} else {{"] + self.sub_bloco(indent + "    ")
                return linhas + [f"{indent}}}"]
            if escolha == 7:
                i = self.contador()
                self.escopos[-1][i] = ("int", False)
                limite = r.randrange(1, 5)
                linhas = [f"{indent}int {i};", f"{indent}for ({i} = 0; {i} < {limite}; {i} = {i} + 1) {{"]
                self.lacos += 1
                linhas += self.sub_bloco(indent + "    ", protegido=i)
                self.lacos -= 1
                return linhas + [f"{indent}}}"]
            if escolha == 8:
                i = self.contador()
                self.escopos[-1][i] = ("int", False)
                limite = r.randrange(1, 5)
                linhas = [f"{indent}int {i} = 0;", f"{indent}while ({i} < {limite}) {{"]
                linhas.append(f"{indent}    {i} = {i} + 1;")  # antes de qualquer continue
                self.lacos += 1
                linhas += self.sub_bloco(indent + "    ", protegido=i)
                self.lacos -= 1
                return linhas + [f"{indent}}}"]
            # bloco com declaração (sombreamento)
            return [f"{indent}{{"] + self.sub_bloco(indent + "    ") + [f"{indent}}}"]
        finally:
            self.profundidade -= 1

    def sub_bloco(self, indent: str, protegido=None, novo_escopo=True) -> list[str]:
        """Declarações e comandos; `novo_escopo=False` no corpo da função, que
        divide o escopo com os parâmetros e com o que vem depois."""
        if novo_escopo:
            self.escopos.append({})
        if protegido:
            # O contador do laço não pode ser escrito no corpo: some da vista.
            self.escopos[-1][protegido] = ("protegido", False)
        linhas = []
        for _ in range(self.r.randrange(0, 3)):
            tipo = self.r.choice(TIPOS)
            nome = self.novo_nome()
            # O nome já vale dentro do próprio inicializador (como em C): até
            # lá, fica fora de alcance para o gerador.
            self.escopos[-1][nome] = ("declarando", False)
            linhas.append(f"{indent}{tipo} {nome} = {self.expr(tipo)};")
            self.escopos[-1][nome] = (tipo, False)
        linhas += self.bloco(indent, self.r.randrange(1, 4))
        if novo_escopo:
            self.escopos.pop()
        return linhas

    # -- programa ---------------------------------------------------------

    def funcao(self, indice: int) -> list[str]:
        r = self.r
        retorno = r.choice(TIPOS + ["void"])
        params = []
        for _ in range(r.randrange(0, 3)):
            if r.random() < 0.2:
                params.append(("int", True))
            else:
                params.append((r.choice(TIPOS), False))
        nome = f"f{indice}"
        nomes_params = [f"p{j}" for j in range(len(params))]
        self.escopos = [{}]
        for p, (t, v) in zip(nomes_params, params):
            self.escopos[0][p] = (t, v)
        self.retorno = retorno
        assinatura = ", ".join(
            f"{t} {p}[]" if v else f"{t} {p}" for p, (t, v) in zip(nomes_params, params)
        )
        linhas = [f"{retorno} {nome}({assinatura}) {{"]
        linhas += self.sub_bloco("    ", novo_escopo=False)
        # Altera uma global int (se nenhum local a sombreia com outro tipo).
        alteraveis = [n for n in self.visiveis("int") if n in self.globais]
        if alteraveis and r.random() < 0.5:
            g = r.choice(alteraveis)
            linhas.append(f"    {g} = {g} + 1;")
        if retorno != "void":
            linhas.append(f"    return {self.expr(retorno)};")
        linhas.append("}")
        self.funcoes.append((nome, retorno, params))
        return linhas

    def programa(self) -> str:
        r = self.r
        linhas = []
        for _ in range(r.randrange(0, 3)):
            tipo = r.choice(TIPOS)
            nome = self.novo_nome()
            linhas.append(f"{tipo} {nome} = {self.literal(tipo)};")
            self.globais[nome] = (tipo, False)
        if r.random() < 0.5:
            linhas.append("int gv[3];")
            self.globais["gv"] = ("int", True)
        for indice in range(r.randrange(0, 4)):
            linhas += self.funcao(indice)
        self.escopos = [{}]
        self.retorno = "int"
        linhas.append("int main() {")
        linhas.append("    int vet[3];")
        self.escopos[0]["vet"] = ("int", True)
        linhas += self.sub_bloco("    ", novo_escopo=False)
        linhas += self.bloco("    ", r.randrange(2, 6))
        linhas += ["    return 0;", "}"]
        return "\n".join(linhas) + "\n"


def rodar(comando: list[str], opcoes: list[str], fonte: Path):
    p = subprocess.run(comando + opcoes + [str(fonte)], capture_output=True, timeout=60)
    return p.returncode, p.stdout, p.stderr


def executar(texto: str):
    try:
        return ir_interp.executar(texto), None
    except ir_interp.ErroExecucao as exc:
        return None, str(exc)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--c", help="executável C já compilado (src/c/minic)")
    parser.add_argument("-n", type=int, default=300, help="quantidade de programas")
    parser.add_argument("--semente", type=int, default=2026)
    parser.add_argument("--salvar", help="pasta onde guardar os programas que falharem")
    parser.add_argument("--sem-vm", action="store_true", help="não executa na máquina virtual")
    args = parser.parse_args(argv)
    sys.stdout.reconfigure(encoding="utf-8")

    comando_py = [sys.executable, str(MINIC_PY)]
    comando_c = [args.c] if args.c else None
    falhas = 0
    otimizados = 0
    with tempfile.TemporaryDirectory() as pasta:
        fonte = Path(pasta) / "programa.c"
        for i in range(args.n):
            texto = Gerador(random.Random(args.semente * 100003 + i)).programa()
            fonte.write_text(texto, encoding="utf-8")
            problemas = []
            codigo, out, err = rodar(comando_py, [], fonte)
            if codigo != 0:
                problemas.append("gerador produziu programa inválido: " + out.decode("utf-8")[:300])
            else:
                saidas = {}
                for nivel in ("-O0", "-O1"):
                    saidas[nivel] = rodar(comando_py, ["--ir", nivel], fonte)
                    if saidas[nivel][0] != 0 or saidas[nivel][2]:
                        problemas.append(f"{nivel}: código {saidas[nivel][0]}: {saidas[nivel][2][:300]}")
                    if comando_c and rodar(comando_c, ["--ir", nivel], fonte) != saidas[nivel]:
                        problemas.append(f"{nivel}: C e Python diferem")
                if not problemas:
                    ir0 = saidas["-O0"][1].decode("utf-8")
                    ir1 = saidas["-O1"][1].decode("utf-8")
                    exec0, exec1 = executar(ir0), executar(ir1)
                    if exec0 != exec1:
                        problemas.append(f"-O1 executa {exec1}, -O0 executa {exec0}")
                    elif ir0 != ir1:
                        otimizados += 1
                    # O back-end: a máquina virtual imprime o mesmo que o
                    # interpretador, nos dois níveis e nas duas linguagens.
                    if exec0[1] is None and not args.sem_vm:
                        esperado = "".join(linha + "\n" for linha in exec0[0]).encode("utf-8")
                        for nivel in ("-O0", "-O1"):
                            vm_py = rodar(comando_py, ["--run", nivel], fonte)
                            if vm_py != (0, esperado, b""):
                                problemas.append(f"--run {nivel}: código {vm_py[0]}, "
                                                 f"{(vm_py[1] + vm_py[2])[:200]!r}")
                            if comando_c and rodar(comando_c, ["--run", nivel], fonte) != vm_py:
                                problemas.append(f"--run {nivel}: C e Python diferem")
            if problemas:
                falhas += 1
                print(f"FALHA programa {i}: " + "; ".join(problemas))
                if args.salvar:
                    destino = Path(args.salvar)
                    destino.mkdir(parents=True, exist_ok=True)
                    (destino / f"programa_{i}.c").write_text(texto, encoding="utf-8")
    titulo = "C × Python e -O0 × -O1" if comando_c else "-O0 × -O1"
    print(
        f"{args.n - falhas}/{args.n} programas aprovados ({titulo}); "
        f"{otimizados} mudaram com -O1"
    )
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
