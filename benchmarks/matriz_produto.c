// Produto de matrizes 20x20 guardadas em vetores (linha * 20 + coluna): o
// endereço `i * n` é um caso clássico de redução de força.
int main() {
    int n = 20;
    int a[400];
    int b[400];
    int c[400];
    int i;
    int j;
    int k;
    int soma;
    int traco = 0;
    for (i = 0; i < n; i = i + 1) {
        for (j = 0; j < n; j = j + 1) {
            a[i * 20 + j] = i + j;
            b[i * 20 + j] = i * j + 1;
        }
    }
    for (i = 0; i < n; i = i + 1) {
        for (j = 0; j < n; j = j + 1) {
            soma = 0;
            for (k = 0; k < n; k = k + 1) {
                soma = soma + a[i * 20 + k] * b[k * 20 + j];
            }
            c[i * 20 + j] = soma;
        }
    }
    for (i = 0; i < n; i = i + 1) {
        traco = traco + c[i * 20 + i];
    }
    print(traco);
    return 0;
}
