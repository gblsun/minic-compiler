// Código invariante (aula 19): `a * b` não muda dentro do laço e sai para o
// pré-cabeçalho. `x / y` não sai: com divisor desconhecido, movê-la para fora
// faria a divisão acontecer mesmo num laço que roda zero vezes.
int main() {
    int a;
    int b;
    int x;
    int y;
    int i = 0;
    int soma = 0;
    read(a);
    read(b);
    read(x);
    read(y);
    while (i < 5) {
        soma = soma + a * b;
        if (y != 0) {
            soma = soma + x / y;
        }
        i = i + 1;
    }
    print(soma);
    return 0;
}
