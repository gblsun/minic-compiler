# Análise semântica (etapa 3)

O analisador semântico recebe a AST de um programa **sintaticamente válido**,
liga cada uso de nome à sua declaração, sintetiza o tipo de cada expressão e
confere os contextos (condição, laço, retorno, atribuibilidade). O resultado é
"programa aceito" ou a lista de diagnósticos.

As fontes normativas, em ordem de prioridade:

1. o **gabarito da AP 2**
   ([`GABARITO.md`](../ref/testes-oficiais/testes-semanticos-20/GABARITO.md) e
   os 20 `.gabarito`), que fixa o texto exato das mensagens SEM001–SEM013;
2. a **apostila da aula 15**
   ([PDF](../ref/apostilas/apostila-aula-15-analise-semantica.pdf)), com as
   regras de tipos, compatibilidade, retornos e o catálogo de códigos;
3. a **especificação** ([PDF](../ref/especificacao-completa-minic.pdf), Seções 5
   a 7 e 11.1), para o que as duas anteriores não cobrem (`char`, divisão por
   zero, limites de vetor, código de saída 4);
4. a **apostila da aula 14**
   ([PDF](../ref/apostilas/apostila-aula-14-tabela-de-simbolos-e-escopos.pdf)),
   para a tabela de símbolos e os escopos.

Implementação: [`src/python/semantico.py`](../src/python/semantico.py) e
[`src/python/tabela_simbolos.py`](../src/python/tabela_simbolos.py);
em C, [`src/c/semantico.c`](../src/c/semantico.c) e
[`src/c/tabela_simbolos.c`](../src/c/tabela_simbolos.c). As duas versões têm a
mesma estrutura, as mesmas mensagens e a mesma ordem de visita, e a suíte de
testes confere que a saída é idêntica byte a byte.

## Como rodar

```
python minic.py codigo.c                 # Python
gcc -Wall -Wextra minic.c -o minic       # C (unidade única, como o script oficial compila)
./minic codigo.c
```

Saída de um programa com erros (caso oficial 17):

```
SEM008 — linha 5, coluna 20: Argumento 1 de “combina”: esperado int, recebido bool (expressão “1 < 2”).
SEM008 — linha 5, coluna 27: Argumento 2 de “combina”: esperado float, recebido bool (expressão “3 < 4”).
Análise semântica concluída: 2 erros; programa rejeitado.
```

| Situação | stdout | stderr | código |
|---|---|---|---|
| programa aceito | `Análise semântica concluída: 0 erros; programa aceito.` | — | 0 |
| erros semânticos | um diagnóstico por linha + resumo | — | 4 |
| erro léxico | — | mensagens do lexer | 2 |
| erro sintático | — | mensagens do parser | 3 |
| uso inválido | — | `uso: …` | 1 |

**Formato exato.** As linhas do relatório são separadas por **CRLF** e **não**
há quebra de linha depois da última. Não é uma preferência estética: é o que
está dentro dos arquivos `.gabarito` oficiais, e o script
`testes_semanticos_*.sh` compara com `diff` sem normalizar nada. Por isso:

- o relatório inteiro vai para **stdout**, num bloco só. O script junta stdout e
  stderr (`2>&1`), e dividir a saída entre os dois fluxos tornaria a ordem
  dependente de buffer;
- o Python escreve bytes direto em `sys.stdout.buffer`, e o C põe stdout e
  stderr em modo binário no Windows. Sem isso, o modo texto do Windows
  trocaria cada `\n` por `\r\n` mais uma vez;
- os `.gabarito` e os golden próprios estão marcados como `-text` no
  [`.gitattributes`](../.gitattributes), para o git não converter fim de linha.

Uma fase só roda se a anterior terminou sem erro. Com erro léxico ou
sintático, a AST não existe (ou está incompleta), e a análise semântica só
produziria diagnósticos falsos.

## Organização: duas passagens

Seguindo a seção 2 da apostila da aula 15:

- **Passagem A — coleta.** Todas as funções (com as assinaturas) e todas as
  variáveis globais entram no escopo global, em ordem de código, antes de
  qualquer corpo ser visitado. É aqui que a duplicidade global é detectada, e
  a mensagem cita a declaração que veio **antes** no arquivo.
- **Passagem B — verificação.** Declarações, corpos e comandos são visitados
  na ordem do código. Cada bloco abre e fecha um escopo, cada expressão recebe
  um tipo e cada comando tem o seu contexto conferido. No fim de cada função
  não `void`, uma análise de fluxo decide se o corpo pode terminar sem
  `return`.

Os diagnósticos são acumulados e, no fim, **ordenados por posição** (linha,
coluna). No empate, prevalece a ordem em que foram encontrados (ordenação
estável).

## Decisões de projeto

| Tema | Decisão | Por quê |
|---|---|---|
| Funções antes da definição | Visíveis no programa inteiro (passagem A) | Recursão direta e mútua; o gabarito supõe "coleta de assinaturas para recursão". A especificação permite, desde que documentado. |
| Variáveis globais | Visíveis só **depois** da declaração | "Declarar antes de usar" (Seção 2 da especificação). Uma global usada antes da declaração é SEM001. |
| Sombreamento | Permitido em bloco interno; busca do mais interno para o mais externo | Caso oficial 02 e Seção 6 da especificação. |
| Parâmetros e corpo | Mesmo escopo | Redeclarar um parâmetro no nível de fora do corpo é duplicidade, como em C. |
| Duplicidade | Reporta e **mantém a primeira** declaração | O erro não muda o significado dos usos seguintes (evita cascata). |
| Inicializador | O nome já está no escopo quando o inicializador é verificado | Como em C (`int x = x;` usa o próprio `x`). |
| Função `main` | **Não** é exigida | Os 10 programas válidos da AP 2 não têm `main` (usam `principal`), e o gabarito os aceita. A Seção 2 da especificação pede `main`; prevaleceu o teste oficial. |
| Conversões implícitas | `int → float` e `char → int`; nenhuma outra | Apostila (seção 4) e especificação (Seção 5). Não há `char → float`, nem nada entre `bool` e os numéricos. |
| `char` em aritmética | Tratado como numérico e promovido a `int` | Consequência de `char → int` (Seção 5). `'a' + 1` é `int`. |
| Atribuição como expressão | Tem o tipo do destino | O caso oficial 09 exige `a = b = 3`. |
| Destino de atribuição | O parser aceita qualquer expressão (só no pipeline semântico) e a semântica reporta SEM013 | O caso oficial 20 (`3 = n;`) espera SEM013 na coluna do `3`, não um erro de sintaxe. A apostila prevê isso ("o parser pode aceitar a forma geral"). O `parser.py`/`parser.c` da etapa 2 continua rejeitando, como antes. |
| `while`/`for` e retorno | Laço nunca garante retorno | Regra conservadora da apostila (seção 10): o laço pode executar zero vezes. |
| Comandos soltos no topo | Verificados no escopo global, fora de função | O parser os aceita por causa do caso oficial 22 da etapa 2; `return` ali é SEM010. |
| Parênteses nas mensagens | Recolocados só onde a precedência exige | A AST não guarda parênteses. `(a + b) * c` sai igual, mas `(x)` sai `x`. |

### Posição de cada diagnóstico

Colunas começam em 1 e contam **caracteres**, não bytes: um acento dentro de
uma cadeia ou de um comentário conta como uma coluna nas duas
implementações.

- Diagnóstico sobre **uma expressão inteira** (SEM003, SEM005, SEM006, SEM008,
  SEM009, SEM012, SEM013, SEM015, SEM016): aponta o **início** da expressão, ou
  seja, o seu primeiro token. Em `combina(1 < 2, …)`, o argumento 1 é apontado
  na coluna do `1`, e não na do `<`. Como a AST não guarda parênteses,
  `(a + b)` começa no `a`.
- Diagnóstico sobre um **operador** (SEM004): aponta o operador.
- Diagnóstico sobre um **nome** (SEM001, SEM002, SEM014): aponta o nome. Na
  duplicidade de parâmetro e de função, aponta o tipo, que é onde a AST
  posiciona esses nós.
- Diagnóstico sobre um **comando** (SEM009 sem valor, SEM010): aponta a
  palavra-chave. SEM011 aponta o início da definição da função.

## Sistema de tipos

| Construção | Exige | Resultado |
|---|---|---|
| literal | — | `int`, `float` (lexema real), `bool`, `char` ou `string` |
| identificador | declarado e visível; não pode ser função fora de chamada | tipo do símbolo (`vetor de T` se for vetor) |
| `v[i]` | `v` vetor; `i` compatível com `int` | `T`, o tipo do elemento |
| `f(a₁…aₙ)` | `f` função; mesma aridade; cada `aᵢ` compatível com o parâmetro | tipo de retorno (`void` só vale como comando) |
| `-x` | numérico | `float` se `x` é `float`; senão `int` |
| `!x` | `bool` | `bool` |
| `+ - * /` | dois numéricos | `float` se algum é `float`; senão `int` |
| `%` | dois inteiros (`int`/`char`) | `int` |
| `< <= > >=` | dois numéricos | `bool` |
| `== !=` | dois numéricos, ou dois `bool` | `bool` |
| `&& \|\|` | dois `bool` | `bool` |
| `alvo = valor` | alvo atribuível; valor compatível com o alvo | tipo do alvo |

**Compatibilidade** (`valor` usado onde se espera `esperado`; vale para
atribuição, inicialização, argumento, retorno e índice): tipos iguais, mais
`int → float` e `char → int`. Vetor nunca é compatível com nada. A exceção é
o argumento passado a um parâmetro `T v[]`, que aceita qualquer vetor de `T`,
de qualquer tamanho.

**Supressão de cascata.** Uma expressão que já gerou diagnóstico recebe o
tipo sentinela `ERRO`, e quem o recebe não reclama de novo. Exemplo:
`n = (ausente + 1) * true;` gera **só** o SEM001 de `ausente`. O SEM004 do `*`
e o SEM003 da atribuição seriam consequências do primeiro erro. Partes
independentes continuam sendo verificadas: em `n = ausente + 1 * outro;` saem
os dois SEM001.

## Retorno em todos os caminhos (SEM011)

Um comando **pode terminar normalmente** (seguir para o próximo) conforme a
tabela da seção 10 da apostila:

| Comando | Termina normalmente? |
|---|---|
| `return`, `break`, `continue` | não |
| `if (c) S` sem `else` | sim, pelo ramo ausente |
| `if (c) S1 else S2` | se S1 **ou** S2 termina |
| bloco `{ S1; …; Sn }` | se **todos** terminam |
| `while`, `for` | sim, porque pode executar zero vezes |
| demais (expressão, declaração, `print`, `read`) | sim |

Se o corpo de uma função não `void` pode terminar normalmente, sai o SEM011, e
a mensagem diz por onde o controle chega ao fim:

| Último comando do corpo | Explicação na mensagem |
|---|---|
| `if (c)` sem `else` | o ramo em que “c” é falso alcança o fim do corpo |
| `if (c) … else …` em que o `então` termina | o ramo em que “c” é verdadeiro alcança o fim do corpo |
| `if (c) … else …` em que só o `senão` termina | o ramo em que “c” é falso alcança o fim do corpo |
| `while` / `for` | o laço while/for da linha L pode terminar e alcançar o fim do corpo |
| bloco aninhado | a explicação do último comando dele |
| corpo vazio ou outro comando | o fim do corpo é alcançado sem return |

## Catálogo de diagnósticos

Os códigos SEM001–SEM013 são os do gabarito. O texto das mensagens que
aparecem nos 20 casos oficiais é **exatamente** o do gabarito, e as variantes
seguem o mesmo molde. SEM014–SEM016 são extensões nossas para regras da
apostila e da especificação que o catálogo não numera; o gabarito diz que o
SEM013 "nomeia explicitamente" uma regra nova, e seguimos a mesma convenção.

| Código | Regra | Mensagem (`…` = partes variáveis) |
|---|---|---|
| SEM001 | identificador não declarado | `Identificador “x” não declarado neste escopo.` |
| SEM002 | declaração duplicada no mesmo escopo | `“x” já declarado neste escopo; declaração anterior na linha L, coluna C (tipo int).` (ou `vetor de int`, ou `função que retorna int`) |
| SEM003 | atribuição/inicialização incompatível | `Não é possível atribuir float a int sem conversão permitida (destino “n”; expressão “2.5”).` |
| SEM004 | operador com operandos inválidos | `Operador * requer operandos numéricos; recebeu bool e int.` · `Operador % requer operandos int; …` · `Operador == requer operandos de tipos compatíveis; …` · `Operador && requer operandos bool; …` · `Operador - requer operando numérico; recebeu bool.` · `Operador ! requer operando bool; recebeu int.` |
| SEM005 | condição não `bool` | `Condição de while deve ter tipo bool; recebeu int (expressão “n”).` (também `if` e `for`) |
| SEM006 | índice não inteiro | `Índice do vetor “dados” deve ser int; recebeu float (expressão “1.0”).` |
| SEM007 | aridade | `“f” espera 2 argumentos, mas recebeu 1.` |
| SEM008 | tipo de argumento | `Argumento 1 de “f”: esperado int, recebido bool (expressão “1 < 2”).` |
| SEM009 | retorno incompatível | `Retorno float incompatível com o tipo int da função “f”; conversão implícita de float para int não permitida.` · `Retorno sem valor na função “f”, que deve retornar int.` · `A função “g” é void e não pode retornar valor (expressão “3”).` |
| SEM010 | contexto | `break só pode ocorrer dentro de um laço.` · `continue só …` · `return só pode ocorrer dentro de uma função.` |
| SEM011 | função não `void` pode cair pelo fim | `A função “f” pode terminar sem retornar int; <explicação>.` (ver a seção anterior) |
| SEM012 | chamada `void` usada como valor | `Função “acao” não produz valor (retorno void) e não pode ser usada como expressão de atribuição.` (ou `inicializador`, `argumento`, `operando`, `condição`, `índice`, `valor de retorno`, `argumento de print`) |
| SEM013 | destino não atribuível | `Destino de atribuição não é atribuível; o literal inteiro “3” não designa uma variável ou elemento de vetor.` (ou `o literal real/lógico/de caractere`, `a cadeia`, `a chamada “f()”`, `a expressão “n + 1”`, `a função “f”`) · `Destino de atribuição não é atribuível; “v” é um vetor, e só seus elementos (“v[i]”) podem receber valores.` · em `read`: `Destino de leitura …` |
| SEM014 | categoria incorreta | `“x” não designa uma função; foi declarado como variável na linha L, coluna C.` · `“x” não é um vetor e não pode ser indexado; foi declarado como …` · `A função “f” só pode ser usada em chamada.` · `print não aceita o vetor “v” inteiro; imprima um elemento por vez.` |
| SEM015 | divisor constante zero (Seção 7 da especificação) | `Divisão por zero: o divisor é a constante “0”.` · `Resto de divisão por zero: …` |
| SEM016 | vetor: tamanho e limites constantes | `Tamanho do vetor “v” deve ser um literal inteiro positivo; recebeu “0”.` · `Índice “3” fora dos limites do vetor “v” (tamanho 3).` |

O limite de índice só é verificado quando o índice é um literal inteiro (com
sinal opcional) e o tamanho do vetor é conhecido. Isso deixa de fora
parâmetros `T v[]` e índices calculados, que são verificação de execução
(seção 7 da apostila).

## Testes

```
python tests/run_semantic_tests.py            # 20 oficiais + 16 próprios em cada versão + equivalência
bash ref/scripts/testes_semanticos_py.sh ./minic.py ref/testes-oficiais/testes-semanticos-20
bash ref/scripts/testes_semanticos_c.sh  ./minic.c  ref/testes-oficiais/testes-semanticos-20
```

O runner compara byte a byte e tem três suítes: os **20 casos oficiais** da
AP 2, os **16 casos próprios** de [`tests/semantic_inputs/`](../tests/semantic_inputs/),
com golden em [`tests/semantic_expected/`](../tests/semantic_expected/), e a
**equivalência** C × Python. A equivalência roda, além das entradas das duas
primeiras suítes, os 50 casos oficiais do parser e os 17 casos próprios da
etapa 2 pelo pipeline completo: 103 programas ao todo. Os casos próprios
cobrem cada código do catálogo, inclusive os que os oficiais não exercitam
(SEM004, SEM010, SEM014–SEM016), além de `for`, `print`, `read`, `char`,
supressão de cascata, colunas depois de caracteres acentuados e os erros
léxico e sintático antes da semântica.

Resultados registrados em [resultados-etapa3.md](resultados-etapa3.md).
