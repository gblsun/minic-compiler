// Maior sequência de Collatz para n < 3000: laço com desvios e divisão.
int passos(int n) {
    int p = 0;
    while (n != 1) {
        if (n % 2 == 0) {
            n = n / 2;
        } else {
            n = 3 * n + 1;
        }
        p = p + 1;
    }
    return p;
}

int main() {
    int n;
    int maior = 0;
    int melhor = 0;
    for (n = 1; n < 3000; n = n + 1) {
        int p = passos(n);
        if (p > maior) {
            maior = p;
            melhor = n;
        }
    }
    print(melhor);
    print(maior);
    return 0;
}
