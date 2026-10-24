// Inteiros de 32 bits: transbordamento, divisão truncada e resto com o sinal
// do dividendo, calculados em tempo de execução (com valores lidos).
int main() {
    int maior;
    int a;
    int b;
    read(maior);
    read(a);
    read(b);
    print(maior + 1);
    print(maior * 2);
    print(a / b);
    print(a % b);
    print(-a / b);
    print(-a % b);
    return 0;
}
