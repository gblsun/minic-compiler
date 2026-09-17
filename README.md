# minic-compiler

Compilador da linguagem **MINIC** — projeto incremental da disciplina de
Compiladores, construído em quatro etapas e implementado **duas vezes**, em
Python e em C, com a exigência de que as duas versões produzam exatamente a
mesma saída para a mesma entrada.

| Etapa | Componente | Status |
|---|---|---|
| 1 | Analisador léxico (scanner) | ✅ entregue |
| 2 | Analisador sintático e AST (parser) | ⏳ em aberto — entrega **18/09/2026** |
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
├── docs/     documentação derivada da especificação (o que implementar e por quê)
├── src/      código-fonte — src/python/ e src/c/, as duas implementações
├── tests/    suíte de regressão do projeto (entradas, saídas gravadas, runners)
└── ref/      material original da disciplina: especificação, aulas, enunciados,
             scripts e pacotes de teste oficiais (fonte normativa, não editar)
```

Atalhos úteis:

| Quero… | Vá para |
|---|---|
| entender a linguagem | [docs/especificacao.md](docs/especificacao.md) (léxico), [docs/gramatica.md](docs/gramatica.md) (sintaxe) |
| entender o pipeline e os contratos | [docs/arquitetura.md](docs/arquitetura.md) |
| implementar a etapa 2 | [docs/roteiro-etapa2-parser.md](docs/roteiro-etapa2-parser.md) + [docs/ast.md](docs/ast.md) |
| saber como a entrega é avaliada | [ref/testes-oficiais/README.md](ref/testes-oficiais/README.md) |

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

## Etapa 2 — analisador sintático e AST ⏳

**Ainda não implementada.** O que já está pronto é a documentação necessária
para escrever o parser:

- [docs/gramatica.md](docs/gramatica.md) — EBNF da linguagem, precedência,
  associatividade e as decisões de desambiguação (`else` pendente, alvo de
  atribuição, recursão à esquerda);
- [docs/ast.md](docs/ast.md) — nós da AST, notação exata esperada pelos testes
  oficiais e formato de impressão canônico;
- [docs/roteiro-etapa2-parser.md](docs/roteiro-etapa2-parser.md) — plano de
  execução, ordem de implementação e checklist de conformidade;
- [ref/testes-oficiais/](ref/testes-oficiais/) — os 50 casos oficiais de
  avaliação (25 aceitos com AST esperada, 25 rejeitados), com as armadilhas dos
  scripts do professor já mapeadas.

Quando existir, o parser deverá ser invocável pelos comandos que o enunciado
exige:

```bash
python parser.py codigo.c
./parser codigo.c
```
