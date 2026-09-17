float media(float valores[], int n) {
    float total = 0.0;
    int i;
    for (i = 0; i < n; i = i + 1) {
        total = total + valores[i];
    }
    return total / n;
}
