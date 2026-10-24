// Formato do print para cada tipo: int, float (menor representação exata,
// com ".0" quando é inteiro), bool, char e cadeia.
int main() {
    float metade = 1.0 / 2;
    float grande = 1.5;
    int i;
    print(42);
    print(-7);
    print(metade);
    print(3.0);
    print(0.1 + 0.2);
    for (i = 0; i < 5; i = i + 1) {
        grande = grande * 1000.0;
    }
    print(grande);
    print(true);
    print(3 < 2);
    print('A');
    print("olá, mundo");
    print('a' + 1);
    return 0;
}
