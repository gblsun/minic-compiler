// Erro de execução (código 6): divisão por zero com divisor lido. O print
// anterior sai normalmente antes do erro.
int main() {
    int a = 10;
    int b;
    read(b);
    print(a);
    print(a / b);
    return 0;
}
