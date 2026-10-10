# minic-compiler

Compilador da linguagem **MINIC**, desenvolvido de forma incremental na disciplina de Compiladores e implementado **duas vezes**, em **Python** e em **C**. As duas versões produzem exatamente a mesma saída (byte a byte) para a mesma entrada, e isso é verificado por testes automáticos a cada `push`.

```
código-fonte ──► scanner ──► tokens ──► parser ──► AST ──► analisador semântico ──► relatório
                 (etapa 1)              (etapa 2)          (etapa 3)
```

Tudo é escrito à mão, sem `re`, `flex`, `yacc`, `bison` ou ANTLR: o scanner reconhece caractere a caractere e o parser é de descida recursiva.

## Status

| Etapa | Atividade | Componente                          | Status                                                                                      |
| :---: | :-------: | ----------------------------------- | ------------------------------------------------------------------------------------------- |
|   1   |     —     | Analisador léxico (scanner)         | ✅ entregue                                                                                  |
|   2   | **AP 1**  | Analisador sintático e AST (parser) | ✅ concluída: 201/201 verificações ([resultados](docs/resultados-etapa2.md))                 |
|   3   | **AP 2**  | Analisador semântico                | ✅ concluída: 20/20 testes oficiais em Python e em C, 184/184 verificações ([entrega](docs/entrega-ap2.md)) |

Prazos e comandos exigidos por cada atividade: [docs/etapas.md](docs/etapas.md).

## Equipe

**Disciplina:** Compiladores, turma 202602 CC 6A Manhã (Ciência da Computação, 6º semestre, 2026)
**Professor:** Alex Torquato Souza Carneiro

| Nome                            | RA      |
| ------------------------------- | ------- |
| Fellipe Augusto Silva Pereira   | 2401525 |
| Gabriel Muchon Pavanelli        | 2401895 |
| Paloma Eduarda Soares Leite     | 2401660 |
| Victor Wenzel Martins Gonçalves | 2401698 |

---

## Avaliação rápida da AP 1 (analisador sintático)

Comandos do enunciado, na raiz do repositório:

```bash
python parser.py codigo.c                          # Python

gcc -Wall -Wextra -std=c11 parser.c -o parser      # C
./parser codigo.c
```

Com o pacote oficial de 50 casos (versionado em `ref/`):

```bash
bash ref/scripts/testar_parser_python.sh ref/testes-oficiais/testes-parser-50 ./parser.py
bash ref/scripts/testar_parser_c.sh      ref/testes-oficiais/testes-parser-50 ./parser.c
```

O resultado esperado dos scripts é **16 aprovados e 25 erros sintáticos detectados**, que é o teto do pacote (explicação em [docs/resultados-etapa2.md](docs/resultados-etapa2.md)). Detalhes da etapa em [Etapa 2](#etapa-2-analisador-sintático-e-ast-ap-1-) e correções posteriores à avaliação em [docs/correcoes-etapa2.md](docs/correcoes-etapa2.md).

---

## Avaliação rápida da AP 2 (analisador semântico)

Os comandos são **exatamente os do enunciado**, executados na raiz do repositório (onde ficam `minic.py` e `minic.c`):

```bash
./testes_semanticos_py.sh minic.py [diretorio]   # Python
./testes_semanticos_c.sh  minic.c  [diretorio]   # C
```

`[diretorio]` é a pasta de testes da atividade (`minic-testes-semanticos`). A partir de um clone novo:

```bash
git clone https://github.com/gblsun/minic-compiler.git
cd minic-compiler
# coloque aqui os dois scripts testes_semanticos_*.sh e a pasta
# minic-testes-semanticos, como vieram na atividade
chmod +x testes_semanticos_*.sh
./testes_semanticos_py.sh minic.py minic-testes-semanticos
./testes_semanticos_c.sh  minic.c  minic-testes-semanticos
```

Resultado esperado, nas duas versões:

```text
[OK]     minic-testes-semanticos/01_promocao_numerica.c
...
[OK]     minic-testes-semanticos/20_destino_nao_atribuivel.c

Resultado: 20/20 aprovados; 0 reprovados.
```

**Observações**

- **Nada a instalar nem compilar antes.** O script C compila o `minic.c` sozinho (`gcc -Wall -Wextra`, sem nenhum aviso). O Python precisa só de `python3` 3.10 ou mais novo, sem bibliotecas externas.
- **Sem copiar nada.** Os scripts e os testes oficiais também estão versionados em `ref/`:
  ```bash
  ./ref/scripts/testes_semanticos_py.sh minic.py ref/testes-oficiais/testes-semanticos-20
  ./ref/scripts/testes_semanticos_c.sh  minic.c  ref/testes-oficiais/testes-semanticos-20
  ```
- **Um programa isolado:**
  ```bash
  python3 minic.py programa.c
  gcc -Wall -Wextra minic.c -o minic && ./minic programa.c
  ```
- **Fins de linha dos `.gabarito`.** Eles separam as linhas com CRLF e não terminam com quebra de linha, e o script compara com `diff` exato. Se forem abertos e salvos em um editor que converta CRLF em LF, os casos de rejeição deixam de bater. O repositório marca esses arquivos como `-text` no `.gitattributes` para o git não convertê-los.

Os critérios de correção, cada um com a evidência correspondente (desacoplamento, equivalência C × Python, lógica e casos de teste), estão em [docs/entrega-ap2.md](docs/entrega-ap2.md).

---

## Pré-requisitos

- **Python 3.10+** (testado com 3.10, 3.12 e 3.14)
- **Compilador C11** (por exemplo `gcc`)
- Opcional: `pip install -r requirements.txt` apenas para a interface Streamlit

O CI do GitHub roda tudo em Linux a cada push: Ubuntu 22.04 (gcc 11, Python 3.10) e Ubuntu 24.04 (gcc 13, clang 18, Python 3.12). Veja [`.github/workflows/testes.yml`](.github/workflows/testes.yml).

## Estrutura do repositório

```
minic-compiler/
├── minic.py     ponto de entrada da etapa 3 (Python): lexer → parser → semântica
├── minic.c      ponto de entrada da etapa 3 (C), compila src/c/ em unidade única
├── parser.py    ponto de entrada da etapa 2 (Python)
├── parser.c     ponto de entrada da etapa 2 (C), compila src/c/ em unidade única
├── src/         código-fonte: src/python/ e src/c/, as duas implementações
├── tests/       suítes de regressão (entradas, saídas gravadas, runners)
├── docs/        documentação derivada da especificação
├── exercicios/  exercícios de aula resolvidos (aula 13)
└── ref/         material original da disciplina (fonte normativa, não editar)
```

Os arquivos `minic.*` e `parser.*` da raiz são só "cascas": existem porque os scripts do professor compilam **uma única unidade de tradução**. O código de verdade fica em `src/`, com um arquivo por componente:

| Componente           | Python                                                                         | C                                                                                   |
| -------------------- | ------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------- |
| Scanner              | [`lexer.py`](src/python/lexer.py)                                              | [`lexer.c`](src/c/lexer.c) / [`.h`](src/c/lexer.h)                                  |
| AST                  | [`minic_ast.py`](src/python/minic_ast.py)                                      | [`ast.c`](src/c/ast.c) / [`.h`](src/c/ast.h)                                        |
| Parser               | [`parser.py`](src/python/parser.py)                                            | [`parser.c`](src/c/parser.c) / [`.h`](src/c/parser.h)                               |
| Tabela de símbolos   | [`tabela_simbolos.py`](src/python/tabela_simbolos.py)                          | [`tabela_simbolos.c`](src/c/tabela_simbolos.c) / [`.h`](src/c/tabela_simbolos.h)    |
| Analisador semântico | [`semantico.py`](src/python/semantico.py)                                      | [`semantico.c`](src/c/semantico.c) / [`.h`](src/c/semantico.h)                      |
| CLI do pipeline      | [`minic.py`](src/python/minic.py)                                              | [`minic_main.c`](src/c/minic_main.c)                                                |

## Mapa da documentação

| Quero…                                        | Vá para                                                                                              |
| --------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| entender a linguagem                          | [docs/especificacao.md](docs/especificacao.md) (léxico) e [docs/gramatica.md](docs/gramatica.md) (sintaxe) |
| entender o pipeline e os contratos            | [docs/arquitetura.md](docs/arquitetura.md)                                                           |
| ver a AST e sua notação                       | [docs/ast.md](docs/ast.md)                                                                           |
| entender a análise semântica e as mensagens   | [docs/semantica.md](docs/semantica.md)                                                               |
| ver cada teste semântico com seu resultado    | [docs/testes-semanticos.md](docs/testes-semanticos.md)                                               |
| ver os resultados das etapas 2 e 3            | [docs/resultados-etapa2.md](docs/resultados-etapa2.md), [docs/resultados-etapa3.md](docs/resultados-etapa3.md) |
| saber como a entrega é avaliada               | [ref/testes-oficiais/README.md](ref/testes-oficiais/README.md)                                       |
| saber o que ainda falta corrigir ou decidir   | [PENDENCIAS.md](PENDENCIAS.md)                                                                       |

---

## Etapa 1: analisador léxico ✅

Reconhece os tokens da MINIC ([docs/tokens.md](docs/tokens.md)) em um arquivo-fonte.

| Canal         | Conteúdo                                                                                  |
| ------------- | ----------------------------------------------------------------------------------------- |
| stdout        | um token por linha: `<TOKEN, lexema>` (com atributo) ou só `TOKEN` (reservadas e símbolos) |
| stderr        | `Erro léxico na linha L, coluna C: mensagem.`                                             |
| código de saída | `0` sem erro, `2` erro léxico, `1` erro de uso                                          |

```bash
# Python
python src/python/main.py tests/inputs/valido_soma.mc

# C
gcc -Wall -Wextra -std=c11 src/c/lexer.c src/c/main.c -o src/c/scanner
src/c/scanner tests/inputs/valido_soma.mc
```

Exemplos prontos, incluindo casos de erro (`erro_*.mc`), estão em [tests/inputs/](tests/inputs). Para a suíte de testes:

```bash
python tests/run_tests.py                 # runner dono dos golden files
bash src/python/test_scanner_python.sh    # formato pedido pela disciplina (Python)
bash src/c/test_scanner_c.sh              # idem, compilando a versão C
python tests/run_tests.py --update        # regrava os golden files (sempre a partir do Python)
```

Mais detalhes em [tests/README.md](tests/README.md) e [src/c/README.md](src/c/README.md).

<details>
<summary><strong>Interface Streamlit (opcional)</strong></summary>

Interface web para testar o lexer: escolha um exemplo, cole código ou envie um arquivo e veja os tokens e erros em uma tabela. Não faz parte da entrega obrigatória.

```bash
pip install -r requirements.txt     # só na primeira vez, de preferência em um venv
python -m streamlit run src/python/app_streamlit.py
```

Abre em `http://localhost:8501`; use `Ctrl+C` para parar. No Windows, prefira `python -m streamlit`, porque o executável `streamlit` nem sempre está no `PATH`.

</details>

## Etapa 2: analisador sintático e AST (AP 1) ✅

Parser por **descida recursiva** que consome os tokens do scanner, reconhece a gramática de [docs/gramatica.md](docs/gramatica.md) e constrói a AST descrita em [docs/ast.md](docs/ast.md). Há recuperação em modo pânico, então vários erros são reportados em uma só execução.

| Canal         | Conteúdo                                                                   |
| ------------- | -------------------------------------------------------------------------- |
| stdout        | a AST, quando o programa é sintaticamente válido                           |
| stderr        | `Erro de sintaxe na linha L, coluna C: esperado X; encontrado Y.`          |
| código de saída | `0` aceito, `3` erro sintático, `2` erro léxico, `1` erro de uso         |

```bash
python parser.py codigo.c                          # Python

gcc -Wall -Wextra -std=c11 parser.c -o parser      # C
./parser codigo.c
```

Opções extras, iguais nas duas versões: `--tree` (AST indentada, um nó por linha) e `--tokens` (só os tokens do scanner).

Exemplo com um caso oficial:

```bash
python parser.py ref/testes-oficiais/testes-parser-50/casos/09_if_com_else/codigo.c
```

```text
Program(Function(int main() Block(VarDecl(int x=Lit(int,1)), If(Id(x),ExprStmt(Assign(Id(x),Lit(int,2))),ExprStmt(Assign(Id(x),Lit(int,3)))), Return(Id(x)))))
```

**Testes**

```bash
python tests/run_parser_tests.py            # 50 oficiais + 17 próprios + equivalência Python/C
python tests/run_parser_tests.py --update   # regrava os golden dos casos próprios
bash src/python/test_parser_python.sh       # script oficial (Python)
bash src/c/test_parser_c.sh                 # script oficial (C)
```

Resultado: **201/201 verificações**, sendo 50/50 casos oficiais e 17/17 próprios em cada implementação, mais 67/67 entradas com saída idêntica entre Python e C. Os scripts oficiais marcam 16 aprovados e 25 erros sintáticos detectados, que é o teto do pacote de testes (explicação em [docs/resultados-etapa2.md](docs/resultados-etapa2.md)). As correções feitas depois da avaliação da etapa 2 estão em [docs/correcoes-etapa2.md](docs/correcoes-etapa2.md).

> **Por que "Erro de sintaxe" e não "Erro sintático"?** O script oficial conta os erros com um `grep`. Em locale `C`/`POSIX` o "á" não casa, e o contador "Erros sintáticos" dava 0 mesmo com os 25 casos rejeitados. Com a mensagem só em ASCII o resultado é o mesmo em qualquer locale.

## Etapa 3: analisador semântico (AP 2) ✅

Atividade **"AP 2 - Analisador Semântico"** ([enunciado](ref/enunciados/ap-analisador-semantico-enunciado.md)). Recebe a AST da etapa 2 e verifica:

- nomes e escopos, com sombreamento;
- tipos e conversões implícitas (apenas `int → float` e `char → int`);
- vetores, chamadas de função (aridade e tipos dos argumentos) e retornos, inclusive retorno em todos os caminhos;
- condições booleanas e o contexto de `break` e `continue`;
- destinos de atribuição.

Um erro não gera diagnósticos em cascata: o tipo sentinela `ERRO` suprime os derivados, e as partes independentes do programa continuam sendo verificadas.

| Canal         | Conteúdo                                                                                                    |
| ------------- | ----------------------------------------------------------------------------------------------------------- |
| stdout        | um diagnóstico por linha, no formato do gabarito, e o resumo `Análise semântica concluída: N erros; programa aceito\|rejeitado.` |
| stderr        | mensagens de erro léxico ou sintático (nesses casos a semântica não roda)                                   |
| código de saída | `0` aceito, `4` erro semântico, `3` erro sintático, `2` erro léxico, `1` erro de uso                      |

Exemplo de diagnóstico:

```text
SEM003 — linha 3, coluna 9: Não é possível atribuir float a int sem conversão permitida (destino “n”; expressão “2.5”).
```

O catálogo completo (SEM001 a SEM016), as regras e as decisões de projeto estão em [docs/semantica.md](docs/semantica.md).

**Decisões em que o pacote oficial prevaleceu sobre a especificação**

| Ponto            | Especificação                                  | O que foi feito                       | Motivo                                                  |
| ---------------- | ---------------------------------------------- | ------------------------------------- | ------------------------------------------------------- |
| função `main`    | obrigatória (Seção 2)                          | não é exigida                         | os 10 programas válidos da AP 2 não têm `main`          |
| `3 = n;`         | erro de sintaxe (destino deve ser localizável) | erro semântico SEM013                 | caso oficial 20                                         |
| `a = b = 3`      | atribuição "não produz valor"                  | a atribuição tem o tipo do destino    | caso oficial 09                                         |
| formato da saída | —                                              | CRLF entre linhas, sem quebra no fim  | é o conteúdo dos `.gabarito`, comparado com `diff` exato |

**Testes**

```bash
python tests/run_semantic_tests.py            # 20 oficiais + 19 próprios + equivalência Python/C
python tests/run_semantic_tests.py --update   # regrava os golden dos casos próprios
python tests/fuzz_semantico.py --c ./minic    # programas gerados: equivalência + oráculos
```

Resultado: **184/184 verificações**, com 20/20 casos oficiais e 19/19 próprios em cada implementação, e 106/106 programas com saída idêntica entre Python e C. O teste diferencial gera mais 900 programas no CI (aleatórios, bem tipados e com exatamente um erro injetado), com o C compilado com AddressSanitizer e UBSan. Os scripts oficiais do professor dão **20/20** nas duas versões.

## Exercícios da aula 13: tradução dirigida por sintaxe

Os 10 exercícios da aula 13 ([enunciado](ref/exercicios/exercicios-aula-13-enunciados.pdf)) estão resolvidos em [`exercicios/aula13/`](exercicios/aula13/), cada um em Python e em C: atributos sintetizados e herdados, grafo de dependências com ordenação topológica, SDDs S e L-atribuídas, construção de AST, escopos, tipos, recuperação de erros e um mini pipeline scanner → parser → atributos → AST. Eles reaproveitam o lexer da etapa 1, sem copiá-lo.

```bash
python exercicios/aula13/run_tests.py           # 120 casos em Python e C (+ 10 compilações)
python exercicios/aula13/run_tests.py --python  # só Python, se não houver gcc
```

Resultado: **250/250 verificações**. Regras, decisões e limitações de cada exercício estão no [README dos exercícios](exercicios/aula13/README.md).
