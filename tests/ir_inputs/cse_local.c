// Subexpressão comum com valores que só se conhecem em execução (read): a * b
// é calculado uma vez só no -O1.
int main() {
    int a;
    int b;
    int x;
    int y;
    read(a);
    read(b);
    x = a * b + 1;
    y = a * b + 2;
    print(x);
    print(y);
    a = a + 1;
    print(a * b);
    return 0;
}
