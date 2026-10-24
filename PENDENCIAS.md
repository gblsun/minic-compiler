# Pendências — o que falta fazer, decidir e revisar

Lista de trabalho do grupo. Aqui só entra o que está **em aberto**, **precisa
de uma segunda opinião** ou é **limitação conhecida**. Ideias novas vão para
[`possiveis_features.md`](docs/possiveis_features.md); o que já foi resolvido
fica no [histórico](#6-histórico-o-que-já-foi-resolvido), no fim.

**Responsável pela lista: Fellipe Augusto Silva Pereira (RA 2401525).** Ele
revisa, prioriza e reparte os itens no grupo. Para passar um item adiante, é
só editar a coluna *Responsável*.

## Situação em 24/10/2026

**Todas as etapas da especificação estão implementadas**, em Python e em C,
com saída idêntica byte a byte e tudo testado no CI a cada push:

| Etapa | Situação | Onde conferir |
|---|---|---|
| 1 — léxico | entregue | [`etapas.md`](docs/etapas.md#etapa-1--analisador-léxico-) |
| 2 — parser e AST (AP 1) | entregue; 50/50 casos oficiais | [`resultados-etapa2.md`](docs/resultados-etapa2.md) |
| 3 — semântica (AP 2) | entregue; 20/20 nos scripts oficiais | [`entrega-ap2.md`](docs/entrega-ap2.md) |
| IR (Testes para o MINIC) | entregue; 30/30 casos oficiais | [`entrega-ir.md`](docs/entrega-ir.md) |
| 4 — código e otimização | **implementada, falta entregar**: back-end, otimizações de laço, benchmarks, manual, apresentação | [`etapas.md`](docs/etapas.md#etapa-4--geração-de-código-e-otimização-) |

O que sobra é, sobretudo, **operacional** (seção 1) e **decisões para o grupo
confirmar** (seção 2).

## Índice

| Item | Assunto | Prioridade | Status | Responsável |
|---|---|---|---|---|
| [1](#1-antes-de-entregar-a-etapa-4) | Checklist da entrega da etapa 4 | **alta** | em aberto | grupo |
| [2.1](#21-saída-padrão-do-minic) | `minic programa.c` sem opções dá o relatório da AP 2, e não compila | alta | decisão | grupo |
| [2.2](#22-alvo-do-back-end-máquina-virtual-de-pilha-própria) | Alvo do back-end: VM de pilha própria | média | decisão | grupo |
| [2.3](#23-regras-de-execução-que-a-especificação-não-fixa) | Regras de execução que a especificação não fixa | média | decisão | grupo |
| [2.4](#24-ap-2-pacote-oficial--especificação) | AP 2: pacote oficial × especificação (`main`, `3 = n`, `a = b = 3`) | baixa | decisão | grupo |
| [2.5](#25-etapa-2-divergências-do-pacote-oficial) | Etapa 2: divergências do pacote oficial (casos 24 e 41, espaços) | baixa | decisão | Fellipe |
| [3](#3-limitações-conhecidas) | Limitações conhecidas (não são defeitos) | — | registradas | — |
| [4](#4-material-da-disciplina-em-falta) | Material da disciplina em falta | baixa | em aberto | Fellipe |
| [5](#5-revisões-de-código-sugeridas) | Revisões de código sugeridas | baixa | em aberto | Fellipe |
| [6](#6-histórico-o-que-já-foi-resolvido) | Histórico: o que já foi resolvido | — | resolvido | — |

---

## 1. Antes de entregar a etapa 4

- [ ] **Ler o enunciado da etapa 4 assim que for publicado no Classroom.**
      Até agora a implementação segue a Seção 15 da especificação e a apostila
      da aula 19. Se o enunciado pedir um comando de invocação, um formato de
      saída ou um pacote de testes próprios, ajustar e registrar em
      [`etapas.md`](docs/etapas.md).
- [ ] **Confirmar o prazo e a forma de entrega** (link do repositório, PDF,
      apresentação ao vivo?).
- [ ] **Juntar o branch da etapa 4 na `main`**, depois do CI verde no PR.
- [ ] **Revisar o manual e a apresentação**: [`docs/manual.md`](docs/manual.md)
      e [`docs/apresentacao.md`](docs/apresentacao.md). Os slides estão em
      Markdown no formato Marp; para exportar em PDF, use a extensão "Marp for
      VS Code" (*Export Slide Deck*).
- [ ] **Ensaiar a demonstração** do último slide, com a versão em C compilada
      na máquina da apresentação.
- [ ] Opcional: **regravar os benchmarks numa máquina Linux**
      (`python3 benchmarks/rodar.py --escrever`). As instruções executadas não
      mudam; só a coluna de tempo e a linha de ambiente.
- [ ] **Cada integrante clonar do zero e rodar**:
      ```bash
      gcc -Wall -Wextra minic.c -o minic
      ./minic --run -O1 tests/exec_inputs/spec_19_1_fatorial.c     # 120
      python3 tests/run_backend_tests.py --python
      ```
- [ ] **Avisar o professor sobre os scripts do pacote de IR**, se ele os usar:
      do jeito que vieram, dão 0/30 para qualquer compilador
      ([`entrega-ir.md`](docs/entrega-ir.md#por-que-não-pelos-scripts-do-pacote)).
      Uma linha no comentário da entrega basta.

---

## 2. Decisões que o grupo precisa confirmar

Nenhuma é defeito: todas estão implementadas, documentadas e testadas. Falta
alguém do grupo ler e concordar, ou discordar antes de entregar.

### 2.1 Saída padrão do `minic`

A Seção 11 diz que `minic programa.mc` "compila o programa". Aqui, **sem
opção, o `minic` imprime o relatório da análise semântica** (o mesmo que
`--check`), porque é assim que o script da AP 2 o chama e compara a saída com
o gabarito. Compilar e executar é `--run`, e só gerar o código é
`--target=asm`.

Os scripts do pacote de IR também chamam `minic arquivo.c` sem opções, mas
mudar a saída padrão não os faria passar: eles comparam o gabarito inteiro,
com prosa, byte a byte ([`ir.md`](docs/ir.md#os-scripts-oficiais-test_ir_pythonsh--test_ir_csh)).

Se o enunciado da etapa 4 pedir outra saída padrão, a troca fica no `main()` de
[`src/python/minic.py`](src/python/minic.py) e de
[`src/c/minic_main.c`](src/c/minic_main.c): é o valor inicial de `modo`. Os
testes da AP 2 em `tests/run_semantic_tests.py` e no CI precisariam então
passar `--check`.

### 2.2 Alvo do back-end: máquina virtual de pilha própria

A Seção 10 aceita assembly de pilha, x86-64, LLVM IR ou "código para uma
máquina virtual própria", desde que a escolha seja registrada e mantida
estável. A escolha foi a **VM de pilha própria** (MVM). Os motivos estão em
[`backend.md`](docs/backend.md#escolha-do-alvo):

- é o modelo da aula 17;
- roda igual nas duas linguagens e em qualquer sistema;
- os testes conferem a saída do programa compilado, e não só o texto gerado.

Revisar: o grupo concorda em registrar essa escolha como definitiva? Trocar
de alvo depois exigiria refazer `geracao.*` e `vm.*`, mas não o resto.

### 2.3 Regras de execução que a especificação não fixa

A especificação não diz o que acontece em vários pontos da execução. O
back-end decidiu assim, e as duas implementações seguem as mesmas regras
([`backend.md`](docs/backend.md#semântica-de-execução)):

| Ponto | Decisão |
|---|---|
| tamanho do `int` | 32 bits, com transbordamento em complemento de dois (como o C na prática) |
| `INT_MIN / -1` | dá `INT_MIN` (em C é comportamento indefinido) |
| variável não inicializada | vale zero (`0`, `0.0`, `false`, caractere nulo) |
| `print` de float | menor número de dígitos que representa o valor, com `.0` se for inteiro |
| `read` | itens separados por espaço; `int` precisa caber em 32 bits |
| profundidade de recursão | até 10 000 chamadas aninhadas; passou disso, erro de execução (código 6) |
| programa sem `main` | aceito pela semântica (como na AP 2), mas não pode ser executado: código 5 |

### 2.4 AP 2: pacote oficial × especificação

Três decisões da etapa 3 seguem os testes oficiais da AP 2 e não o texto da
especificação ([`semantica.md`](docs/semantica.md#decisões-de-projeto)):

- **`main` não é exigida pela semântica**: os 10 programas válidos da AP 2 usam
  `principal`. Para executar, `main` é exigida (item 2.3).
- **`3 = n;` é erro semântico (SEM013)**, e não sintático, no `minic`; o parser
  da etapa 2 continua rejeitando como antes.
- **`a = b = 3` tem valor** (o tipo do destino), por causa do caso oficial 09.

### 2.5 Etapa 2: divergências do pacote oficial

- O caso oficial 24 é comparado contra uma AST corrigida: o gabarito tem um `)`
  faltando ([`ref/testes-oficiais/README.md`](ref/testes-oficiais/README.md#defeitos-conhecidos-do-pacote)).
- A comparação de AST normaliza espaços, porque o pacote formata o mesmo
  construto de dois jeitos ([`ast.md`](docs/ast.md#por-que-não-dá-para-comparar-byte-a-byte)).
- Três pontos seguem os casos oficiais e não a gramática do PDF: comandos
  soltos no topo, vetor em declaração global e `main` não exigida
  ([`gramatica.md`](docs/gramatica.md#divergências-entre-a-especificação-e-os-casos-oficiais)).
- No caso 41 (`a[1 = 2;`), nossa mensagem aponta o `=`, e não o
  `FECHA_COLCHETE` da pista do pacote. A gramática permite atribuição dentro
  do índice, então o primeiro problema real é o alvo inválido.

---

## 3. Limitações conhecidas

Não são defeitos: são escolhas de escopo, documentadas, que uma versão futura
poderia melhorar.

| Limitação | Efeito | Onde está documentada |
|---|---|---|
| Tradução direta da IR para a pilha, sem *peephole* | cada temporário vira `STORE` + `LOAD` | [`backend.md`](docs/backend.md#limitações) |
| Subexpressões comuns só dentro do bloco básico | repetições entre blocos não são aproveitadas | [`ir.md`](docs/ir.md#limitações) |
| Sem reassociação (`a + 20 + 28 - 28`) | contas com constantes no meio de variáveis ficam | [`benchmarks.md`](docs/benchmarks.md#leitura) |
| Código invariante só move temporários | `x = a * b` dentro do laço, com `x` do usuário, fica | [`ir.md`](docs/ir.md#limitações) |
| Sem *inlining* nem recursão de cauda | chamadas custam o mesmo em `-O0` e `-O1` | [`benchmarks.md`](docs/benchmarks.md#o-que-ainda-limitaria-o-ganho) |
| VM interpretada, sem código nativo | é o que a Seção 10 permite com "VM própria" | [`backend.md`](docs/backend.md#limitações) |
| Interpretador de testes (`tests/ir_interp.py`) sem limites de vetor | o oráculo ignora programas com acesso fora do vetor; esse erro é testado à parte | [`tests/README.md`](tests/README.md#suíte-do-back-end-geração-de-código-e-execução) |

---

## 4. Material da disciplina em falta

### 4.1 Pacote de fixtures do scanner está incompleto

O `MANIFESTO.md` de
[`ref/testes-oficiais/testes-scanner-minic/`](ref/testes-oficiais/testes-scanner-minic/)
lista sete casos válidos (`v01`–`v07`) e um oitavo inválido (`i07`) que **não
vieram no download**, e o `i06` veio sem o `.minic`. Para completar, é
preciso baixar o pacote de novo no Classroom ou pedir ao professor. Não afeta
nenhuma entrega.

### 4.2 Nosso scanner não produz o JSONL das fixtures oficiais

A etapa 1 foi entregue com a notação do material de aula (`<TOKEN, lexema>`),
e as fixtures oficiais usam JSONL com outros nomes de token. Se um dia for
preciso rodá-las, o caminho é um modo `--jsonl` com um mapa de nomes, sem
tocar nas regras léxicas
([diferenças](ref/testes-oficiais/README.md#divergências-entre-estas-fixtures-e-o-nosso-scanner)).

---

## 5. Revisões de código sugeridas

Nada quebrado, mas vale um olhar:

- **Formato do float duplicado.** `formatar_float` existe em
  [`src/python/vm.py`](src/python/vm.py) e em
  [`tests/ir_interp.py`](tests/ir_interp.py), de propósito: o interpretador de
  testes não importa o compilador, para continuar sendo um oráculo
  independente. Se o formato mudar, os dois têm de mudar juntos, e o
  `run_backend_tests.py` acusa a diferença.
- **`ast_free()` em [`src/c/ast.c`](src/c/ast.c) não é usada por ninguém.** O
  parser libera os nós com `ast_free_node()` (raso). Manter como API pública ou
  remover?
- **O passe de laços recalcula tudo a cada transformação** (blocos,
  dominadores, laços). É quadrático no tamanho da função, o que é irrelevante
  nos programas da disciplina, mas seria o primeiro ponto a otimizar para
  entradas grandes.
- **Os runners não têm um modo "só um caso"** (`--caso 24`), o que ajudaria a
  depurar sem rolar a saída inteira.

---

## 6. Histórico: o que já foi resolvido

Registro curto. Os detalhes de cada item estão no histórico do git deste
arquivo.

| Item | Problema | Resolução |
|---|---|---|
| Léxico 1 | Python aceitava letra e dígito fora do ASCII; o C não | checagem ASCII explícita no `lexer.py`; casos de regressão em `tests/parser_inputs/` |
| Léxico 2 | mensagem do C com byte cru quebrava a saída UTF-8 | byte inválido sai como `\xNN` nas duas versões |
| Léxico 3 | coluna em bytes no C, em caracteres no Python | o C passou a contar caracteres UTF-8 (etapa 3) |
| Parser | expressão muito aninhada: traceback no Python, *crash* no C | limite de 200 níveis, com diagnóstico normal e código 3 |
| Ambiente 1 | rodar a entrega em Linux | o CI faz isso a cada push (Ubuntu 22.04 e 24.04) |
| Ambiente 2 | versão do Python do professor | o CI testa 3.10 e 3.12; o código exige 3.10+ |
| Ambiente 3 | `parser.py` da raiz × módulo `parser` embutido | carregamento por caminho (`importlib`) |
| Ambiente 4 | `grep` do script oficial em locale C | mensagem "Erro de sintaxe" em ASCII |
| Ambiente 5 | CRLF nos gabaritos da AP 2 | pacote marcado como `-text`; saída reproduz os bytes |
| IR | tipo do `read` em vetor se perdia na renumeração do `-O1` | a renumeração leva junto o tipo de cada temporário |

Checklists das entregas anteriores:

- **AP 1 (etapa 2):** repositório público, `git push` e correções feitas.
- **AP 2 (etapa 3):** bateria completa verde em 10/10/2026
  ([`resultados-etapa3.md`](docs/resultados-etapa3.md#última-execução-completa)).
- **Testes da IR:** PR #3 juntado na `main` em 24/10/2026, com o CI verde.
