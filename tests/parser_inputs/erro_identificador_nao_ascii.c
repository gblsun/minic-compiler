// A especificação (Seção 3.5) só aceita identificador ASCII: [A-Za-z_][A-Za-z0-9_]*.
// O 'ç' e o dígito arábico-índico '٣' são erro léxico, e o parser nem chega a rodar.
int main() {
    int ação = ٣;
    return 0;
}
