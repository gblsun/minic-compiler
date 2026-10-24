// Conversões implícitas viram instruções: int -> float (itof) e char -> int (ctoi).
float metade(float x) {
    return x / 2;
}

int main() {
    int n = 7;
    float f = n;
    char c = 'a';
    int codigo = c;
    print(metade(n));
    print(f + 1);
    print(codigo + 1);
    print(c < 'b');
    print(-c);
    print(7 / 2);
    return 0;
}
