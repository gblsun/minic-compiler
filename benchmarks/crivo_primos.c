// Crivo de Eratóstenes até 5000: laços aninhados, vetor e muitas comparações.
int main() {
    int limite = 5000;
    int composto[5001];
    int i;
    int j;
    int primos = 0;
    for (i = 2; i <= limite; i = i + 1) {
        if (composto[i] == 0) {
            primos = primos + 1;
            for (j = i * i; j <= limite; j = j + i) {
                composto[j] = 1;
            }
        }
    }
    print(primos);
    return 0;
}
