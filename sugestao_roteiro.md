# Roteiro — Entrega do Analisador Léxico (Etapa 1) do MINIC

> Sugestão de roteiro — ajuste a ordem, a estrutura ou as etapas conforme fizer mais sentido para o seu fluxo de trabalho.

Baseado na **ESPECIFICAÇÃO COMPLETA DA LINGUAGEM MINIC** (v1.0).

Requisito de entrega (enunciado da atividade):
> "A entrega do analisador léxico deverá ser feita mediante um repositório no Github contendo os a documentação, os códigos C, Python e os testes com os respectivos resultados."

O restante deste roteiro — estrutura sugerida, tabela de regex, checklist etc. — são adicionais e diferenciais, não exigências do enunciado.

---

## 1. Estrutura do repositório

```
minic-lexer/
├── README.md
├── docs/
│   ├── especificacao.md      (resumo da spec léxica: seções 3.x)
│   └── tokens.md              (tabela de tokens + regex)
├── src/
│   ├── python/
│   │   ├── lexer.py
│   │   └── main.py
│   └── c/
│       ├── lexer.c
│       ├── lexer.h
│       └── main.c
├── tests/
│   ├── inputs/                (arquivos .mc de entrada)
│   └── expected/              (saídas esperadas de tokens/erros)
└── Makefile (ou script de build)
```

## 2. Definir a especificação léxica formal antes de codar

Montar uma tabela com: categoria → regex → exemplo → ação do lexer (vira `docs/tokens.md`).
Categorias obrigatórias (spec, seção 3.4):

- Identificador, inteiro, real, caractere, cadeia
- Operadores aritméticos, relacionais e lógicos
- Atribuição, delimitadores
- Palavras reservadas (seção 3.3)
- Comentários `//` e `/* */` (sem aninhamento)
- Sequências de escape (seção 3.5): `\n`, `\t`, `\\`, `\'`, `\"`

## 3. Formato de token e tabela de regex (aulas 2–4)

O material de aula usa a notação `<TOKEN, lexema>` para tokens com atributo (ex: `<ID, position>`, `<INTEGER, 60>`) e apenas o nome do token para os que não precisam de atributo (ex: `ASSIGN`, `SEMICOLON`, `PLUS`). Vale padronizar a saída do lexer nesse formato.

Tabela de regex sugerida (apostila aula 4, seção 5):

| Token | Regex sugerida | Exemplos |
|---|---|---|
| IDENT | `[A-Za-z_][A-Za-z0-9_]*` | contador, _x, soma2 |
| INT | `[0-9]+` | 0, 42, 2026 |
| FLOAT | `[0-9]+\.[0-9]+` | 3.14, 0.5 |
| CHAR | `'([^\\']\|\\.)'` | 'a', '\n' |
| OP_REL | `(==\|!=\|<=\|>=\|<\|>)` | ==, <=, != |
| ESPACO | `[ \t\n\r]+` | ignorado |
| COMENTARIO | `//.*` ou `/\* ... \*/` | ignorado |

**Prioridade das regras** (importante): operadores de dois caracteres devem ser testados antes dos de um caractere — `==` antes de `=`, `<=` antes de `<` — senão o scanner pode gerar dois tokens em vez de um. Da mesma forma, palavras reservadas devem ser verificadas antes de classificar algo como identificador (ex: `if` não pode virar `IDENTIFIER`).

**Recuperação de erro — modo de pânico**: quando nenhum padrão reconhece o próximo prefixo da entrada, a estratégia mais simples é descartar caracteres até encontrar um token válido novamente, permitindo reportar mais de um erro por execução.

## 4. Implementar o scanner em Python

- Ler caractere a caractere, mantendo **linha e coluna** atualizadas
- Reconhecer palavras reservadas vs identificadores (tabela de keywords)
- Reconhecer números (diferenciar `int` de `float` pelo ponto decimal)
- Tratar strings e chars com escapes
- Ignorar espaços/tabs/quebras de linha e comentários
- Gerar erros léxicos com posição (símbolo não reconhecido, string não terminada — formato da seção 12)
- Saída: lista de tokens `(tipo, lexema, linha, coluna)`

Pode usar `re` para validar, mas recomenda-se implementação manual primeiro — ajuda a fixar a lógica antes de portar para C.

## 5. Implementar o mesmo scanner em C

Portar a mesma lógica do Python (mesmos tokens, mesmas mensagens de erro, mesmo formato de saída), para permitir comparação entre as duas implementações. Sugestão: função de estados (switch sobre o caractere atual) ou máquina de estados finita explícita.

## 6. Criar os casos de teste

Baseado na seção 13.2 (casos inválidos obrigatórios) e nos programas de referência (seção 19), além do exemplo de teste usado em aula (aula 2, slide 17 / aula 3, seção 10):

```
int main() {
  int total = 2 + 3 * 4;
  // mostra o resultado
  print(total);
  return 0;
}
```

- O `.mc` válido acima (ou fatorial/vetor-soma da spec)
- Casos de erro: símbolo não reconhecido, literal não terminado, char com tamanho inválido — a apostila da aula 3 dá exemplos prontos: `int x = 10 @ 2;` (símbolo não permitido), `print("MINIC);` (string não terminada), `int x = 3.14.5;` (literal malformado), `/* início…` (comentário sem fechamento)
- Salvar a saída esperada de cada input em `tests/expected/`
- Script (Python ou bash) que roda o lexer em C **e** em Python sobre cada input e compara com o esperado

## 7. Documentar

- **`docs/especificacao.md`**: resumo das regras léxicas seguidas (seção 3 do PDF)
- **README.md**: como rodar o lexer em Python, como compilar/rodar em C (`gcc`/`make`), como executar a suíte de testes, formato dos tokens, códigos de erro léxico (código `2`, seção 11.1)
- Tabela de cobertura: quais tokens/erros estão implementados

## 8. Checklist final antes de entregar

**Estrutura da entrega** (o que o enunciado pede explicitamente)
- [x] Repositório contém documentação (`docs/`)
- [x] Repositório contém código em Python (`src/python/`)
- [x] Repositório contém código em C (`src/c/`)
- [x] Repositório contém testes **com os respectivos resultados** (`tests/inputs/` + `tests/expected/*.{stdout,stderr,exit}.txt`, gerados rodando o lexer de verdade — não são só os inputs)

**Cobertura léxica** (spec, seções 3.3–3.5)
- [x] Reconhece todos os tokens obrigatórios: identificadores, inteiros, reais, char, string, operadores aritméticos/relacionais/lógicos, atribuição, delimitadores, palavras reservadas (cobertos em `tests/inputs/tokens_gerais.mc`)
- [x] Ignora corretamente espaços/tabs/quebras de linha
- [x] Ignora comentários de linha (`//`) e de bloco (`/* */`, sem aninhamento)
- [x] Trata sequências de escape em char/string (`\n`, `\t`, `\\`, `\'`, `\"`)
- [x] Mantém linha e coluna em todo token gerado
- [x] Operadores de dois caracteres têm prioridade sobre os de um caractere (`==` antes de `=`, `<=` antes de `<`)
- [x] Palavras reservadas têm prioridade sobre identificadores (ex: `if` não vira `IDENTIFIER`)
- [x] Saída dos tokens segue o formato `<TOKEN, lexema>` (ou equivalente documentado) — ver `docs/tokens.md`

**Tratamento de erros** (spec, seções 12 e 13.2)
- [x] Detecta símbolo não reconhecido (`erro_simbolo.mc`, `erro_multiplos.mc`)
- [x] Detecta literal (string ou char) não terminado (`erro_string_nao_terminada.mc`, `erro_multiplos.mc`)
- [x] Detecta char com tamanho inválido (ex: `'ab'`) (`erro_char_tamanho_invalido.mc`)
- [x] Diagnóstico de erro segue o formato: categoria, linha, coluna, lexema/token relevante (`Erro léxico na linha L, coluna C: mensagem.`)

**Equivalência entre implementações**
- [x] Python e C produzem os mesmos tokens e os mesmos erros para os mesmos inputs (verificado rodando `src/python/test_scanner_python.sh` e `src/c/test_scanner_c.sh` contra os mesmos `tests/expected/` — 9/9 em ambos)
- [x] Formato de saída (tokens e erros) é consistente entre as duas versões

**Testes**
- [x] Ao menos um programa `.mc` válido completo testado (ex: fatorial, ou vetor/soma — seção 19) (`valido_fatorial.mc`, `valido_soma.mc`, `valido_vetor_leitura.mc`)
- [x] Todos os casos inválidos da seção 13.2 cobertos
- [x] Script/processo que roda os testes automaticamente e compara com o resultado esperado (`tests/run_tests.py`, `src/python/test_scanner_python.sh`, `src/c/test_scanner_c.sh`)
- [x] Resultados reais da execução (não só o esperado) estão salvos ou registrados na documentação (`tests/expected/`)

**Documentação**
- [x] README explica como rodar o lexer em Python e em C, e como rodar a suíte de testes
- [x] `docs/especificacao.md` resume as regras léxicas seguidas
- [x] Tabela de cobertura indicando o que foi implementado (`docs/tokens.md` — tokens; `docs/especificacao.md` — erros)