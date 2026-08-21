# Possíveis features

> Lista de diferenciais observados em outros repositórios de analisador léxico do MINIC
> (mesma disciplina). Nada aqui é exigência do enunciado — a entrega exige apenas
> documentação, código em C, código em Python e testes com os respectivos resultados.
> Isto é só um backlog de ideias para avaliar se vale a pena incorporar.

## Observado no Repositório A

- [ ] Gramática formal em EBNF (`docs/gramatica.ebnf`)
- [ ] Documento de arquitetura separado da especificação léxica (`docs/arquitetura.md`)
- [x] Interface gráfica opcional além do modo terminal — implementada com Streamlit
      em vez de Tkinter (`src/python/app_streamlit.py`, ver `requirements.txt`)
- [ ] CLI com flags para controlar a saída (ex.: `--tokens`, `--errors`, `--jsonl`)
- [ ] Saída dos tokens/erros serializada em JSONL, com módulo dedicado de serialização
- [ ] Código Python modularizado em arquivos por responsabilidade (scanner, tokens,
      tipos de token, erros, resultado da análise) em vez de um único `lexer.py`
- [ ] Runner de testes dedicado (equivalente a um `fixture_runner.py`) e demos de código
      embutidas no próprio programa
- [ ] Mais programas de teste válidos e mais categorias de erro léxico cobertas
- [ ] Estrutura de pastas já preparada para as próximas etapas do compilador
      (parser, semântico, IR, otimizador, geração de código)

## Observado no Repositório B

- [ ] Tabela declarativa de tokens (lista de `(nome, regex)` combinada numa única regex
      via `"|".join(...)`) em vez de reconhecimento manual token a token
- [ ] Streaming de tokens em JSON linha a linha (um `json.dumps` por token), incluindo
      um token `EOF` explícito ao final da análise
- [ ] Atributo tipado por token (ex.: converter o lexema para `int`/`float`/string sem
      aspas), separado do lexema bruto no registro do token

## Ideias próprias (não vistas nos outros repositórios)

- [ ] Lançador único na raiz do repo que pergunta "Deseja rodar em C ou Python?"
      (ou aceita uma flag tipo `--lang c|python`) e chama a implementação escolhida
      sobre o mesmo arquivo `.mc`, sem precisar lembrar o comando de cada versão
- [ ] Modo `--compare` nesse mesmo lançador: roda Python e C sobre o mesmo arquivo
      e verifica se a saída (tokens, erros, exit code) bate — dá pra usar isso pra
      provar na prática o item do checklist "Python e C produzem os mesmos tokens
      e os mesmos erros para os mesmos inputs"

## Observação

O Repositório B não cobre alguns requisitos obrigatórios do enunciado (não tem
implementação em C, não tem pasta de testes nem resultados salvos, e a documentação
é só um parágrafo no README, sem especificação léxica formal) — não usar como
referência de "o que é suficiente para entregar", só como fonte de ideias de
implementação. Já o Repositório A cobre os quatro requisitos (docs, C, Python e
testes com resultado), então serve melhor como referência de estrutura completa.
