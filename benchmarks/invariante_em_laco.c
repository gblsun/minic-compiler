// Expressões que não mudam dentro do laço (aula 19): o -O1 as calcula uma vez,
// antes do laço.
int main() {
    int largura = 37;
    int altura = 11;
    int escala = 3;
    int i;
    int total = 0;
    for (i = 0; i < 2000; i = i + 1) {
        total = total + largura * altura * escala + i % 7;
    }
    print(total);
    return 0;
}
