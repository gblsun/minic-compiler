# minic-compiler

Compilador da linguagem **MINIC** — projeto incremental da disciplina de
Compiladores, construído em quatro etapas e implementado **duas vezes**, em
Python e em C, com a exigência de que as duas versões produzam exatamente a
mesma saída para a mesma entrada.

| Etapa | Componente | Status |
|---|---|---|
| 1 | Analisador léxico (scanner) | ✅ entregue |
| 2 | Analisador sintático e AST (parser) | ✅ **concluída** — 195/195 verificações ([resultados](docs/resultados-etapa2.md)) |
| 3 | Análise semântica e IR | não iniciada |
| 4 | Geração de código e otimização | não iniciada |

Detalhes de cada etapa, prazos e comandos exigidos por cada atividade:
[docs/etapas.md](docs/etapas.md).

- **Disciplina**: Compiladores — 202602 CC 6A Manhã (Ciência da Computação, 6º semestre, 2026)
- **Professor**: Alex Torquato Souza Carneiro

**Grupo** (ordem alfabética):

| Nome | RA |
|---|---|
| Fellipe Augusto Silva Pereira | 2401525 |
| Gabriel Muchon Pavanelli | 2401895 |
| Paloma Eduarda Soares Leite | 2401660 |
| Victor Wenzel Martins Gonçalves | 2401698 |

## Estrutura do repositório

Cada pasta tem seu próprio README com mais detalhes:

```text
minic-compiler/
├── parser.py  ponto de entrada da etapa 2 em Python (exigido pelo enunciado)
├── parser.c   ponto de entrada da etapa 2 em C (compila src/c/ em unidade única)
├── docs/      documentação derivada da especificação (o que implementar e por quê)
├── src/       código-fonte — src/python/ e src/c/, as duas implementações
├── tests/     suítes de regressão do projeto (entradas, saídas gravadas, runners)
└── ref/       material original da disciplina: especificação, aulas, enunciados,
              scripts e pacotes de teste oficiais (fonte normativa, não editar)
```

Atalhos úteis:

| Quero… | Vá para |
|---|---|
| entender a linguagem | [docs/especificacao.md](docs/especificacao.md) (léxico), [docs/gramatica.md](docs/gramatica.md) (sintaxe) |
| entender o pipeline e os contratos | [docs/arquitetura.md](docs/arquitetura.md) |
| ver a AST e sua notação | [docs/ast.md](docs/ast.md) |
| ver os resultados dos testes da etapa 2 | [docs/resultados-etapa2.md](docs/resultados-etapa2.md) |
| saber como a entrega é avaliada | [ref/testes-oficiais/README.md](ref/testes-oficiais/README.md) |
| saber o que vem na etapa 3 | [docs/etapas.md](docs/etapas.md) |

## Etapa 1 — analisador léxico ✅

Reconhece os tokens da MINIC ([docs/tokens.md](docs/tokens.md)) a partir de um
arquivo `.mc`, sem usar `re`/`flex` — o reconhecimento é manual, caractere a
caractere, que é o objetivo da disciplina.

- **stdout**: um token por linha, no formato `<TOKEN, lexema>` (tokens com
  atributo) ou só `TOKEN` (palavras reservadas e símbolos).
- **stderr**: erros léxicos, no formato `Erro léxico na linha L, coluna C: mensagem.`
- **código de saída**: `0` sem erro léxico, `2` com erro léxico, `1` em erro de uso.

O mesmo contrato vale para as duas implementações, e é isso que os testes
verificam.

### Pré-requisitos

- Python 3.10+ (testado com Python 3.14) — para a versão Python e para `tests/run_tests.py`
- Um compilador C11 (ex.: `gcc`) — para a versão C

### Rodar o lexer em Python

Sem dependências externas — só a biblioteca padrão.

```bash
python src/python/main.py tests/inputs/valido_soma.mc
```

Troque o arquivo por qualquer outro `.mc`; os exemplos prontos estão em
[tests/inputs/](tests/inputs/), incluindo casos de erro (`erro_*.mc`).

### Rodar o lexer em C

```bash
gcc -Wall -Wextra -std=c11 src/c/lexer.c src/c/main.c -o src/c/scanner
src/c/scanner tests/inputs/valido_soma.mc
```

Saída idêntica à da versão Python — ver [src/c/README.md](src/c/README.md) para
como o algoritmo foi portado.

### Rodar a suíte de testes

Três scripts comparam a saída do lexer, para cada `.mc` em `tests/inputs/`, com
o resultado gravado em `tests/expected/` — ver [tests/README.md](tests/README.md)
para o papel de cada um:

```bash
python tests/run_tests.py                 # runner "dono" dos golden files (Python)
bash src/python/test_scanner_python.sh    # mesma suíte, formato pedido pela disciplina (Python)
bash src/c/test_scanner_c.sh              # mesma suíte, compilando e testando a versão em C
```

Se você alterar o lexer (Python **ou** C) de propósito e precisar regravar os
resultados esperados — sempre a partir da versão Python, que é a autoridade dos
golden files:

```bash
python tests/run_tests.py --update
```

### Interface Streamlit (opcional)

Interface web para testar o lexer interativamente: escolher um exemplo, colar
código ou fazer upload de um `.mc`, e ver os tokens/erros numa tabela. Não faz
parte da entrega obrigatória.

```bash
pip install -r requirements.txt          # só na primeira vez, idealmente num venv
python -m streamlit run src/python/app_streamlit.py
```

Abre `http://localhost:8501` no navegador; `Ctrl+C` no terminal para parar.

> No Windows, o comando `streamlit` direto pode não ser reconhecido porque o
> `pip` instala o executável numa pasta (`...\Python\Scripts`) que nem sempre
> está no `PATH`. `python -m streamlit` sempre funciona, pois não depende do
> `PATH`.

## Etapa 2 — analisador sintático e AST ✅

Parser por **descida recursiva** (sem yacc/bison/ANTLR) que consome os tokens
do scanner da etapa 1, reconhece a gramática completa de
[docs/gramatica.md](docs/gramatica.md) e constrói a AST de
[docs/ast.md](docs/ast.md).

- **stdout**: a AST, quando o programa é sintaticamente válido.
- **stderr**: os diagnósticos, no formato
  `Erro sintático na linha L, coluna C: esperado X; encontrado Y.` — com
  recuperação em modo pânico, então vários erros são reportados numa só
  execução.
- **código de saída**: `0` aceito, `3` erro sintático, `2` erro léxico
  (o parser não roda), `1` erro de uso.

### Rodar o parser (comandos do enunciado)

```bash
python parser.py codigo.c     # versão Python
```

```bash
gcc -Wall -Wextra -std=c11 parser.c -o parser
./parser codigo.c             # versão C
```

Exemplo, com um dos casos oficiais:

```bash
python parser.py ref/testes-oficiais/testes-parser-50/casos/09_if_com_else/codigo.c
```

```text
Program(Function(int main() Block(VarDecl(int x=Lit(int,1)), If(Id(x),ExprStmt(Assign(Id(x),Lit(int,2))),ExprStmt(Assign(Id(x),Lit(int,3)))), Return(Id(x)))))
```

Opções extras (iguais nas duas implementações): `--tree` imprime a AST
indentada, um nó por linha; `--tokens` imprime só os tokens do scanner.

### Testes da etapa 2

```bash
python tests/run_parser_tests.py          # 50 casos oficiais + 15 próprios + equivalência Python/C
python tests/run_parser_tests.py --update # regrava os golden dos casos próprios
bash src/python/test_parser_python.sh     # script oficial da disciplina (Python)
bash src/c/test_parser_c.sh               # script oficial da disciplina (C)
```

Os dois últimos são atalhos para os scripts do professor, que também podem ser
chamados direto — o pacote de 50 casos está versionado no repositório:

```bash
LC_ALL=C.UTF-8 bash ref/scripts/testar_parser_python.sh ref/testes-oficiais/testes-parser-50 ./parser.py
LC_ALL=C.UTF-8 bash ref/scripts/testar_parser_c.sh      ref/testes-oficiais/testes-parser-50 ./parser.c
```

> O `LC_ALL=C.UTF-8` não é frescura: o script oficial conta os erros com um
> `grep` que procura por "sintático", e em locale `C` o "á" não casa — o
> contador "Erros sintáticos" apareceria como 0 mesmo com o parser detectando
> todos os 25. Os wrappers em `src/` já exportam isso.

Resultado atual: **195/195 verificações** — 50/50 casos oficiais e 15/15 casos
próprios em cada implementação, mais 65/65 entradas com saída byte a byte
idêntica entre Python e C. Os scripts oficiais do professor marcam
16 aprovados / 25 erros sintáticos detectados, que é o teto do pacote de testes
— o porquê está em [docs/resultados-etapa2.md](docs/resultados-etapa2.md).

### Onde está o quê

| Arquivo | Papel |
|---|---|
| [parser.py](parser.py) | ponto de entrada exigido pelo enunciado (atalho para `src/python/parser.py`) |
| [parser.c](parser.c) | ponto de entrada em C; compila os fontes de `src/c/` como unidade única, que é como o script do professor compila |
| [src/python/parser.py](src/python/parser.py) | o parser em Python + CLI |
| [src/python/minic_ast.py](src/python/minic_ast.py) | nós da AST e as duas impressões (compacta e indentada) |
| [src/c/parser.c](src/c/parser.c) / [parser.h](src/c/parser.h) | o parser em C |
| [src/c/ast.c](src/c/ast.c) / [ast.h](src/c/ast.h) | a AST em C |
| [src/c/parser_main.c](src/c/parser_main.c) | CLI em C |
| [tests/run_parser_tests.py](tests/run_parser_tests.py) | as três suítes de teste do parser |
