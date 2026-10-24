// Programa de referência 19.2 da especificação: entrada, vetor e repetição.
int main() {
    int valores[5];
    int i;
    int soma = 0;
    for (i = 0; i < 5; i = i + 1) {
        read(valores[i]);
        soma = soma + valores[i];
    }
    print(soma);
    return 0;
}
