# minic-compiler

Analisador léxico da linguagem MINIC (etapa 1 do compilador). Implementação em
Python funcional; a versão em C ainda está pendente (ver [sugestao_roteiro.md](sugestao_roteiro.md)).

Especificação léxica em [docs/especificacao.md](docs/especificacao.md) e
[docs/tokens.md](docs/tokens.md).

## Estrutura do repositório

Cada pasta tem seu próprio README com mais detalhes:

- [src/](src/) — código-fonte ([src/python/](src/python/) implementado; C pendente)
- [tests/](tests/) — testes de regressão (entradas, saída esperada, runner)
- [docs/](docs/) — documentação derivada da especificação
- [ref/](ref/) — PDFs originais da disciplina (fonte normativa)

## Pré-requisitos

- Python 3.10+ (o projeto foi testado com Python 3.14)

## Passo a passo

### 1. Clonar e entrar no repositório

```bash
git clone <url-do-repositorio>
cd minic-compiler
```

### 2. Rodar o lexer pela linha de comando

Sem dependências externas — só a biblioteca padrão do Python.

```bash
python src/python/main.py tests/inputs/valido_soma.mc
```

- **stdout**: um token por linha, no formato `<TOKEN, lexema>` (tokens com
  atributo) ou só `TOKEN` (palavras reservadas e símbolos).
- **stderr**: erros léxicos, no formato `Erro léxico na linha L, coluna C: mensagem.`
- **código de saída**: `0` se não houve erro léxico, `2` se houve.

Troque `tests/inputs/valido_soma.mc` por qualquer outro arquivo `.mc` — os
exemplos prontos estão em [tests/inputs/](tests/inputs/), incluindo casos de
erro (`erro_*.mc`).

### 3. Rodar a suíte de testes

Compara a saída do lexer, para cada `.mc` em `tests/inputs/`, com o resultado
esperado salvo em `tests/expected/`.

```bash
python tests/run_tests.py
```

Se você alterar o lexer de propósito e precisar regravar os resultados
esperados:

```bash
python tests/run_tests.py --update
```

### 4. (Opcional) Rodar a interface Streamlit

Interface web para testar o lexer interativamente — escolher um exemplo,
colar código ou fazer upload de um `.mc`, e ver os tokens/erros numa tabela.

Instale a dependência (só precisa fazer uma vez, idealmente num
[ambiente virtual](https://docs.python.org/3/library/venv.html)):

```bash
pip install -r requirements.txt
```

Suba a interface:

```bash
python -m streamlit run src/python/app_streamlit.py
```

> No Windows, o comando `streamlit` direto pode não ser reconhecido porque o
> `pip` instala o executável numa pasta (`...\Python\Scripts`) que nem sempre
> está no `PATH`. `python -m streamlit` sempre funciona, pois não depende do
> `PATH`.

Isso abre automaticamente `http://localhost:8501` no navegador. Para parar,
`Ctrl+C` no terminal onde o comando está rodando.
