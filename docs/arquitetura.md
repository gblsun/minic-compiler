# Arquitetura do compilador MINIC

Como as partes se encaixam, quais são os contratos entre elas e o que é
interface pública (não pode mudar sem quebrar testes ou etapas vizinhas).
Baseado nas Seções 1, 11 e 12 de
[`ref/especificacao-completa-minic.pdf`](../ref/especificacao-completa-minic.pdf).

## Pipeline

```text
arquivo .mc / .c
      │
      ▼
  scanner  ──► sequência de tokens (tipo, lexema, linha, coluna) + erros léxicos   [etapa 1 ✅]
      │
      ▼
  parser   ──► AST ou diagnóstico sintático                                        [etapa 2 ✅]
      │
      ▼
  semântica ─► AST anotada + tabela de símbolos                                    [etapa 3]
      │
      ▼
  IR (3AC) ──► otimizador ──► gerador de código                                    [etapa 4]
```

Cada etapa tem um componente, uma responsabilidade e **uma saída principal**
(Seção 1.1 da especificação):

| Componente | Responsabilidade | Saída principal |
|---|---|---|
| Lexer | converter caracteres em tokens | sequência de tokens com linha e coluna |
| Parser | validar a gramática e construir a AST | AST ou diagnóstico sintático |
| Analisador semântico | verificar nomes, escopos, tipos e regras contextuais | AST anotada ou diagnósticos |
| Gerador de IR | traduzir a AST para código de três endereços | IR estruturada |
| Otimizador | aplicar transformações corretas à IR | IR otimizada |
| Gerador de código | produzir código de destino executável | assembly, LLVM IR ou VM |

Ver [`etapas.md`](etapas.md) para o status e os prazos de cada etapa.

## Duas implementações, um comportamento

Todo o projeto é implementado **duas vezes**, em Python
([`src/python/`](../src/python/)) e em C ([`src/c/`](../src/c/)), com a
exigência de que as duas produzam a mesma saída para a mesma entrada. Isso não
é redundância acadêmica gratuita: é o que permite testar uma contra a outra
(mesmos arquivos golden, dois runners) e é o que o enunciado da disciplina pede.

A regra que mantém isso administrável: **a versão Python é a autoridade** —
ela é escrita primeiro, é ela que gera os arquivos golden em
`tests/expected/`, e a versão C é portada dela à mão, função por função. Ver
[`src/c/README.md`](../src/c/README.md) para o de-para das estruturas.

## Contrato entre scanner e parser

O que o parser pode assumir sobre o que recebe do scanner (Python:
`Lexer(source).tokenize()` em [`src/python/lexer.py`](../src/python/lexer.py);
C: `lexer_init`/`lexer_tokenize` em [`src/c/lexer.h`](../src/c/lexer.h)):

| Campo do token | Garantia |
|---|---|
| `type` | um dos nomes de [`tokens.md`](tokens.md) — palavras reservadas com prefixo `KW_`, `IDENT`, `INT`, `FLOAT`, `CHAR`, `STRING`, operadores, delimitadores, `EOF` |
| `lexeme` | o texto original, exatamente como apareceu no fonte |
| `value` | o valor já convertido para `IDENT`/`INT`/`FLOAT`/`CHAR`/`STRING`; `None`/ausente nos demais |
| `line`, `column` | 1-based, apontando para o **primeiro** caractere do lexema |

Mais:

- **Espaços e comentários não geram token**, mas contam em linha/coluna.
- **O último token é sempre `EOF`**, com a posição do fim do arquivo. Ele é
  filtrado na impressão da CLI da etapa 1, mas existe na lista e é o marcador
  de fim de entrada do parser — nada de olhar o tamanho da lista para saber
  quando parar.
- **Tokenizar não aborta no primeiro erro léxico.** O scanner opera em modo
  pânico: registra o erro, descarta o mínimo necessário e continua. O resultado
  é o par `(tokens, errors)` — tokens válidos convivem com a lista de erros.
- Decisão implementada na etapa 2: havendo erro léxico, o parser **não roda** —
  os erros léxicos são impressos e o processo sai com código 2 (um fluxo de
  tokens corrompido só geraria erros sintáticos em cascata, mascarando a causa
  real).

## Interface de linha de comando

A especificação (Seção 11) descreve uma CLI única para o compilador completo:

| Comando | Resultado |
|---|---|
| `minic programa.mc` | compila o programa |
| `minic --tokens programa.mc` | exibe os tokens |
| `minic --ast programa.mc` | exibe a AST |
| `minic --symbols programa.mc` | exibe a tabela de símbolos |
| `minic --ir programa.mc` | exibe a IR |
| `minic --check programa.mc` | executa análises sem gerar código |
| `minic --target=asm programa.mc` | gera assembly |
| `minic -O0` / `-O1 programa.mc` | desativa/ativa otimizações |

**Este repositório não implementa essa CLI unificada**, e por um motivo
concreto: cada atividade da disciplina define seus próprios comandos de
invocação, e é por eles que a entrega é avaliada.

| Etapa | Comando exigido pela atividade | O que existe aqui |
|---|---|---|
| 1 — scanner | `python scanner.py arquivo.c` / `./scanner arquivo.c` | `python src/python/main.py arquivo.mc` e `src/c/scanner arquivo.mc` |
| 2 — parser | `python parser.py codigo.c` / `./parser codigo.c` | `parser.py` e `parser.c` na raiz, atendendo aos comandos exatos |

A etapa 2 exige um ponto de entrada chamado literalmente `parser.py` (e um
executável `parser`), e é por isso que existem [`parser.py`](../parser.py) e
[`parser.c`](../parser.c) na raiz: o primeiro só ajusta o `sys.path` e chama o
`main()` de `src/python/parser.py`; o segundo inclui os fontes de `src/c/` para
compilar como uma única unidade de tradução, que é como o script de teste do
professor compila. Nenhum dos dois duplica lógica.

## Códigos de saída (Seção 11.1)

| Código | Significado | Usado por |
|---|---|---|
| 0 | sucesso | todas as etapas |
| 1 | uso inválido da ferramenta | argumento faltando, arquivo ilegível |
| 2 | erro léxico | etapa 1 ✅ |
| 3 | erro sintático | etapa 2 |
| 4 | erro semântico | etapa 3 |
| 5 | erro na geração de código | etapa 4 |
| 6 | erro durante execução | etapa 4 |

O runner de testes compara o código de saída junto com stdout e stderr — ver
[`tests/README.md`](../tests/README.md).

## Diagnósticos (Seção 12)

Todo diagnóstico informa **categoria, linha, coluna, lexema/token relevante** e,
quando possível, uma sugestão de correção. Formato dos exemplos da
especificação:

```text
Erro léxico na linha 3, coluna 12: símbolo "@" não reconhecido.
Erro sintático na linha 4, coluna 8: esperado ")" após a condição; encontrado "{".
Erro semântico na linha 6, coluna 5: identificador "resultado" não foi declarado.
```

Convenções do projeto:

- **tokens em stdout, diagnósticos em stderr** — assim cada fluxo é comparado
  independentemente nos testes, e o usuário pode redirecionar um sem o outro;
- **mais de um erro por execução**: o scanner já faz isso (modo pânico); o
  parser deve fazer o mesmo, sincronizando em `;`, `}`, `)` ou numa palavra
  reservada que inicie construção, "sem mascarar a causa original";
- a frase começa com a categoria exata (`Erro léxico`, `Erro sintático`) — o
  script oficial `testar_parser_*.sh` procura por esse padrão com `grep` para
  contar os erros sintáticos detectados.

## Organização de pastas

A Seção 14 da especificação recomenda um layout por fase
(`src/lexer/`, `src/parser/`, `src/ast/`, `src/semantic/`, …). Este repositório
usa um layout **por linguagem de implementação** (`src/python/`, `src/c/`),
porque a exigência de duas implementações equivalentes é a restrição mais forte
do projeto: manter Python e C lado a lado por fase espalharia cada comparação
por duas árvores diferentes. Dentro de cada linguagem, os arquivos seguem a
nomenclatura por fase (`lexer.py`, `parser.py`, `ast.py`, …).
