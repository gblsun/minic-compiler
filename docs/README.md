# docs/

Documentação de apoio para implementar e revisar o compilador MINIC. Tudo aqui
é **derivado** do material da disciplina em [ref/](../ref/) — em caso de
dúvida ou divergência, o material original é a fonte normativa; estes arquivos
existem para não precisar reabrir os PDFs toda hora.

## Por onde começar

- Quer entender **onde o projeto está**? → [etapas.md](etapas.md)
- Quer entender **como as partes se encaixam**? → [arquitetura.md](arquitetura.md)
- Vai mexer no **scanner**? → [especificacao.md](especificacao.md) e [tokens.md](tokens.md)
- Vai mexer no **parser**? → [gramatica.md](gramatica.md), [ast.md](ast.md) e [roteiro-etapa2-parser.md](roteiro-etapa2-parser.md)
- Quer avaliar a **AP 2** (analisador semântico)? → [entrega-ap2.md](entrega-ap2.md)
- Vai mexer na **análise semântica**? → [semantica.md](semantica.md) e [testes-semanticos.md](testes-semanticos.md)
- Vai mexer na **IR ou no otimizador** (`--ir`, `-O1`)? → [ir.md](ir.md)
- Quer avaliar os **testes da IR** (atividade "Testes para o MINIC (IR)")? → [entrega-ir.md](entrega-ir.md)
- Quer os **resultados dos testes**? → [resultados-etapa2.md](resultados-etapa2.md) e [resultados-etapa3.md](resultados-etapa3.md)
- Quer saber **o que mudou depois da avaliação da etapa 2**? → [correcoes-etapa2.md](correcoes-etapa2.md)
- Quer os **exercícios da aula 13** (tradução dirigida por sintaxe)? → [exercicios/aula13/](../exercicios/aula13/)
- Quer saber **o que ainda está errado ou em aberto**? → [PENDENCIAS.md](../PENDENCIAS.md)

## Visão geral e planejamento

| Arquivo | O que é |
|---|---|
| [etapas.md](etapas.md) | As quatro etapas do projeto, o status de cada uma, prazos, comandos exigidos por cada atividade e integrantes do grupo. |
| [arquitetura.md](arquitetura.md) | Pipeline (scanner → parser → semântica → IR → código), contrato entre as fases, CLI, códigos de saída e formato dos diagnósticos. Resume as Seções 1, 11 e 12 da especificação. |

## Referência da linguagem

| Arquivo | O que é |
|---|---|
| [especificacao.md](especificacao.md) | Resumo operacional da especificação **léxica** (Seção 3, mais os pontos das Seções 11.1, 12 e 13.2 relevantes para o lexer). |
| [tokens.md](tokens.md) | Tabela de tokens da implementação (categoria → padrão → exemplo), com o formato de saída (`<TOKEN, lexema>` / `TOKEN`) documentado. |
| [gramatica.md](gramatica.md) | Gramática **sintática** em EBNF (Seção 4), precedência e associatividade, os não terminais que a especificação deixa indefinidos, e as decisões de implementação (recursão à esquerda, `else` pendente, alvo de atribuição). |
| [semantica.md](semantica.md) | **Análise semântica** (etapa 3): formato de saída, as duas passagens, decisões de projeto (escopos, conversões, `main`, destino de atribuição), sistema de tipos, retorno em todos os caminhos e o catálogo completo de diagnósticos SEM001–SEM016. |
| [ir.md](ir.md) | **Representação intermediária e otimização**: formato da IR de três endereços, decisões de tradução, os cinco passes do `-O1` com os cuidados de corretude, testes (30/30 oficiais, equivalência C × Python, -O0 × -O1) e a análise dos scripts oficiais do pacote de IR. |
| [ast.md](ast.md) | Contrato dos nós da AST, a notação exata que os testes oficiais esperam, o formato de impressão canônico e as divergências entre as três fontes (especificação, apostila 12, pacote de testes). |

## Roteiros de entrega

| Arquivo | O que é |
|---|---|
| [roteiro-etapa1-lexer.md](roteiro-etapa1-lexer.md) | Roteiro e checklist usados na **etapa 1** (analisador léxico), já concluída — mantido como registro do que foi entregue. |
| [roteiro-etapa2-parser.md](roteiro-etapa2-parser.md) | Plano de execução da **etapa 2** (parser e AST), já executado: decisões tomadas, ordem de implementação, tratamento de erros, pontos de entrada exigidos, testes e checklist de conformidade marcado. |
| [resultados-etapa2.md](resultados-etapa2.md) | Registro da execução das suítes da etapa 2 (201/201), como ler o resumo dos scripts oficiais do professor e a tabela dos diagnósticos emitidos nos 25 casos de rejeição. |
| [correcoes-etapa2.md](correcoes-etapa2.md) | Resposta à avaliação da etapa 2: cada apontamento, a causa encontrada, a correção e como conferir. |
| [entrega-ap2.md](entrega-ap2.md) | **Página de entrega da AP 2**: como rodar, e cada critério de correção (desacoplamento, equivalência C × Python, lógica, casos de teste) com a evidência correspondente. |
| [entrega-ir.md](entrega-ir.md) | **Página de entrega dos testes da IR**: como rodar, resultado por grupo do pacote (30/30 em Python e em C), por que os scripts do pacote não servem de critério, e o que foi entregue. |
| [testes-semanticos.md](testes-semanticos.md) | Catálogo dos testes da etapa 3: os 20 oficiais e os 19 próprios, com o objetivo, o código e a saída esperada de cada um (gerado por `tests/gerar_catalogo_semantico.py`). |
| [resultados-etapa3.md](resultados-etapa3.md) | Registro da execução das suítes da etapa 3 (184/184), do teste diferencial (900 programas gerados), dos sanitizers e dos scripts oficiais da AP 2 (20/20 em Python e em C). |
| [PENDENCIAS.md](../PENDENCIAS.md) | O que precisa ser corrigido, revisado ou decidido antes da entrega: bugs confirmados (com a reprodução anotada), decisões que pedem uma segunda opinião do grupo, riscos de ambiente ainda não testados e o checklist operacional da entrega. |
| [possiveis_features.md](possiveis_features.md) | Backlog de ideias/diferenciais observados em outros projetos da mesma disciplina. Nada ali é exigência do enunciado — é uma lista para avaliar o que vale incorporar. |

A especificação completa (fonte normativa), os slides, as apostilas, os
enunciados e os pacotes de teste oficiais estão em [ref/](../ref/).
