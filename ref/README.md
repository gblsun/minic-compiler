# ref/

Material de referência da disciplina — PDFs e scripts originais, não
editados. Em caso de dúvida ou divergência com os resumos em
[docs/](../docs/), estes arquivos são a fonte normativa.

| Arquivo | Conteúdo |
|---|---|
| [especificacao-completa-minic.pdf](especificacao-completa-minic.pdf) | Especificação completa da linguagem MINIC (v1.0) — a fonte usada para `docs/especificacao.md` e `docs/tokens.md`. |
| [Aula01 - Compiladores (1).pdf](Aula01%20-%20Compiladores%20(1).pdf) | Slides — introdução a compiladores. |
| [Aula02 - Compiladores.pdf](Aula02%20-%20Compiladores.pdf) | Slides — arquitetura de compiladores. |
| [Aula03 - Compiladores.pdf](Aula03%20-%20Compiladores.pdf) | Slides — análise léxica. |
| [Aula04 - Compiladores.pdf](Aula04%20-%20Compiladores.pdf) | Slides — expressões regulares e autômatos finitos (mesmo tema da apostila abaixo). |
| [Aula05 - Compiladores.pdf](Aula05%20-%20Compiladores.pdf) | Slides — construção do analisador léxico (maximal munch, leitura em blocos, casos de erro); base teórica direta de `src/python/lexer.py` e `src/c/lexer.c`. |
| [apostila-aula-2-arquitetura-compiladores.pdf](apostila-aula-2-arquitetura-compiladores.pdf) | Apostila — arquitetura de compiladores. |
| [apostila-aula-3-analise-lexica.pdf](apostila-aula-3-analise-lexica.pdf) | Apostila — análise léxica. |
| [apostila-aula-4-expressoes-regulares-automatos-finitos.pdf](apostila-aula-4-expressoes-regulares-automatos-finitos.pdf) | Apostila — expressões regulares e autômatos finitos (base teórica do lexer). |
| [test_scanner_python.sh](test_scanner_python.sh) | Script de teste do scanner Python fornecido pela disciplina, no formato original (saída JSONL, layout de pastas plano). Versão adaptada à estrutura real deste projeto em [`../src/python/test_scanner_python.sh`](../src/python/test_scanner_python.sh) — ver [`../tests/README.md`](../tests/README.md). |
| [test_scanner_c.sh](test_scanner_c.sh) | Script de teste do scanner C fornecido pela disciplina, no formato original. Versão adaptada em [`../src/c/test_scanner_c.sh`](../src/c/test_scanner_c.sh). |
