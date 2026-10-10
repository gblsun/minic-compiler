bool par(int n) {
    if (n == 0) return true;
    return impar(n - 1);
}
bool impar(int n) {
    if (n == 0) return false;
    return par(n - 1);
}
int main() {
    if (par(4)) return 1;
    else {
        return 0;
    }
}
