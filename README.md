# minic-compiler

[![testes](https://github.com/gblsun/minic-compiler/actions/workflows/testes.yml/badge.svg)](https://github.com/gblsun/minic-compiler/actions/workflows/testes.yml)

Compilador da linguagem **MINIC**, um subconjunto didático de C, feito na disciplina de Compiladores da Impacta. É implementado **duas vezes, em Python e em C**, e as duas versões produzem a mesma saída byte a byte para a mesma entrada. Testes automáticos verificam isso a cada `push`.

```text
código-fonte ─► scanner ─► parser ─► semântica ─► IR (3AC) ─► otimizador ─► gerador de código ─► máquina virtual
                tokens     AST       tipos         --ir        -O1           --target=asm          --run
               (etapa 1)  (etapa 2)  (etapa 3)    (etapa 3)   (etapa 4)     (etapa 4)             (etapa 4)
```

Tudo é escrito à mão, sem `re`, `flex`, `yacc`, `bison`, ANTLR ou LLVM:

- o scanner reconhece caractere a caractere;
- o parser é de descida recursiva;
- o otimizador faz análise de fluxo de dados sobre blocos básicos, inclusive otimização de laços;
- o back-end gera código para uma máquina virtual de pilha própria, que o compilador também executa.

## Sumário

- [Início rápido](#início-rápido)
- [Status das entregas](#status-das-entregas)
- [Como avaliar cada atividade](#como-avaliar-cada-atividade)
- [Usando o compilador](#usando-o-compilador)
- [As fases em detalhe](#as-fases-em-detalhe)
- [Testes](#testes)
- [Estrutura do repositório](#estrutura-do-repositório)
- [Limitações e próximos passos](#limitações-e-próximos-passos)
- [Mapa da documentação](#mapa-da-documentação)
- [Equipe](#equipe)

---

## Início rápido

Requisitos: **Python 3.10+** e um **compilador C11** (`gcc` ou `clang`). Nenhuma biblioteca externa.

```bash
git clone https://github.com/gblsun/minic-compiler.git
cd minic-compiler

python3 minic.py programa.c              # análise semântica
python3 minic.py --ir -O1 programa.c     # IR otimizada
python3 minic.py --run programa.c        # compila e executa

gcc -Wall -Wextra minic.c -o minic       # a mesma coisa em C (compila sem avisos)
./minic --run -O1 programa.c
```

Um programa MINIC, o exemplo da Seção 13.1 da especificação:

```c
int soma(int a, int b) {
    return a + b;
}

int main() {
    int valores[3];
    int i;
    int total = 0;
    for (i = 0; i < 3; i = i + 1) {
        valores[i] = i * 2;
        total = total + valores[i];
    }
    print(soma(total, 1));
    return 0;
}
```

O que cada fase produz para ele:

<table>
<tr><th>Análise semântica (<code>minic.py programa.c</code>)</th><th>IR otimizada (<code>minic.py --ir -O1 programa.c</code>)</th></tr>
<tr><td valign="top">

```text
Análise semântica concluída: 0 erros; programa aceito.
```

Com erros, cada diagnóstico traz código, linha e coluna. Por exemplo, com `int n = 2.5;` na linha 2 e `print(total);` sem declarar `total` na linha 4:

```text
SEM003 — linha 2, coluna 13: Não é possível atribuir
  float a int sem conversão permitida (destino “n”;
  expressão “2.5”).
SEM001 — linha 4, coluna 11: Identificador “total”
  não declarado neste escopo.
Análise semântica concluída: 2 erros; programa rejeitado.
```

(Quebras de linha acrescentadas aqui para caber na tabela.)

</td><td valign="top">

```text
func soma(a, b)
  t1 = a + b
  return t1
endfunc
func main
  total = 0
  i = 0
  t1 = 0
L_test:
  t2 = i < 3
  ifFalse t2 goto L_end
  store valores[i], t1
  t3 = load valores[i]
  t4 = total + t3
  total = t4
  t5 = i + 1
  i = t5
  t1 = t1 + 2
  goto L_test
L_end:
  param total
  param 1
  t6 = call soma, 2
  print t6
  return 0
endfunc
```

O `i * 2` virou uma soma incremental (`t1 = t1 + 2`): é a redução de força, uma otimização de laço.

</td></tr>
</table>

E executando:

```text
$ python3 minic.py --run --stats programa.c
7
instruções executadas: 135
$ python3 minic.py --run -O1 --stats programa.c
7
instruções executadas: 113
```

O mesmo `minic` mostra cada fase: `--tokens`, `--ast`, `--symbols`, `--ir` e `--target=asm`. Ver [Usando o compilador](#usando-o-compilador) e o [manual](docs/manual.md).

## Status das entregas

| Etapa | Atividade                       | Componente                              | Resultado                                                                                             |
| :---: | ------------------------------- | --------------------------------------- | ----------------------------------------------------------------------------------------------------- |
|   1   | Analisador léxico               | scanner                                 | ✅ entregue                                                                                            |
|   2   | **AP 1** — analisador sintático | parser e AST                            | ✅ 50/50 casos oficiais, 201/201 verificações ([resultados](docs/resultados-etapa2.md))                |
|   3   | **AP 2** — analisador semântico | tabela de símbolos e semântica          | ✅ 20/20 nos scripts oficiais, em Python e em C; 184/184 verificações ([entrega](docs/entrega-ap2.md)) |
|  3–4  | **Testes para o MINIC (IR)**    | IR de três endereços e otimizador `-O1` | ✅ 30/30 casos oficiais em Python e em C; 235/235 verificações ([entrega](docs/entrega-ir.md))         |
|   4   | Código e otimização             | back-end, benchmarks, manual            | ✅ máquina virtual de pilha, otimizações de laço, benchmarks, manual e apresentação ([etapa 4](docs/etapas.md#etapa-4--geração-de-código-e-otimização-)) |

Prazos e comandos exigidos por cada atividade: [docs/etapas.md](docs/etapas.md).

---

## Como avaliar cada atividade

Cada atividade define os próprios comandos, e os pacotes de teste oficiais estão versionados em [`ref/testes-oficiais/`](ref/testes-oficiais/), sem edição. Os comandos abaixo rodam na raiz do repositório, a partir de um clone novo.

| Atividade | Comando principal | Resultado esperado |
| --------- | ----------------- | ------------------ |
| **AP 1** — parser | `python3 parser.py codigo.c` | 50/50 pela suíte do repositório; os scripts do pacote marcam 16 aprovados (é o teto, veja abaixo) |
| **AP 2** — semântica | `./testes_semanticos_py.sh minic.py [diretorio]` e `./testes_semanticos_c.sh minic.c [diretorio]` | **20/20** nas duas versões |
| **Testes para o MINIC (IR)** | `python3 minic.py --ir [-O1] arquivo.c` | **30/30** pela suíte do repositório; os scripts do pacote dão 0/30 para qualquer compilador (veja abaixo) |
| **Etapa 4** — código e otimização | `python3 minic.py --run [-O1] arquivo.c` | programa executado na máquina virtual; 250/250 no back-end |

<details>
<summary><strong>AP 1 — analisador sintático</strong> (<code>parser.py</code> / <code>parser.c</code>)</summary>

<br>

Comandos do enunciado:

```bash
python3 parser.py codigo.c
gcc -Wall -Wextra -std=c11 parser.c -o parser && ./parser codigo.c
```

Com o pacote oficial de 50 casos:

```bash
bash ref/scripts/testar_parser_python.sh ref/testes-oficiais/testes-parser-50 ./parser.py
bash ref/scripts/testar_parser_c.sh      ref/testes-oficiais/testes-parser-50 ./parser.c
```

**O que esperar.** Os scripts imprimem `Aprovados: 16`, `Reprovados: 34` e `Erros sintáticos: 25`. É o teto do pacote e **não indica falha do parser**:

- os **25 casos de erro sintático são todos detectados**, e nenhum caso termina com erro de execução;
- dos 34 reprovados, **25 são impossíveis por construção**. Nos casos 26 a 50, o gabarito diz "NÃO HÁ AST: o parser deve rejeitar a entrada.", frase que nenhum parser imprime;
- os outros **9 diferem só em espaço em branco**, porque o próprio pacote formata o mesmo construto de dois jeitos;
- o caso 24 tem um `)` faltando no gabarito.

A suíte do repositório compara do jeito certo: **50/50**. Explicação caso a caso em [docs/resultados-etapa2.md](docs/resultados-etapa2.md); correções posteriores à avaliação em [docs/correcoes-etapa2.md](docs/correcoes-etapa2.md).

</details>

<details>
<summary><strong>AP 2 — analisador semântico</strong> (<code>minic.py</code> / <code>minic.c</code>)</summary>

<br>

Comandos do enunciado, com os scripts e a pasta de testes da atividade copiados para a raiz:

```bash
chmod +x testes_semanticos_*.sh
./testes_semanticos_py.sh minic.py minic-testes-semanticos
./testes_semanticos_c.sh  minic.c  minic-testes-semanticos
```

Ou, sem copiar nada, com as cópias versionadas em `ref/`:

```bash
./ref/scripts/testes_semanticos_py.sh minic.py ref/testes-oficiais/testes-semanticos-20
./ref/scripts/testes_semanticos_c.sh  minic.c  ref/testes-oficiais/testes-semanticos-20
```

```text
[OK]     minic-testes-semanticos/01_promocao_numerica.c
...
[OK]     minic-testes-semanticos/20_destino_nao_atribuivel.c

Resultado: 20/20 aprovados; 0 reprovados.
```

**20/20 nas duas versões.** O script C compila o `minic.c` sozinho, sem nenhum aviso.

Os `.gabarito` separam as linhas com CRLF e não terminam com quebra de linha, e o script compara com `diff` exato. Um editor que converta CRLF em LF quebra os casos de rejeição. Por isso o `.gitattributes` marca esses arquivos como `-text`.

Cada critério de correção (desacoplamento, equivalência C × Python, lógica, casos de teste) com a evidência correspondente: [docs/entrega-ap2.md](docs/entrega-ap2.md).

</details>

<details>
<summary><strong>Testes para o MINIC (IR)</strong> (<code>minic.py --ir</code> / <code>minic.c --ir</code>)</summary>

<br>

Os comandos são os que o README do pacote sugere (`minic --ir arquivo.c` e `minic -O1 --ir arquivo.c`):

```bash
python3 minic.py --ir programa.c          # IR sem otimização (-O0)
python3 minic.py --ir -O1 programa.c      # IR otimizada

gcc -Wall -Wextra minic.c -o minic        # a mesma coisa em C
./minic --ir -O1 programa.c

python3 tests/run_ir_tests.py             # os 30 casos do pacote, em Python e em C
```

| Grupo do pacote | O que o gabarito pede | Python | C |
| --------------- | --------------------- | :----: | :-: |
| P01–P10, válidos | IR canônica `-O0` | 10/10 (8 idênticas, 2 equivalentes) | 10/10 |
| E01–E10, inválidos | rejeição com a categoria certa (código 2, 3 ou 4) | 10/10 | 10/10 |
| O01–O10, otimização | IR `-O1` equivalente, com menos instruções | 10/10 | 10/10 |

Em P06 e P10 a IR difere do gabarito só em pontos em que o próprio gabarito foge da tradução direta, e as duas imprimem o mesmo resultado ([detalhes](docs/entrega-ir.md)).

**Sobre os scripts `test_ir_*.sh` do pacote.** Eles chamam `minic arquivo.c` **sem `--ir`** e comparam o gabarito inteiro byte a byte, inclusive linhas em prosa como `Resultado: REJEITAR` e `Critério: …`. Nos casos de otimização, comparam também as duas IR (`-O0` e `-O1`) com uma única execução. Por isso dão **0/30 para qualquer compilador**, e é fácil conferir:

```bash
bash ref/scripts/test_ir_python.sh minic.py ref/testes-oficiais/testes-ir-30/testes
bash ref/scripts/test_ir_c.sh      minic.c  ref/testes-oficiais/testes-ir-30/testes
```

Com o filtro `IGNORE_RE` personalizado que o próprio script sugere, o teto é 17/30. O `minic` sem opções não pode imprimir a IR, porque os scripts da AP 2 também o chamam sem opções e esperam o relatório semântico.

O README do mesmo pacote diz que os gabaritos "não são uma exigência de igualdade byte a byte" e pede comparação de estrutura e semântica. É o que `run_ir_tests.py` faz: renumera temporários e rótulos e **executa** as duas IR num interpretador para comparar o que imprimem.

Página de entrega: [docs/entrega-ir.md](docs/entrega-ir.md). Análise completa: [docs/ir.md](docs/ir.md).

</details>

<details>
<summary><strong>Etapa 4 — geração de código e otimização</strong> (<code>--run</code>, <code>--target=asm</code>, <code>-O1</code>)</summary>

<br>

```bash
python3 minic.py --target=asm programa.c       # código para a máquina de pilha
python3 minic.py --run programa.c              # compila e executa
python3 minic.py --run -O1 --stats programa.c  # com otimização e contagem de instruções

python3 tests/run_backend_tests.py             # 14 programas executados + VM × interpretador + C × Python
python3 benchmarks/rodar.py                    # -O0 × -O1 em 8 programas
```

Resultado: **250/250 verificações** no back-end. Nos benchmarks, o `-O1` executa de 0% a 42% menos instruções (7,8% no total), com a mesma saída em todos os programas. Descrição em [docs/backend.md](docs/backend.md), [docs/benchmarks.md](docs/benchmarks.md), [docs/manual.md](docs/manual.md) e [docs/apresentacao.md](docs/apresentacao.md).

</details>

---

## Usando o compilador

### Pontos de entrada

O `minic` implementa a interface da Seção 11 da especificação:

```text
minic [--check|--tokens|--ast|--symbols|--ir|--target=asm|--run] [-O0|-O1] [--stats] arquivo.c
```

| Modo | O que faz |
| ---- | --------- |
| *(nenhum)* · `--check` | analisa sem gerar código e imprime o relatório da análise semântica (o formato da AP 2) |
| `--tokens` | os tokens, um por linha |
| `--ast` | a AST, numa linha |
| `--symbols` | a tabela de símbolos: categoria, tipo, nível, posição, inicialização e uso, e o deslocamento no registro de ativação |
| `--ir` | a IR de três endereços |
| `--target=asm` | o código gerado para a máquina de pilha |
| `--run` | gera o código e executa; o `read` lê da entrada padrão |
| `-O0` / `-O1` | sem otimização (padrão) / com otimização |
| `--stats` | com `--run`: quantas instruções a máquina executou (em stderr) |

`python3 minic.py …` e `./minic …` aceitam exatamente as mesmas opções. As atividades das etapas 1 e 2 têm também seus executáveis próprios: `python3 src/python/main.py arq.c` (scanner) e `python3 parser.py [--tree] arq.c` (parser).

Compilando as versões C (os arquivos da raiz incluem `src/c/` numa unidade de tradução só, que é como os scripts do professor compilam):

```bash
gcc -Wall -Wextra minic.c -o minic
gcc -Wall -Wextra -std=c11 parser.c -o parser
gcc -Wall -Wextra -std=c11 src/c/lexer.c src/c/main.c -o src/c/scanner
```

### Saídas e códigos de saída

Cada fase só roda se a anterior terminou sem erro. Os códigos seguem a Seção 11.1 da especificação.

| Código | Significado | Onde sai a mensagem |
| :----: | ----------- | ------------------- |
| `0` | sucesso | stdout: tokens, AST, relatório, símbolos, IR, código ou a saída do programa |
| `1` | uso inválido (argumento faltando, arquivo ilegível) | stderr |
| `2` | erro léxico — `Erro léxico na linha L, coluna C: …` | stderr |
| `3` | erro sintático — `Erro de sintaxe na linha L, coluna C: esperado X; encontrado Y.` | stderr |
| `4` | erro semântico — `SEM0NN — linha L, coluna C: …` | stdout (formato do gabarito da AP 2) |
| `5` | erro na geração de código — `Erro na geração de código: o programa não define a função main.` | stderr |
| `6` | erro de execução — `Erro de execução em f: divisão por zero.` (também: índice fora do vetor, recursão sem fim, entrada inválida) | stderr |

### A linguagem em resumo

| Área | O que a MINIC tem |
| ---- | ----------------- |
| Tipos | `int`, `float`, `bool`, `char`, `void` (só retorno), vetores unidimensionais `int v[10]` |
| Comandos | declaração, atribuição, bloco, `if`/`else`, `while`, `for`, `return`, `break`, `continue`, `print`, `read` |
| Expressões | aritméticas, relacionais, lógicas (`&&`, `\|\|`, `!`), chamadas, indexação |
| Conversões implícitas | só `int → float` e `char → int` |
| Escopo | léxico, com sombreamento em bloco interno; globais, parâmetros e locais |

Referência completa: [docs/especificacao.md](docs/especificacao.md) (léxico), [docs/gramatica.md](docs/gramatica.md) (sintaxe), [docs/semantica.md](docs/semantica.md) (regras) e o [PDF da especificação](ref/especificacao-completa-minic.pdf), que é a fonte normativa.

---

## As fases em detalhe

### 1. Analisador léxico

Reconhece os tokens ([docs/tokens.md](docs/tokens.md)) caractere a caractere, com *maximal munch*, e imprime `<TOKEN, lexema>` para identificadores e literais e só `TOKEN` para palavras reservadas e símbolos. Um erro não para a análise: o scanner reporta e continua, então vários erros aparecem numa execução.

<details>
<summary>Interface Streamlit (opcional)</summary>

<br>

Interface web para testar o scanner: escolha um exemplo, cole código ou envie um arquivo e veja os tokens numa tabela. Não faz parte da entrega obrigatória.

```bash
pip install -r requirements.txt
python -m streamlit run src/python/app_streamlit.py     # abre em http://localhost:8501
```

</details>

### 2. Analisador sintático e AST

Parser por **descida recursiva**, uma função por não terminal da [gramática](docs/gramatica.md), que constrói a [AST](docs/ast.md). A recursão à esquerda das expressões vira laço, e o `else` pendente se liga ao `if` mais próximo. Com recuperação em **modo pânico**, vários erros são reportados numa só execução.

```text
$ python3 parser.py ref/testes-oficiais/testes-parser-50/casos/09_if_com_else/codigo.c
Program(Function(int main() Block(VarDecl(int x=Lit(int,1)), If(Id(x),ExprStmt(Assign(Id(x),Lit(int,2))),ExprStmt(Assign(Id(x),Lit(int,3)))), Return(Id(x)))))
```

> **Por que "Erro de sintaxe" e não "Erro sintático"?** O script oficial conta os erros com `grep`. Em locale `C`, o "á" não casa, e o contador dava 0 mesmo com os 25 casos rejeitados. Com a mensagem em ASCII, o resultado é o mesmo em qualquer locale.

### 3. Analisador semântico

Duas passagens sobre a AST. A primeira coleta as assinaturas de funções e as globais; a segunda verifica:

- nomes e escopos, com sombreamento;
- tipos e conversões implícitas;
- vetores (índice inteiro, limites constantes);
- chamadas (aridade e tipo dos argumentos);
- retornos, inclusive retorno em todos os caminhos;
- condições booleanas e o contexto de `break` e `continue`;
- destinos de atribuição.

Um erro **não gera cascata**: o tipo sentinela `ERRO` suprime os diagnósticos derivados, e o resto do programa continua sendo verificado. São 16 diagnósticos, de SEM001 a SEM016, catalogados em [docs/semantica.md](docs/semantica.md).

Onde o pacote oficial da AP 2 e a especificação divergem, prevaleceu o pacote:

| Ponto | Especificação | O que foi feito | Por quê |
| ----- | ------------- | --------------- | ------- |
| função `main` | obrigatória (Seção 2) | não é exigida | os 10 programas válidos da AP 2 usam `principal` |
| `3 = n;` | erro de sintaxe | erro semântico SEM013 | caso oficial 20 |
| `a = b = 3` | atribuição "não produz valor" | tem o tipo do destino | caso oficial 09 |

### 4. Representação intermediária (IR)

A AST verificada é traduzida para **código de três endereços** (Seção 9), no formato dos gabaritos do pacote de IR:

| Instrução | Para que serve |
| --------- | -------------- |
| `x = a op b` | aritmética, relacional, `and`/`or` |
| `x = neg a` · `x = not a` | operadores unários |
| `x = itof a` · `x = ctoi a` | conversões implícitas, explícitas na IR |
| `x = load v[i]` · `store v[i], a` | acesso a vetor |
| `param a` · `x = call f, n` | chamada de função |
| `goto L` · `ifFalse a goto L` | desvios |
| `print a` · `read x` · `return a` | entrada, saída e retorno |
| `func f(a, b)` … `endfunc` | delimitam uma função |

Algumas decisões de tradução:

- variáveis sombreadas recebem sufixo (`x#1`, `x#2`);
- conversões implícitas viram instruções, para o otimizador não confundir divisão inteira com real;
- `&&` e `||` usam curto-circuito quando o lado direito tem uma chamada;
- as globais são inicializadas numa função `@init`.

Todas as decisões estão em [docs/ir.md](docs/ir.md#decisões-de-tradução).

### 5. Otimizador (`-O1`)

Seis passes, repetidos até nada mais mudar:

| Passe | Escopo | Exemplo |
| ----- | ------ | ------- |
| Dobra de constantes e identidades algébricas | local | `2 + 3` → `5`, `x * 1` → `x`, `3 < 5` → `true` |
| Propagação de constantes e cópias | **global**, fluxo de dados *forward* | `a = 6; b = a; print b` → `print 6` |
| Subexpressões comuns | local (bloco básico) | o segundo `a * b` reaproveita o primeiro |
| Limpeza do fluxo de controle | global | remove ramo de `if` constante, código após `return` |
| Código morto | **global**, variáveis vivas (*backward*) | remove atribuição que ninguém lê |
| Laços (aula 19) | **global**, dominadores e laços naturais | tira do laço o que não muda nele; troca `i * k` por uma soma a cada volta |

**Nada com efeito observável é removido nem reordenado.** Ficam onde estão:

- `print`, `read` e chamadas;
- acessos a vetor;
- divisões por valor desconhecido, que também nunca saem de um laço: num laço que roda zero vezes, a divisão movida poderia falhar.

Uma chamada invalida o que se sabia sobre as globais. A dobra reproduz os inteiros de 32 bits do C, com transbordamento e divisão truncada.

No caso oficial O07, o `if (3 < 5) { r = 11; } else { r = 22; } print(r);` vai de 7 instruções para 2: `print 11` e `return 0`. Em programas maiores, o `-O1` executa de 0% a 42% menos instruções ([benchmarks](docs/benchmarks.md)).

### 6. Back-end: máquina virtual de pilha

O alvo de geração de código (Seção 10) é uma **máquina virtual de pilha própria**, a MVM, que o próprio compilador executa com `--run`. É o modelo da aula 17:

- **registro de ativação** por chamada, com parâmetros, locais e temporários em deslocamentos fixos (os que o `--symbols` mostra);
- **convenção de chamada**: os argumentos vão pela pilha, `CALL f n` chama, e o valor de retorno fica no topo;
- **globais** numa área estática; **vetores locais** alocados no quadro e passados por referência.

```text
int fatorial(int n) {              .func fatorial 1
    if (n <= 1) {                  .slot 0 n int
        return 1;                    LOAD 0  ; n
    }                                PUSH 1
    return n * fatorial(n - 1);      LE
}                                    …
                                     CALL fatorial 1
                                     …
                                     RET
```

A execução confere o que a análise estática não alcança: divisão por zero, índice fora do vetor, recursão sem fim e entrada inválida terminam com código 6 e uma mensagem que diz em que função aconteceram. O conjunto de instruções, a semântica e os erros estão em [docs/backend.md](docs/backend.md).

---

## Testes

Cada fase tem uma suíte própria. Todas rodam as versões Python e C e conferem que elas são **idênticas byte a byte**.

| Suíte | Comando | O que cobre | Resultado |
| ----- | ------- | ----------- | :-------: |
| Scanner | `python3 tests/run_tests.py` | golden files de tokens e erros | 9/9 |
| Parser | `python3 tests/run_parser_tests.py` | 50 oficiais + 17 próprios + equivalência | 201/201 |
| Semântica | `python3 tests/run_semantic_tests.py` | 20 oficiais + 19 próprios + equivalência | 184/184 |
| IR e `-O1` | `python3 tests/run_ir_tests.py` | 30 oficiais + 13 próprios + equivalência | 235/235 |
| Back-end | `python3 tests/run_backend_tests.py` | 14 programas executados (golden) + VM × interpretador + C × Python em 8 modos | 250/250 |
| Aula 13 | `python3 exercicios/aula13/run_tests.py` | 10 exercícios em Python e em C | 250/250 |

Além dos casos fixos, há **testes diferenciais** com programas gerados aleatoriamente:

```bash
gcc -Wall -Wextra minic.c -o minic
python3 tests/fuzz_semantico.py --c ./minic -n 900   # semântica: Python × C, mais oráculos
python3 tests/fuzz_ir.py        --c ./minic -n 1000  # IR e execução: Python × C, -O0 × -O1, VM × interpretador
python3 benchmarks/rodar.py                          # -O0 × -O1: a saída de cada programa tem de ser a mesma
```

**Como a corretude é verificada.** O [`tests/ir_interp.py`](tests/ir_interp.py) é um interpretador da IR textual, independente do back-end. Ele serve de oráculo duas vezes:

- **otimizador**: cada programa é executado com a IR `-O0` e com a `-O1`, e as duas têm de imprimir exatamente o mesmo. A Seção 10.1 da especificação pede que a corretude seja demonstrada "com testes antes e depois da otimização";
- **back-end**: o programa compilado e executado na máquina virtual (`--run`) tem de imprimir o mesmo que o interpretador.

**Integração contínua.** O [workflow](.github/workflows/testes.yml) roda a cada push:

- todas as suítes, em Ubuntu 22.04 (gcc 11, Python 3.10) e 24.04 (gcc 13, Python 3.12);
- os scripts oficiais da disciplina;
- compilação sem avisos com gcc e clang;
- o C instrumentado com **AddressSanitizer e UBSan** sobre todas as entradas e sobre os programas gerados.

Detalhes de cada suíte, e como adicionar um caso: [tests/README.md](tests/README.md).

---

## Estrutura do repositório

```text
minic-compiler/
├── minic.py · minic.c      pontos de entrada do compilador (todos os modos da Seção 11)
├── parser.py · parser.c    pontos de entrada da AP 1
├── src/
│   ├── python/             implementação em Python
│   └── c/                  implementação em C, transcrita da Python
├── tests/                  suítes, golden files, interpretador da IR e fuzzers
├── benchmarks/             programas e script dos benchmarks -O0 × -O1
├── docs/                   documentação derivada da especificação
├── exercicios/aula13/      exercícios de tradução dirigida por sintaxe
└── ref/                    material original da disciplina (fonte normativa, não editar)
```

Os `minic.*` e `parser.*` da raiz são só "cascas": os scripts do professor chamam esses nomes e compilam **uma única unidade de tradução**. O código fica em `src/`, um arquivo por componente:

| Componente | Python | C |
| ---------- | ------ | - |
| Scanner | [`lexer.py`](src/python/lexer.py) | [`lexer.c`](src/c/lexer.c) / [`.h`](src/c/lexer.h) |
| AST | [`minic_ast.py`](src/python/minic_ast.py) | [`ast.c`](src/c/ast.c) / [`.h`](src/c/ast.h) |
| Parser | [`parser.py`](src/python/parser.py) | [`parser.c`](src/c/parser.c) / [`.h`](src/c/parser.h) |
| Tabela de símbolos | [`tabela_simbolos.py`](src/python/tabela_simbolos.py) | [`tabela_simbolos.c`](src/c/tabela_simbolos.c) / [`.h`](src/c/tabela_simbolos.h) |
| Analisador semântico | [`semantico.py`](src/python/semantico.py) | [`semantico.c`](src/c/semantico.c) / [`.h`](src/c/semantico.h) |
| IR | [`ir.py`](src/python/ir.py) | [`ir.c`](src/c/ir.c) / [`.h`](src/c/ir.h) |
| Otimizador | [`otimizador.py`](src/python/otimizador.py) | [`otimizador.c`](src/c/otimizador.c) / [`.h`](src/c/otimizador.h) |
| Gerador de código | [`geracao.py`](src/python/geracao.py) | [`geracao.c`](src/c/geracao.c) / [`.h`](src/c/geracao.h) |
| Máquina virtual | [`vm.py`](src/python/vm.py) | [`vm.c`](src/c/vm.c) / [`.h`](src/c/vm.h) |
| CLI do pipeline | [`minic.py`](src/python/minic.py) | [`minic_main.c`](src/c/minic_main.c) |

A versão Python é a referência e a C é transcrita dela, com a mesma ordem de visita, as mesmas mensagens e os mesmos nomes de temporários. Onde o C obrigou a mudar de mecânica, como `setjmp`/`longjmp` no lugar de exceções e strings internadas no lugar de `dict`, a explicação está em [src/c/README.md](src/c/README.md).

---

## Limitações e próximos passos

Todas as etapas da especificação estão implementadas. O que daria para melhorar:

- **Código gerado mais enxuto.** A IR é traduzida instrução a instrução para a pilha, e cada temporário vira um `STORE` seguido de um `LOAD`. Um otimizador de janela (*peephole*) ou uma alocação de registradores removeria isso.
- **Mais otimizações.** Subexpressões comuns entre blocos (*available expressions* global), reassociação, *inlining* e eliminação de recursão de cauda.
- **Alvo nativo.** A máquina virtual é interpretada; gerar x86-64 ou LLVM IR seria o passo seguinte.
- **Saída padrão do `minic`.** Sem opção, ele imprime o relatório da AP 2 em vez de compilar, porque os scripts daquela atividade o chamam assim e esperam esse relatório. Gerar a IR é `--ir`, e compilar e executar é `--run`.

Decisões que o grupo ainda precisa revisar e o checklist de entrega: [PENDENCIAS.md](PENDENCIAS.md).

---

## Mapa da documentação

| Quero… | Vá para |
| ------ | ------- |
| ver o status, os prazos e os comandos de cada etapa | [docs/etapas.md](docs/etapas.md) |
| entender o pipeline e os contratos entre as fases | [docs/arquitetura.md](docs/arquitetura.md) |
| entender a linguagem | [docs/especificacao.md](docs/especificacao.md), [docs/tokens.md](docs/tokens.md), [docs/gramatica.md](docs/gramatica.md) |
| ver a AST e sua notação | [docs/ast.md](docs/ast.md) |
| entender a análise semântica e as mensagens | [docs/semantica.md](docs/semantica.md), [docs/testes-semanticos.md](docs/testes-semanticos.md) |
| usar o compilador (manual) | [docs/manual.md](docs/manual.md) |
| entender a IR e as otimizações | [docs/ir.md](docs/ir.md) |
| entender o back-end e a máquina virtual | [docs/backend.md](docs/backend.md) |
| ver os benchmarks `-O0` × `-O1` | [docs/benchmarks.md](docs/benchmarks.md) |
| ver a apresentação | [docs/apresentacao.md](docs/apresentacao.md) |
| avaliar uma entrega | [AP 1](docs/resultados-etapa2.md) · [AP 2](docs/entrega-ap2.md) · [IR](docs/entrega-ir.md) · [etapa 4](docs/etapas.md#etapa-4--geração-de-código-e-otimização-) |
| saber como os pacotes oficiais são usados | [ref/testes-oficiais/README.md](ref/testes-oficiais/README.md) |
| ver os exercícios da aula 13 | [exercicios/aula13/README.md](exercicios/aula13/README.md) |
| ver decisões em aberto e o checklist | [PENDENCIAS.md](PENDENCIAS.md) |

O índice completo da documentação está em [docs/README.md](docs/README.md).

---

## Equipe

**Disciplina:** Compiladores, turma 202602 CC 6A Manhã (Ciência da Computação, 6º semestre, 2026)
**Professor:** Alex Torquato Souza Carneiro

| Nome | RA |
| ---- | -- |
| Fellipe Augusto Silva Pereira | 2401525 |
| Gabriel Muchon Pavanelli | 2401895 |
| Paloma Eduarda Soares Leite | 2401660 |
| Victor Wenzel Martins Gonçalves | 2401698 |
