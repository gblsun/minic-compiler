// `&&`/`||` com chamada à direita: a chamada só pode acontecer quando o lado
// esquerdo não decide sozinho o resultado (o print dentro dela denuncia).
bool avisa(int x) {
    print(x);
    return x > 0;
}

int main() {
    bool a = false;
    bool b = true;
    if (a && avisa(1)) {
        print(10);
    }
    if (b || avisa(2)) {
        print(20);
    }
    if (b && avisa(3)) {
        print(30);
    }
    print(a || !b);
    return 0;
}
