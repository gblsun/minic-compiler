// Redução de força (aula 19): `i * 3` e `j * 8`, com `i` e `j` variáveis de
// indução, viram somas incrementais. `k` muda de dois jeitos dentro do laço e
// não é variável de indução básica: `k * 5` fica como está.
int main() {
    int v[20];
    int i;
    int j = 10;
    int k = 1;
    for (i = 0; i < 6; i = i + 1) {
        v[i] = i * 3;
        print(j * 8);
        j = j - 2;
        if (i % 2 == 0) {
            k = k + 1;
        } else {
            k = k * 2;
        }
        print(k * 5);
    }
    print(v[5]);
    return 0;
}
