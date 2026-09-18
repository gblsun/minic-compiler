# Pendências — o que precisa ser corrigido, revisado e decidido

Lista de trabalho do grupo. Não é backlog de ideias (isso é
[`possiveis_features.md`](docs/possiveis_features.md)): aqui só entra o que está
**errado**, **em aberto** ou **precisa de uma segunda opinião** antes da
entrega.

Como ler cada item:

- **Confirmado** — reproduzido nesta máquina, com o comando anotado.
- **Não verificado** — risco identificado por leitura do código, ainda sem
  teste que prove (ou descarte) o problema.
- **Decisão** — já resolvido de um jeito; falta o grupo concordar.

Estado do projeto no momento em que esta lista foi escrita: etapas 1 e 2
implementadas, 195/195 verificações nas suítes
([`resultados-etapa2.md`](docs/resultados-etapa2.md)), tudo commitado em `main`,
**nada enviado ao GitHub ainda**.

---

## 1. Bugs confirmados

### 1.1 O lexer Python aceita identificador fora do ASCII; o C não

**Prioridade: alta** (quebra a equivalência entre as duas implementações, que é
requisito do projeto) · **Confirmado**

```bash
printf 'int a\xc3\xa7ao = 1;\n' > /tmp/acento.c
python parser.py /tmp/acento.c    # Program(VarDecl(int açao=Lit(int,1)))  exit 0
./parser /tmp/acento.c            # 2 erros léxicos                        exit 2
```

Causa: [`src/python/lexer.py`](src/python/lexer.py) usa `ch.isalpha()`, que
em Python é **Unicode** — `"ç".isalpha()` é `True`. O C usa `isalpha()` da
libc, que em locale C só aceita `[A-Za-z]`. A Seção 3.5 da especificação define
identificador como `[A-Za-z_][A-Za-z0-9_]*`, então **quem está errado é o
Python**.

O que fazer: trocar `isalpha()`/`isalnum()` por checagem ASCII explícita em
`_scan_token` e `_scan_identifier_or_keyword`. Nenhum golden muda (todas as
entradas de teste são ASCII — conferido).

### 1.2 O lexer Python aceita dígito fora do ASCII

**Prioridade: alta** · **Confirmado** · mesma causa e mesma correção do 1.1

```bash
printf 'int x = \xd9\xa3;\n' > /tmp/digito.c
python parser.py /tmp/digito.c    # Program(VarDecl(int x=Lit(int,٣)))  exit 0
./parser /tmp/digito.c            # 2 erros léxicos                     exit 2
```

`"٣".isdigit()` é `True` em Python, e `int("٣")` devolve 3 — ou seja, o número
entra na AST como literal válido. A especificação define inteiro como `[0-9]+`.

### 1.3 Mensagem de erro do C corrompe a saída em byte não-ASCII

**Prioridade: média** (só ocorre em entrada inválida, mas produz saída
inválida) · **Confirmado**

No exemplo do 1.1, o C imprime `símbolo "<byte 0xC3>" não reconhecido` com o
byte cru. Como um byte solto de uma sequência UTF-8 não é UTF-8 válido, quem
lê a saída em UTF-8 quebra (a nossa própria auditoria quebrou com
`UnicodeDecodeError` ao ler o stderr do C).

O que fazer: em [`src/c/lexer.c`](src/c/lexer.c), imprimir bytes não
imprimíveis/não-ASCII na forma `\xNN` em vez do caractere cru.

### 1.4 Expressão muito aninhada: traceback no Python, crash no C

**Prioridade: média** (nenhuma entrada real chega perto disso, mas a falha é
feia nos dois lados) · **Confirmado**

| Parênteses aninhados | Python | C |
|---|---|---|
| 50 | ok (exit 0) | ok (exit 0) |
| 100 | **traceback + exit 1** | ok |
| 1000 | traceback + exit 1 | ok |
| 5000 | traceback + exit 1 | **exit 127 (estouro de pilha)** |

```bash
python -c "open('/tmp/p.c','w').write('int x = ' + '('*100 + '1' + ')'*100 + ';')"
python parser.py /tmp/p.c   # RecursionError
```

Descida recursiva gasta uma moldura de pilha por nível de precedência, então o
limite do Python (~100 parênteses) é bem mais baixo que o do C (~milhares).

O que fazer: contar profundidade no parser e, ao passar de um limite (o mesmo
nas duas implementações), emitir um diagnóstico normal —
`Erro sintático na linha L, coluna C: expressão aninhada demais` — com exit 3,
em vez de traceback/crash. Assim as duas voltam a concordar.

### 1.5 Contagem de coluna: o C conta bytes, o Python conta caracteres

**Prioridade: baixa** · **Confirmado** (consequência dos itens acima)

Em fonte 100% ASCII — todos os testes, todos os casos oficiais — não há
diferença. Com caractere não-ASCII (que só aparece dentro de literais, ou em
entrada inválida), as colunas dos tokens seguintes divergem.

O que fazer: decidir e **documentar** qual é a definição oficial do projeto
("coluna em caracteres" ou "coluna em bytes") e alinhar as duas. Mexer nisso
sem necessidade é arriscado; talvez só documentar já resolva para a entrega.

---

## 2. Decisões tomadas que o grupo precisa revisar

Nenhuma é erro; todas estão implementadas e documentadas. O que falta é alguém
do grupo ler e concordar (ou discordar antes de entregar).

### 2.1 O caso oficial 24 é comparado contra uma AST corrigida

O arquivo `ast.esperada.txt` do caso 24 tem um `)` faltando, o que joga o
`Return` para fora do corpo da função. Nosso runner compara contra a árvore
corrigida e **diz isso no relatório**. Detalhes em
[`ref/testes-oficiais/README.md`](ref/testes-oficiais/README.md#defeitos-conhecidos-do-pacote).

Revisar: vale avisar o professor do defeito na entrega? (Sugestão: sim, uma
linha no comentário da atividade.)

### 2.2 A comparação de AST normaliza espaços

Os arquivos oficiais formatam o mesmo construto de dois jeitos diferentes, então
nenhum impressor determinístico casa com todos. Escolhemos a regra que casa
exatamente com 16 dos 25 e comparamos o resto ignorando espaço em branco fora
de lexemas. Ver [`ast.md`](docs/ast.md#por-que-não-dá-para-comparar-byte-a-byte).

Revisar: o script oficial vai mostrar **16 aprovados / 34 reprovados**, e isso
precisa estar claro para quem corrigir — está explicado em
[`resultados-etapa2.md`](docs/resultados-etapa2.md#scripts-oficiais-da-disciplina),
mas talvez valha repetir no comentário da entrega.

### 2.3 Três pontos em que seguimos os casos oficiais, não a gramática do PDF

Comandos soltos no topo do arquivo, vetor em declaração global e `main` não
exigida pelo parser. Registrados em
[`gramatica.md`](docs/gramatica.md#divergências-entre-a-especificação-e-os-casos-oficiais).

### 2.4 Caso 41: nossa mensagem difere da pista do pacote

Em `a[1 = 2;` apontamos o `=` ("lado esquerdo da atribuição não é atribuível");
a pista do pacote esperava `FECHA_COLCHETE`. A gramática permite atribuição
dentro do índice, então o primeiro problema real é mesmo o alvo inválido. O
caso é rejeitado nas duas implementações, com posição exata.

---

## 3. Riscos de ambiente (não verificados aqui)

Tudo foi desenvolvido e testado em **Windows 11, Python 3.14, gcc MinGW-w64**.
A correção provavelmente acontece em Linux, e nada disso foi testado lá.

### 3.1 Rodar a entrega inteira em Linux

**Prioridade: alta** · **Não verificado**

Antes de entregar, alguém do grupo com Linux (ou WSL) deveria clonar do zero e
rodar:

```bash
python3 parser.py ref/testes-oficiais/testes-parser-50/casos/09_if_com_else/codigo.c
gcc -Wall -Wextra -std=c11 parser.c -o parser && ./parser <mesmo arquivo>
python3 tests/run_parser_tests.py
bash ref/scripts/testar_parser_python.sh ref/testes-oficiais/testes-parser-50 ./parser.py
bash ref/scripts/testar_parser_c.sh      ref/testes-oficiais/testes-parser-50 ./parser.c
```

Pontos específicos para conferir nesse teste:

- o bit de execução de `parser.py` sobreviveu ao clone (está gravado no git
  como `100755`; o script oficial exige `-x`);
- os finais de linha dos `.sh` vieram como LF (o `.gitattributes` força, mas
  vale confirmar);
- o `gcc` do Linux compila `parser.c` sem aviso (aqui compila limpo com
  `-Wall -Wextra`).

### 3.2 Versão do Python do professor

**Prioridade: média** · **Não verificado**

O código usa sintaxe de anotação `str | None`, que **exige Python 3.10+**. Em
3.9 ou anterior, o parser Python nem carrega. O README já declara "Python
3.10+", mas se houver risco de o professor usar 3.8/3.9, a correção é barata:
acrescentar `from __future__ import annotations` no topo de
`src/python/parser.py`, `minic_ast.py` e `tests/run_parser_tests.py`.

### 3.3 `parser.py` da raiz e o módulo `parser` da biblioteca padrão

**Prioridade: média** · **Não verificado**

O atalho da raiz faz `from parser import main` depois de pôr `src/python/` na
frente do `sys.path`. Em Python ≤ 3.9 existia um módulo embutido chamado
`parser`, e módulos embutidos têm prioridade sobre o `sys.path` — o import
poderia pegar o módulo errado. Em 3.10+ (o nosso caso) esse módulo não existe
mais, então hoje funciona.

Correção defensiva: carregar o arquivo por caminho com
`importlib.util.spec_from_file_location`, que não depende de nome de módulo.

### 3.4 Locale no script oficial

**Prioridade: baixa** · **Confirmado e contornado**

O `grep` do script do professor procura por "sintático"; em locale `C` o "á"
não casa e o contador "Erros sintáticos" aparece como **0** mesmo com os 25
casos sendo detectados. Os wrappers em `src/` já exportam `LC_ALL=C.UTF-8`, e o
README mostra a chamada direta com o locale.

Se o professor rodar sem isso e vier 0, é este efeito — vale mencionar na
entrega.

---

## 4. Material da disciplina em falta

### 4.1 Pacote de fixtures do scanner está incompleto

**Prioridade: baixa** (não afeta a entrega da etapa 2)

O `MANIFESTO.md` de
[`ref/testes-oficiais/testes-scanner-minic/`](ref/testes-oficiais/testes-scanner-minic/)
lista sete casos válidos (`v01`–`v07`) e um oitavo inválido (`i07`) que **não
vieram no download**; o `i06` veio sem o `.minic`. Para completar, é preciso
rebaixar o pacote no Classroom ou pedir ao professor.

### 4.2 Nosso scanner não produz o JSONL das fixtures oficiais

**Prioridade: baixa**

A etapa 1 foi entregue com a notação do material de aula (`<TOKEN, lexema>`), e
as fixtures oficiais usam JSONL com outros nomes de token. Se um dia for
preciso rodá-las, o caminho é um modo `--jsonl` no `main.py`/`main.c` com um
mapa de nomes — sem tocar nas regras léxicas. Tabela das diferenças em
[`ref/testes-oficiais/README.md`](ref/testes-oficiais/README.md#divergências-entre-estas-fixtures-e-o-nosso-scanner).

---

## 5. Antes de entregar (operacional)

- [ ] **Confirmar a data e a forma de entrega no Classroom.** O print que temos
      (17/09/2026) diz "Data de entrega: Amanhã, 23:59", ou seja 18/09/2026 —
      conferir se não mudou, e se a entrega é o link do repositório.
- [ ] **`git push`** — os commits das etapas estão só na máquina local.
- [ ] Conferir que o repositório do GitHub é público (ou que o professor tem
      acesso).
- [ ] Conferir nomes e RAs do grupo no [README](README.md) e em
      [`etapas.md`](docs/etapas.md).
- [ ] Cada integrante clonar do zero e rodar os comandos do
      [README](README.md) — se falhar para alguém, falha para o professor.
- [ ] Decidir se os itens 1.1–1.4 entram antes da entrega ou ficam para depois
      (nenhum deles afeta os 50 casos oficiais nem os 15 nossos).

---

## 6. Revisões de código sugeridas

Nada quebrado, mas vale um olhar:

- **`ast_free()` em [`src/c/ast.c`](src/c/ast.c) não é usada por ninguém.** O
  parser é dono dos nós e libera com `ast_free_node()` (raso). Manter a função
  recursiva como API pública ou remover? Se ficar, vale um teste que a exercite.
- **Cobertura de `Print`/`Read`/`For` na versão C**: os casos próprios cobrem,
  mas só com um exemplo de cada. Vale mais um caso com `for` aninhado e `read`
  em vetor multidimensional (que deve ser erro semântico, não sintático).
- **Nenhum teste roda o parser sobre os programas de referência da Seção 19 da
  especificação** (fatorial recursivo e entrada/vetor/repetição). São dois
  programas prontos no PDF — bons candidatos a casos próprios da etapa 3.
- **`tests/run_parser_tests.py` não tem um modo "só um caso"** (`--caso 24`),
  o que ajudaria a depurar sem rolar 50 linhas de saída.
