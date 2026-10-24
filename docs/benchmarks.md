# Benchmarks: -O0 × -O1

> Gerado por [`benchmarks/rodar.py --escrever`](../benchmarks/rodar.py), a
> partir de [`benchmarks/relatorio.modelo.md`](../benchmarks/relatorio.modelo.md).
> Não edite este arquivo à mão: mude o modelo ou os programas e rode o script.

Entregável da etapa 4 (Seção 15 da especificação): o efeito do otimizador
(`-O1`, [ir.md](ir.md#otimizador--o1)) em programas que fazem trabalho de
verdade, executados na máquina virtual do back-end ([backend.md](backend.md)).

## Método

- **Programas**: os 8 de [`benchmarks/`](../benchmarks/). Cada um imprime um
  resultado, e o script falha se a saída mudar entre `-O0` e `-O1`: um ganho
  que muda o resultado é um defeito, não uma otimização.
- **Métrica principal: instruções executadas** pela máquina virtual
  (`minic --run --stats`). É determinística: a mesma em qualquer máquina e em
  qualquer execução, e a mesma nas versões C e Python (elas executam o mesmo
  código).
- **Tamanho do código**: instruções da máquina de pilha geradas
  (`minic --target=asm`), sem contar rótulos e diretivas.
- **Tempo**: versão em C compilada com `gcc -O2`, melhor de 5 execuções,
  **incluindo** iniciar o processo e compilar o programa MINIC. Nesses tamanhos,
  quase todo o tempo é esse custo fixo; o número é ilustrativo.
- **Ambiente** desta medição: Windows AMD64, Python 3.14.7.

## Resultados

| Programa | Saída | Instruções executadas -O0 | -O1 | Redução | Tamanho do código (instruções) | Tempo (ms, C) |
| -------- | ----- | ------------------------: | --: | ------: | -----------------------------: | ------------: |
| [`collatz.c`](../benchmarks/collatz.c) | 2919 216 | 6.766.126 | 6.760.128 | **0.1%** | 84 → 82 | 43 → 43 |
| [`constantes.c`](../benchmarks/constantes.c) | 20000 | 27.038 | 27.014 | **0.1%** | 59 → 35 | 6 → 6 |
| [`crivo_primos.c`](../benchmarks/crivo_primos.c) | 669 | 303.772 | 251.426 | **17.2%** | 68 → 58 | 8 → 8 |
| [`fibonacci_recursivo.c`](../benchmarks/fibonacci_recursivo.c) | 6765 | 372.145 | 372.145 | **0.0%** | 35 → 35 | 14 → 14 |
| [`invariante_em_laco.c`](../benchmarks/invariante_em_laco.c) | 2447995 | 70.020 | 54.014 | **22.9%** | 49 → 35 | 8 → 6 |
| [`matriz_produto.c`](../benchmarks/matriz_produto.c) | 946200 | 462.752 | 356.518 | **23.0%** | 218 → 194 | 9 → 11 |
| [`mdc_euclides.c`](../benchmarks/mdc_euclides.c) | 10160 | 331.460 | 305.744 | **7.8%** | 65 → 63 | 9 → 9 |
| [`ordenacao_bolha.c`](../benchmarks/ordenacao_bolha.c) | 16 65424 | 1.290.929 | 748.953 | **42.0%** | 168 → 112 | 15 → 12 |
| **total** | | **9.624.242** | **8.875.942** | **7.8%** | | |

## Leitura

- **`ordenacao_bolha.c`: o maior ganho.** O laço interno calcula `j + 1` três
  vezes e faz `índice * 1` em todo acesso ao vetor. A eliminação de
  subexpressões comuns calcula `j + 1` uma vez só, e a simplificação algébrica
  apaga os `* 1`.
- **`matriz_produto.c` e `invariante_em_laco.c`: otimizações de laço** (aula
  19). Em `invariante_em_laco.c`, `largura * altura * escala` não muda dentro
  do laço e passa a ser calculado uma vez, antes dele. Em `matriz_produto.c`,
  os endereços `i * 20 + k` e `k * 20 + j` são de variáveis de indução: a
  redução de força troca a multiplicação por uma soma a cada volta, e o código
  invariante sai dos laços internos.
- **`crivo_primos.c` e `mdc_euclides.c`: ganho médio.** O que sai são cópias
  e temporários (propagação de cópias e código morto) e os `* 1` dos
  endereços. A estrutura dos laços já é enxuta.
- **`constantes.c`: a propagação apaga as contas, mas o laço fica.** `área` e
  `perímetro` são calculados em tempo de compilação (o código encolhe de 59
  para 35 instruções), mas
  `acumulado + 20 + 28 - 28` continua com três operações por volta: o
  otimizador não reassocia expressões (`a + b - b` → `a`), porque isso exige
  provar que não há transbordamento intermediário diferente.
- **`collatz.c` e `fibonacci_recursivo.c`: quase nada a ganhar.** O custo está
  em desvios dependentes de dados (Collatz) e em chamadas recursivas
  (Fibonacci), que o `-O1` preserva: nenhum passe remove ou junta chamadas, já
  que elas podem ter efeito.

## O que ainda limitaria o ganho

- A IR é traduzida instrução a instrução para a pilha. Cada temporário vira
  um `STORE` seguido de um `LOAD`, que um otimizador de janela (*peephole*)
  ou uma alocação de registradores eliminaria.
- Não há *inlining* nem eliminação de recursão de cauda.
- A eliminação de subexpressões comuns é local ao bloco básico; uma versão
  global (*available expressions*) pegaria repetições entre blocos.
