// Vetor local, vetor passado como parâmetro e read num elemento.
int total(int v[], int n) {
    int i;
    int s = 0;
    for (i = 0; i < n; i = i + 1) {
        s = s + v[i];
    }
    return s;
}

int main() {
    int dados[4];
    int k;
    for (k = 0; k < 3; k = k + 1) {
        dados[k] = k * k;
    }
    read(dados[3]);
    print(total(dados, 4));
    print(dados[2]);
    return 0;
}
