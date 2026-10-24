// Propagação entre blocos: `k` vale 5 nos dois ramos do if (propaga depois da
// junção); `m` vale 1 ou 2 conforme o ramo (não propaga). Dentro do laço, `i`
// muda a cada volta (não propaga), mas `passo` não muda (propaga).
int main() {
    int k;
    int m;
    int i = 0;
    int passo = 2;
    int lido;
    read(lido);
    if (lido > 0) {
        k = 5;
        m = 1;
    } else {
        k = 5;
        m = 2;
    }
    print(k + m);
    while (i < 6) {
        i = i + passo;
    }
    print(i);
    return 0;
}
