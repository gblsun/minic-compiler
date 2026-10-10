int limite = 10;
int soma(int a, int b) {
    return a + b;
}
float promove(int n) {
    return n;
}
int main() {
    int valores[3];
    int i;
    int total = 0;
    float x;
    bool ok;
    char letra = 'A';
    for (i = 0; i < 3; i = i + 1) {
        valores[i] = i * 2;
        total = total + valores[i];
    }
    x = promove(total) + 0.5;
    ok = x > 2.0 && !(total == limite);
    while (ok && i < limite) {
        i = i + 1;
        if (i == 7) continue;
        if (i > 8) break;
    }
    total = total + letra;
    read(valores[0]);
    print(soma(total, 1));
    print("fim");
    return total % 3;
}
