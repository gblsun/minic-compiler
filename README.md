# minic-compiler

Analisador léxico da linguagem MINIC (etapa 1 do compilador), implementado em
Python **e** em C ([src/python/](src/python/) e [src/c/](src/c/)) — as duas
versões seguem a mesma especificação e produzem os mesmos tokens e os mesmos
erros para o mesmo arquivo `.mc` (ver [sugestao_roteiro.md](sugestao_roteiro.md)
para o roteiro original da entrega).

Especificação léxica em [docs/especificacao.md](docs/especificacao.md) e
[docs/tokens.md](docs/tokens.md).

## AP1 — analisador léxico

- **Disciplina**: Compiladores — 202602 CC 6A Manhã (Ciência da Computação, 6º semestre, 2026)
- **Entrega**: 22/08/2026, 23:59

**Grupo** (ordem alfabética):

| Nome | RA |
|---|---|
| Fellipe Augusto Silva Pereira | 2401525 |
| Gabriel Muchon Pavanelli | 2401895 |
| Paloma Eduarda Soares Leite | 2401660 |
| Victor Wenzel Martins Gonçalves | 2401698 |

## Estrutura do repositório

Cada pasta tem seu próprio README com mais detalhes:

- [src/](src/) — código-fonte ([src/python/](src/python/) e [src/c/](src/c/), ambos implementados)
- [tests/](tests/) — testes de regressão (entradas, saída esperada, runners)
- [docs/](docs/) — documentação derivada da especificação
- [ref/](ref/) — PDFs e scripts originais da disciplina (fonte normativa)

## Pré-requisitos

- Python 3.10+ (o projeto foi testado com Python 3.14) — para a versão Python e para `tests/run_tests.py`
- Um compilador C11 (ex.: `gcc`) — para a versão C

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

### 3. Rodar o lexer em C

Mesmo contrato de entrada/saída da versão Python (stdout/stderr/exit code
idênticos) — ver [src/c/README.md](src/c/README.md) para como o algoritmo
foi portado.

```bash
gcc -Wall -Wextra -std=c11 src/c/lexer.c src/c/main.c -o src/c/scanner
src/c/scanner tests/inputs/valido_soma.mc
```

### 4. Rodar a suíte de testes

Três scripts comparam a saída do lexer, para cada `.mc` em `tests/inputs/`,
com o resultado esperado salvo em `tests/expected/` — ver
[tests/README.md](tests/README.md) para o papel de cada um:

```bash
python tests/run_tests.py                 # runner "dono" dos golden files (Python)
bash src/python/test_scanner_python.sh    # mesma suíte, formato pedido pela disciplina (Python)
bash src/c/test_scanner_c.sh              # mesma suíte, compilando e testando a versão em C
```

Se você alterar o lexer (Python **ou** C) de propósito e precisar regravar os
resultados esperados — sempre a partir da versão Python, que é a autoridade
dos golden files:

```bash
python tests/run_tests.py --update
```

### 5. (Opcional) Rodar a interface Streamlit

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
