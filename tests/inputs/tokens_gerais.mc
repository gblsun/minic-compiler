// cobre todas as palavras reservadas, operadores, delimitadores e literais
/* comentario
   de bloco em
   varias linhas */
int float_dummy;
float pi = 3.14;
bool ok = true;
bool nao_ok = false;
char letra = 'a';
char nova_linha = '\n';
char tab = '\t';
char barra = '\\';
char aspas_simples = '\'';
char aspas_duplas = '\"';
void vazio;

int soma(int a, int b) {
    if (a >= b && b <= a) {
        return a + b - 1 * 2 / 3 % 4;
    } else {
        while (a != b) {
            a = a - 1;
        }
        for (a = 0; a < b; a = a + 1) {
            if (a == 0) {
                continue;
            }
            if (a > 100) {
                break;
            }
        }
        return -a;
    }
}

int main() {
    int v[2];
    v[0] = 1;
    v[1] = !ok || nao_ok;
    print(soma(1, 2));
    read(v[0]);
    return 0;
}
