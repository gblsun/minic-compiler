// Erro de execução (código 6): índice fora dos limites do vetor.
int main() {
    int v[4];
    int i;
    for (i = 0; i <= 4; i = i + 1) {
        v[i] = i;
    }
    print(v[0]);
    return 0;
}
