// Laço em que o -O1 tira código invariante e reduz a multiplicação a somas;
// a saída tem de ser a mesma nos dois níveis.
int main() {
    int a;
    int b;
    int i;
    int soma = 0;
    int pares[8];
    read(a);
    read(b);
    for (i = 0; i < 8; i = i + 1) {
        pares[i] = i * 2;
        soma = soma + a * b + pares[i];
    }
    print(soma);
    print(pares[7]);
    return 0;
}
