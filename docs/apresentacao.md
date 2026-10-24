---
marp: true
title: Compilador MINIC
paginate: true
---

<!--
Apresentação da etapa 4 (Seção 15 da especificação). Está no formato Marp:
abra no VS Code com a extensão "Marp for VS Code" para ver os slides e exportar
para PDF/PPTX, ou leia como Markdown comum — cada "---" separa um slide.
Os números vêm de docs/benchmarks.md e das suítes de teste (README).
-->

# Compilador MINIC

Um compilador completo, escrito duas vezes: **Python** e **C**

Compiladores — Ciência da Computação, Impacta, 2026
Prof. Alex Torquato Souza Carneiro

Fellipe Augusto Silva Pereira · Gabriel Muchon Pavanelli
Paloma Eduarda Soares Leite · Victor Wenzel Martins Gonçalves

---

## O que foi construído

```text
código ─► scanner ─► parser ─► semântica ─► IR (3AC) ─► otimizador ─► gerador ─► máquina virtual
          tokens     AST       tipos         --ir        -O1          --target=asm   --run
```

- As 4 etapas da especificação, do léxico à execução
- **Duas implementações** (Python e C) com saída **idêntica byte a byte**
- Tudo à mão: sem `re`, `flex`, `bison` ou LLVM
- Seção 11 completa: `--tokens`, `--ast`, `--symbols`, `--ir`, `--target=asm`, `--run`, `-O0/-O1`

---

## Por que duas implementações?

- A atividade pede as duas, e equivalentes
- Cada fase é escrita em Python (a referência) e transcrita para C
- Os testes comparam as duas **em tudo**:
  - tokens e AST;
  - mensagens de erro;
  - IR e código gerado;
  - saída do programa e número de instruções executadas
- Uma diferença entre as duas quase sempre aponta um defeito real em uma delas

---

## Etapas 1 e 2: léxico e sintaxe

- **Scanner** caractere a caractere, com *maximal munch*, linha e coluna em todo
  token e vários erros por execução
- **Parser** de descida recursiva, uma função por não terminal
  - a recursão à esquerda da gramática vira laço
  - o `else` se liga ao `if` mais próximo
  - recuperação em modo pânico: relata vários erros sintáticos de uma vez
- **AST** na notação dos testes oficiais

**AP 1:** 50/50 casos oficiais; 201/201 verificações

---

## Etapa 3: análise semântica

- Duas passagens: assinaturas e globais primeiro, verificação depois
- Escopos com sombreamento, tipos e conversões (`int → float`, `char → int`)
- Chamadas (aridade e tipos), retorno em **todos os caminhos**
- Contexto de `break`/`continue`, destinos de atribuição
- 16 diagnósticos (SEM001–SEM016), **sem cascata**: o tipo `ERRO` absorve os derivados

**AP 2:** 20/20 nos scripts oficiais, em Python e em C

---

## Representação intermediária

Código de três endereços no formato do pacote de testes da IR:

```text
int fatorial(int n) {          func fatorial(n)
    if (n <= 1) {                t1 = n <= 1
        return 1;                ifFalse t1 goto L_end_if
    }                            return 1
    return n * fatorial(n - 1);  L_end_if:
}                                t2 = n - 1
                                 param t2
                                 t3 = call fatorial, 1
                                 t4 = n * t3
                                 return t4
                               endfunc
```

Conversões implícitas viram instruções explícitas (`itof`, `ctoi`)

**Testes da IR:** 30/30 casos oficiais nas duas versões

---

## Otimizador `-O1`: seis passes até o ponto fixo

| Passe | Escopo |
| ----- | ------ |
| Dobra de constantes e identidades | local |
| Propagação de constantes e cópias | **global** — fluxo de dados *forward* |
| Subexpressões comuns | local |
| Código inalcançável | global |
| Código morto | **global** — variáveis vivas, *backward* |
| Laços: código invariante e redução de força | **global** — dominadores |

Regra de ouro: **nada com efeito observável sai ou muda de lugar** (`print`, `read`,
chamadas, vetores, divisões por valor desconhecido)

---

## Otimizações de laço (aula 19)

```text
while (i < 5) {             i = 0
    dobro = i * 2;          t1 = 0            ← s = i * 2, antes do laço
    print(dobro);         L_test:
    i = i + 1;              ...
}                           print t1
                            i = i + 1
                            t1 = t1 + 2       ← a multiplicação virou soma
```

- Dominadores → arestas de retorno → laços naturais → pré-cabeçalho
- Código invariante sai do laço, **exceto** divisões por valor desconhecido: no
  laço que roda zero vezes, a divisão movida poderia falhar
- Redução de força em variáveis de indução `i = i ± c`

---

## Back-end: máquina virtual de pilha (MVM)

- Alvo escolhido (Seção 10): **VM própria de pilha**, o modelo da aula 17
- **Registro de ativação**: parâmetros, locais e temporários, cada um num deslocamento do quadro (`--symbols`)
- **Convenção de chamada**: argumentos na pilha, `CALL f n`, retorno no topo
- Vetores locais no quadro; globais numa área estática
- Erros de execução com código 6:
  - divisão por zero;
  - índice fora do vetor;
  - recursão sem fim;
  - entrada inválida

```text
LOAD 0  ; n      PUSH 1      LE      JZ L_end_if      CALL fatorial 1
```

---

## Benchmarks: instruções executadas na VM

| Programa | -O0 | -O1 | Redução |
| -------- | --: | --: | ------: |
| ordenação (bolha, 200 itens) | 1.290.929 | 748.953 | **42,0%** |
| produto de matrizes 20×20 | 462.752 | 356.518 | **23,0%** |
| invariante em laço | 70.020 | 54.014 | **22,9%** |
| crivo de primos até 5000 | 303.772 | 251.426 | **17,2%** |
| Fibonacci recursivo | 372.145 | 372.145 | 0% |

A saída de cada programa é conferida: **idêntica** em `-O0` e `-O1`

---

## Como sabemos que está certo

- **Casos oficiais** da disciplina: AP 1, AP 2 e o pacote de IR
- **Golden files** próprios para cada fase, inclusive programas executados
- **Interpretador da IR** como oráculo: a VM tem de imprimir o mesmo
- **Fuzzing**: milhares de programas válidos gerados aleatoriamente
  - Python × C;
  - `-O0` × `-O1`;
  - VM × interpretador
- **CI** a cada push, com gcc e clang sem avisos e o C com **AddressSanitizer + UBSan**

Os testes acharam defeitos reais durante o desenvolvimento. Exemplo: com `-O1`,
o tipo de um `read` num vetor se perdia na renumeração dos temporários.
Todos foram corrigidos antes da entrega

---

## Limitações e próximos passos

- Tradução direta IR → pilha: cada temporário vira `STORE` + `LOAD`.
  Um *peephole* ou uma alocação de registradores reduziria isso
- Subexpressões comuns só dentro do bloco básico
- Sem *inlining*, sem reassociação, sem eliminação de recursão de cauda
- A VM é interpretada; um alvo nativo (x86-64 ou LLVM) seria o próximo passo

---

## Demonstração

```bash
python3 minic.py --tokens   exemplo.c
python3 minic.py --ast      exemplo.c
python3 minic.py --symbols  exemplo.c
python3 minic.py --ir -O1   exemplo.c
python3 minic.py --target=asm exemplo.c
./minic --run -O1 --stats   exemplo.c
```

Documentação: `README.md` · `docs/manual.md` · `docs/backend.md` · `docs/benchmarks.md`

**Obrigado!**
