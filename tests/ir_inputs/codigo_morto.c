// Atribuição morta, ramo constante e código depois de return.
int f(int x) {
    int lixo;
    lixo = x * 100;
    if (false) {
        print(999);
    }
    return x + 1;
    print(x);
}

int main() {
    int r;
    int nunca;
    r = f(4);
    nunca = r * 2;
    nunca = 0;
    print(r);
    return 0;
}
