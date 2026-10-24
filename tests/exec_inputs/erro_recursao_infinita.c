// Erro de execução (código 6): recursão sem caso base estoura a pilha de
// chamadas (limite de 10000 chamadas aninhadas).
int desce(int n) {
    return desce(n + 1);
}

int main() {
    print(desce(0));
    return 0;
}
