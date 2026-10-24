// Recursão dupla: custo dominado por chamadas (CALL/RET), que o -O1 não remove.
int fib(int n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

int main() {
    print(fib(20));
    return 0;
}
