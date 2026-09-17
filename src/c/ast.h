/* Árvore sintática abstrata (AST) da linguagem MINIC.
 *
 * Transcrição para C de src/python/minic_ast.py, mantendo os mesmos nós, a
 * mesma ordem de filhos e as mesmas duas impressões (compacta e indentada),
 * para que as duas implementações produzam saída idêntica.
 *
 * A diferença estrutural em relação ao Python é só de representação: lá cada
 * nó é uma @dataclass com campos nomeados (`If.cond`, `If.then`, ...); aqui
 * todos os nós são a mesma struct `Ast`, e o significado de cada campo depende
 * do `kind` — a tabela abaixo é o contrato. É a mesma troca já feita no lexer
 * (dict do Python -> array de structs em C): sem classes, o jeito direto de ter
 * "vários tipos de nó" é um enum e uma struct comum.
 *
 *   kind          text            type_name        a        b       c     d    items
 *   ------------- --------------- ---------------- -------- ------- ----- ---- ---------
 *   AST_PROGRAM   -               -                -        -       -     -    itens de topo
 *   AST_VAR_DECL  nome            tipo             init     size    -     -    -
 *   AST_PARAM     nome            tipo             -        -       -     -    -
 *   AST_FUNCTION  nome            tipo de retorno  corpo    -       -     -    parâmetros
 *   AST_BLOCK     -               -                -        -       -     -    itens do bloco
 *   AST_EXPR_STMT -               -                expressão-       -     -    -
 *   AST_IF        -               -                cond     então   senão -    -
 *   AST_WHILE     -               -                cond     corpo   -     -    -
 *   AST_FOR       -               -                init     cond    passo corpo-
 *   AST_RETURN    -               -                expressão-       -     -    -
 *   AST_BREAK     -               -                -        -       -     -    -
 *   AST_CONTINUE  -               -                -        -       -     -    -
 *   AST_PRINT     -               -                argumento-       -     -    -
 *   AST_READ      -               -                alvo     -       -     -    -
 *   AST_ASSIGN    -               -                alvo     valor   -     -    -
 *   AST_BINARY    operador        -                esquerda direita -     -    -
 *   AST_UNARY     operador        -                operando -       -     -    -
 *   AST_CALL      -               -                callee   -       -     -    argumentos
 *   AST_INDEX     -               -                base     índice  -     -    -
 *   AST_ID        nome            -                -        -       -     -    -
 *   AST_LIT       lexema          categoria        -        -       -     -    -
 */

#ifndef AST_H
#define AST_H

#include <stddef.h>

typedef enum {
    AST_PROGRAM,
    AST_VAR_DECL,
    AST_PARAM,
    AST_FUNCTION,
    AST_BLOCK,
    AST_EXPR_STMT,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_RETURN,
    AST_BREAK,
    AST_CONTINUE,
    AST_PRINT,
    AST_READ,
    AST_ASSIGN,
    AST_BINARY,
    AST_UNARY,
    AST_CALL,
    AST_INDEX,
    AST_ID,
    AST_LIT
} AstKind;

typedef struct Ast Ast;

struct Ast {
    AstKind kind;
    int line;
    int column;

    char *text;      /* nome, operador ou lexema — o nó é dono da string */
    char *type_name; /* tipo declarado / categoria do literal — idem */
    int is_array;    /* AST_PARAM: parâmetro declarado como `tipo id[]` */

    Ast *a; /* filhos de aridade fixa, na ordem da tabela do cabeçalho */
    Ast *b;
    Ast *c;
    Ast *d;

    Ast **items; /* filhos de aridade variável (listas) */
    size_t items_len;
    size_t items_cap;
};

/* Cria um nó do tipo indicado, com posição de origem e tudo mais zerado. */
Ast *ast_new(AstKind kind, int line, int column);

/* Acrescenta um filho à lista `items` de `parent` (dobrando a capacidade
 * quando enche, como faz tokens_push no lexer). */
void ast_push(Ast *parent, Ast *child);

/* Libera o nó e, recursivamente, tudo que pende dele. */
void ast_free(Ast *node);

/* Libera UM nó, sem tocar nos filhos.
 *
 * Existe por causa do parser: quando uma análise é abandonada no meio (erro
 * sintático), sobram subárvores órfãs que ninguém mais referencia. Em Python o
 * coletor de lixo dá conta disso sozinho; aqui o parser guarda todos os nós que
 * criou numa lista e libera um por um no fim — e para isso a liberação precisa
 * ser rasa, senão os filhos (que também estão na lista) seriam liberados duas
 * vezes. Ver src/c/parser.c. */
void ast_free_node(Ast *node);

/* Reescreve um valor de char/string na forma com escapes (barra invertida
 * seguida de n, t, barra invertida, aspa simples ou aspa dupla), para imprimir
 * o literal na AST — o lexer entrega esses valores já decodificados. O chamador
 * é dono da string devolvida. */
char *ast_escape(const char *texto);

/* AST na forma compacta de uma linha — a notação dos testes oficiais.
 * O chamador é dono da string devolvida (usar free()). */
char *ast_to_sexpr(const Ast *node);

/* AST indentada, um nó por linha e dois espaços por nível.
 * O chamador é dono da string devolvida (usar free()). */
char *ast_to_tree(const Ast *node);

#endif /* AST_H */
