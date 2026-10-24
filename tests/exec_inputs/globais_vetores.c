// Vetor global, vetor passado por referência e global alterada por função.
int contador = 0;
int dados[5];

void preencher(int v[], int n) {
    int i;
    for (i = 0; i < n; i = i + 1) {
        v[i] = i * i;
        contador = contador + 1;
    }
}

int somar(int v[], int n) {
    int i;
    int s = 0;
    for (i = 0; i < n; i = i + 1) {
        s = s + v[i];
    }
    return s;
}

int main() {
    int local[3];
    preencher(dados, 5);
    preencher(local, 3);
    print(somar(dados, 5));
    print(somar(local, 3));
    print(contador);
    return 0;
}
