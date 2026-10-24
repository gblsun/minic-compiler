// Soma dos MDCs de todos os pares (a, b) com 1 <= a, b <= 60 (algoritmo de Euclides).
int mdc(int a, int b) {
    int r;
    while (b != 0) {
        r = a % b;
        a = b;
        b = r;
    }
    return a;
}

int main() {
    int a;
    int b;
    int soma = 0;
    for (a = 1; a <= 60; a = a + 1) {
        for (b = 1; b <= 60; b = b + 1) {
            soma = soma + mdc(a, b);
        }
    }
    print(soma);
    return 0;
}
