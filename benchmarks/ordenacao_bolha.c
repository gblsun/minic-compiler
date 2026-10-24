// Bubble sort de 200 valores pseudoaleatórios (gerador congruente linear).
int main() {
    int n = 200;
    int v[200];
    int semente = 12345;
    int i;
    int j;
    int troca;
    for (i = 0; i < n; i = i + 1) {
        semente = (semente * 1103 + 12345) % 65536;
        v[i] = semente;
    }
    for (i = 0; i < n - 1; i = i + 1) {
        for (j = 0; j < n - 1 - i; j = j + 1) {
            if (v[j] > v[j + 1]) {
                troca = v[j];
                v[j] = v[j + 1];
                v[j + 1] = troca;
            }
        }
    }
    print(v[0]);
    print(v[n - 1]);
    return 0;
}
