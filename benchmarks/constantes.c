// Muito trabalho com valores conhecidos em tempo de compilação: dobra e
// propagação de constantes eliminam quase tudo.
int main() {
    int base = 10;
    int altura = 4;
    int area = base * altura / 2;
    int perimetro = 2 * (base + altura);
    int i;
    int acumulado = 0;
    for (i = 0; i < 1000; i = i + 1) {
        acumulado = acumulado + area + perimetro - 28;
    }
    print(acumulado);
    return 0;
}
