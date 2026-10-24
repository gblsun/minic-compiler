// Uma chamada pode mudar qualquer global: nada do que se sabia sobre `contador`
// antes de `incrementa()` pode ser propagado para depois dela.
int contador = 10;
int limite;

void incrementa() {
    contador = contador + 1;
}

int main() {
    int antes;
    limite = 3;
    antes = contador;
    incrementa();
    print(antes);
    print(contador);
    print(limite + 1);
    return 0;
}
