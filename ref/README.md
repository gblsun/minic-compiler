# ref/

Material de referência da disciplina — PDFs, scripts e pacotes de teste
originais, **não editados**. Em caso de dúvida ou divergência com os resumos
em [docs/](../docs/), estes arquivos são a fonte normativa.

A única alteração feita aqui foi no **nome** dos arquivos (normalizados para
minúsculas, sem espaços e sem acentos, com número de aula em dois dígitos
para ordenar certo) e na organização em subpastas. O conteúdo é o mesmo que
foi baixado do Google Classroom — a tabela [Nome original de cada
arquivo](#nome-original-de-cada-arquivo) faz o de-para.

```text
ref/
├── especificacao-completa-minic.pdf   contrato da linguagem (fonte normativa nº 1)
├── slides/          slides das aulas 01–11 + roteiro falado da aula 06
├── apostilas/       apostilas das aulas 02–12 + tutorial tokens→AST
├── enunciados/      enunciados das atividades (prints do Classroom)
├── exercicios/      exercícios das aulas 6 a 9 (.docx)
├── scripts/         scripts de teste fornecidos pelo professor
└── testes-oficiais/ pacotes de teste oficiais (scanner e parser)
```

## Especificação

| Arquivo | Conteúdo |
|---|---|
| [especificacao-completa-minic.pdf](especificacao-completa-minic.pdf) | **ESPECIFICAÇÃO COMPLETA DA LINGUAGEM MINIC, v1.0.** Contrato comum às quatro etapas do projeto. Resumida em [`docs/especificacao.md`](../docs/especificacao.md) (léxico, Seção 3), [`docs/gramatica.md`](../docs/gramatica.md) (sintaxe, Seção 4), [`docs/ast.md`](../docs/ast.md) (AST, Seção 8) e [`docs/arquitetura.md`](../docs/arquitetura.md) (pipeline, CLI e diagnósticos, Seções 1, 11 e 12). |

## slides/

| Arquivo | Tema |
|---|---|
| [aula-01-compiladores.pdf](slides/aula-01-compiladores.pdf) | Introdução a compiladores. |
| [aula-02-compiladores.pdf](slides/aula-02-compiladores.pdf) | Arquitetura de compiladores. |
| [aula-03-compiladores.pdf](slides/aula-03-compiladores.pdf) | Análise léxica. |
| [aula-04-compiladores.pdf](slides/aula-04-compiladores.pdf) | Expressões regulares e autômatos finitos. |
| [aula-05-compiladores.pdf](slides/aula-05-compiladores.pdf) | Construção do analisador léxico (maximal munch, casos de erro) — base direta da **etapa 1**. |
| [aula-06-compiladores.pdf](slides/aula-06-compiladores.pdf) | Gramáticas livres de contexto (GLC), BNF/EBNF, árvores de derivação. |
| [aula-07-compiladores.pdf](slides/aula-07-compiladores.pdf) | Análise sintática descendente (descida recursiva). |
| [aula-08-compiladores.pdf](slides/aula-08-compiladores.pdf) | Análise sintática ascendente. |
| [aula-09-compiladores.pdf](slides/aula-09-compiladores.pdf) | Análise descendente com FIRST e FOLLOW. |
| [aula-10-compiladores.pdf](slides/aula-10-compiladores.pdf) | Análise LL(1). |
| [aula-11-compiladores.pdf](slides/aula-11-compiladores.pdf) | Análise sintática ascendente (LR). |
| [aula-06-roteiro.txt](slides/aula-06-roteiro.txt) | Roteiro falado da aula 06, slide por slide. É o material mais didático sobre GLC do pacote, e o único em texto puro (dá para pesquisar com `grep`). |

## apostilas/

| Arquivo | Tema |
|---|---|
| [apostila-aula-02-arquitetura-compiladores.pdf](apostilas/apostila-aula-02-arquitetura-compiladores.pdf) | Arquitetura de compiladores. |
| [apostila-aula-03-analise-lexica.pdf](apostilas/apostila-aula-03-analise-lexica.pdf) | Análise léxica. |
| [apostila-aula-04-expressoes-regulares-automatos-finitos.pdf](apostilas/apostila-aula-04-expressoes-regulares-automatos-finitos.pdf) | Expressões regulares e autômatos finitos. |
| [apostila-aula-05-construcao-analisador-lexico.pdf](apostilas/apostila-aula-05-construcao-analisador-lexico.pdf) | Construção do analisador léxico. |
| [apostila-aula-06-gramaticas-livres-contexto.pdf](apostilas/apostila-aula-06-gramaticas-livres-contexto.pdf) | GLC: não terminais, produções, derivações, precedência e associatividade. |
| [apostila-aula-07-analise-sintatica-descendente.pdf](apostilas/apostila-aula-07-analise-sintatica-descendente.pdf) | Descida recursiva, eliminação de recursão à esquerda, fatoração. |
| [apostila-aula-08-analise-sintatica-ascendente.pdf](apostilas/apostila-aula-08-analise-sintatica-ascendente.pdf) | Empilhamento/redução, handles. |
| [apostila-aula-09-analise-sintatica-descendente-first-follow.pdf](apostilas/apostila-aula-09-analise-sintatica-descendente-first-follow.pdf) | Conjuntos FIRST e FOLLOW, tabela preditiva. |
| [apostila-aula-10-analise-ll1.pdf](apostilas/apostila-aula-10-analise-ll1.pdf) | Análise LL(1), conflitos, parser dirigido por tabela. |
| [apostila-aula-11-analise-sintatica-ascendente.pdf](apostilas/apostila-aula-11-analise-sintatica-ascendente.pdf) | Itens LR, estados, famílias LR. |
| [apostila-aula-12-integracao-scanner-parser-ast.pdf](apostilas/apostila-aula-12-integracao-scanner-parser-ast.pdf) | **Integração scanner–parser–AST.** Contrato dos nós, impressão textual da AST, erros sintáticos, testes de integração. Traz a descrição da atividade prática da **etapa 2** (Partes A, B e C) — resumida em [`docs/roteiro-etapa2-parser.md`](../docs/roteiro-etapa2-parser.md). |
| [tutorial-conversao-tokens-ast-glc.pdf](apostilas/tutorial-conversao-tokens-ast-glc.pdf) | Tutorial passo a passo de tokens → AST com descida recursiva (regras `Factor`/`Term`/`Expr`, modelo de nó em C, erros comuns, como comparar ASTs). O material mais aplicado para escrever o parser. |

## enunciados/

Prints das páginas do Google Classroom, para registrar o que exatamente foi
pedido e quando.

| Arquivo | Conteúdo |
|---|---|
| [ap-analisador-sintatico-enunciado.pdf](enunciados/ap-analisador-sintatico-enunciado.pdf) | Atividade **"AP 1 - analisador sintático"** (10 pontos). Define os comandos obrigatórios de invocação do parser e anexa o pacote `testes-parser-50.tar.gz`. Atenção ao nome: no Classroom a atividade se chama "AP 1", mas corresponde à **etapa 2** do projeto (parser e AST) — a etapa 1 foi o analisador léxico. Prazos em [`docs/etapas.md`](../docs/etapas.md). |
| [scripts-de-teste-para-o-parser-enunciado.pdf](enunciados/scripts-de-teste-para-o-parser-enunciado.pdf) | Como chamar os scripts de teste do parser e onde eles devem ficar. |

> O enunciado da etapa 1 (analisador léxico) não foi salvo em PDF; o texto do
> requisito está citado em [`docs/roteiro-etapa1-lexer.md`](../docs/roteiro-etapa1-lexer.md).

## exercicios/

Exercícios das aulas 6 a 9 (GLC e análise sintática), em `.docx`:
[enunciados](exercicios/exercicios-aulas-6-a-9-enunciados.docx),
[orientações](exercicios/exercicios-aulas-6-a-9-orientacoes.docx),
[códigos](exercicios/exercicios-aulas-6-a-9-codigos.docx),
[resultados esperados](exercicios/exercicios-aulas-6-a-9-resultados-esperados.docx) e
[gabarito](exercicios/exercicios-aulas-6-a-9-gabarito.docx).

## scripts/

Scripts de teste **fornecidos pelo professor**, no formato original. Todos
assumem um layout de pastas plano (scanner, parser e casos de teste no mesmo
diretório), que não é o layout deste repositório — por isso existem versões
adaptadas dentro de `src/`. Ver [`tests/README.md`](../tests/README.md) para
como os runners se encaixam.

| Arquivo | O que faz | Versão adaptada |
|---|---|---|
| [test_scanner_python.sh](scripts/test_scanner_python.sh) | Roda `scanner.py` sobre cada caso e compara a saída **JSONL** com `<entrada>.expected.jsonl`. | [`src/python/test_scanner_python.sh`](../src/python/test_scanner_python.sh) |
| [test_scanner_c.sh](scripts/test_scanner_c.sh) | Compila `scanner.c` com `gcc -Wall -Wextra -std=c11` e faz a mesma comparação JSONL. | [`src/c/test_scanner_c.sh`](../src/c/test_scanner_c.sh) |
| [testar_parser_python.sh](scripts/testar_parser_python.sh) | `bash testar_parser_python.sh ./testes-parser-50 ./parser.py` — roda o parser em cada `codigo.c` e compara stdout+stderr com o esperado. | ainda não existe (etapa 2) |
| [testar_parser_c.sh](scripts/testar_parser_c.sh) | `bash testar_parser_c.sh ./testes-parser-50 ./parser.c` — compila o parser como **unidade única** (`gcc … $PARSER -o parser`) e faz a mesma comparação. | ainda não existe (etapa 2) |

As armadilhas conhecidas desses quatro scripts (comparação byte a byte,
exigência de bit de execução, compilação em arquivo único) estão detalhadas em
[`ref/testes-oficiais/README.md`](testes-oficiais/README.md#armadilhas-dos-scripts-oficiais).

## testes-oficiais/

Pacotes de teste oficiais do scanner e do parser. Têm
[README próprio](testes-oficiais/README.md), porque exigem explicação: o que
cada pacote cobre, o que está faltando neles e como interpretar o resultado
dos scripts.

## Nome original de cada arquivo

Para reencontrar um arquivo no Classroom ou conferir que nada foi trocado:

| Nome neste repositório | Nome original |
|---|---|
| `slides/aula-01-compiladores.pdf` | `Aula01 - Compiladores (1).pdf` |
| `slides/aula-NN-compiladores.pdf` | `AulaNN - Compiladores.pdf` (aulas 02 a 11) |
| `slides/aula-06-roteiro.txt` | `Aula06_Roteiro - Compiladores.txt` |
| `apostilas/apostila-aula-0N-*.pdf` | `apostila-aula-N-*.pdf` (aulas 2 a 9, sem o zero à esquerda) |
| `apostilas/apostila-aula-1N-*.pdf` | idêntico ao original |
| `apostilas/tutorial-conversao-tokens-ast-glc.pdf` | idêntico ao original |
| `enunciados/ap-analisador-sintatico-enunciado.pdf` | `AP 1 - analisador sintático - Sala de Aula.pdf` |
| `enunciados/scripts-de-teste-para-o-parser-enunciado.pdf` | `Scripts de teste para o parser - Sala de Aula.pdf` |
| `exercicios/*.docx` | idêntico ao original |
| `scripts/*.sh` | idêntico ao original |
| `testes-oficiais/testes-scanner-minic/` | `Testes para o analisador léxico/testes-scanner-minic_codes/` |
| `testes-oficiais/testes-parser-50{,.tar.gz}` | `testes-parser-50.tar.gz` |

Duplicatas descartadas na organização (verificadas por hash MD5 antes de sair):
duas cópias extras de `test_scanner_c.sh`/`test_scanner_python.sh`, cópias de
`Aula04`/`Aula05` idênticas às que já estavam em `ref/`, e a variante reduzida
do pacote de testes do scanner (`testes-scanner-minic`, subconjunto de
`testes-scanner-minic_codes`, que é o que ficou).
