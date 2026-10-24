// for com continue (precisa do rótulo L_step), while com break, laços aninhados.
int main() {
    int i;
    int j;
    int soma = 0;
    for (i = 0; i < 6; i = i + 1) {
        if (i % 2 == 0) {
            continue;
        }
        j = 0;
        while (true) {
            if (j >= i) {
                break;
            }
            soma = soma + j;
            j = j + 1;
        }
    }
    print(soma);
    for (;;) {
        soma = soma - 1;
        if (soma < 5) {
            break;
        }
    }
    print(soma);
    return 0;
}
