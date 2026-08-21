# tests/

Testes de regressão do analisador léxico, no formato "golden file" (a saída
esperada fica gravada em disco e é comparada byte a byte com a saída atual).

```
tests/
├── inputs/     arquivos .mc de entrada
├── expected/   saída esperada para cada entrada (stdout, stderr, exit code)
└── run_tests.py
```

## Estrutura

Para cada `tests/inputs/<nome>.mc` existem três arquivos correspondentes em
`tests/expected/`:

- `<nome>.stdout.txt` — tokens que o lexer deve imprimir
- `<nome>.stderr.txt` — erros léxicos esperados (vazio se não houver nenhum)
- `<nome>.exit.txt` — código de saída esperado do processo (`0` ou `2`)

Os nomes seguem a convenção `valido_*.mc` para entradas sem erro léxico e
`erro_*.mc` para entradas que devem disparar erro — só para facilitar achar
um caso específico rapidamente, não é algo que o runner interprete.

## Rodando

```bash
python tests/run_tests.py
```

Compara a saída atual do lexer (rodando `src/python/main.py` de verdade,
como um usuário rodaria) com o que está gravado em `expected/`.

## Adicionando um teste novo

1. Crie `tests/inputs/<nome>.mc` com o caso que você quer cobrir.
2. Rode `python src/python/main.py tests/inputs/<nome>.mc` e confira **à
   mão** que a saída (tokens e/ou erros) está correta.
3. Só então grave o resultado como esperado:

   ```bash
   python tests/run_tests.py --update
   ```

   `--update` regrava a saída esperada para **todos** os arquivos em
   `inputs/`, não só o novo — revise o `git diff` de `tests/expected/` antes
   de commitar, para não acabar congelando uma regressão sem querer.
