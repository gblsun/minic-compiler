// Mesmo nome em escopos diferentes vira x#1, x#2...; uma variável chamada t1
// não pode ser confundida com o temporário t1.
int x = 1;

int soma(int x) {
    int t1 = x + 1;
    {
        int x = t1 * 2;
        print(x);
    }
    return x + t1;
}

int main() {
    print(soma(5));
    print(x);
    {
        int x = 9;
        print(x);
    }
    return 0;
}
