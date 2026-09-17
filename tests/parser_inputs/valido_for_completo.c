int main() {
    int soma = 0;
    int i;
    for (i = 0; i < 10; i = i + 1) {
        if (i % 2 == 0) {
            continue;
        }
        if (i > 7) {
            break;
        }
        soma = soma + i;
    }
    return soma;
}
