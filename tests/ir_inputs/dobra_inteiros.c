// Dobra de constantes com a semântica de int de 32 bits do C: transbordamento
// em complemento de dois, divisão truncada em direção a zero, resto com o
// sinal do dividendo. Divisão por variável zero nunca é removida nem dobrada.
int main() {
    int grande = 2147483647;
    int z = 0;
    print(grande + 1);
    print(-7 / 2);
    print(-7 % 2);
    print(7 % -2);
    print(3 * 4 - 10 / 3);
    if (z != 0) {
        print(1 / z);
    }
    return 0;
}
