#include <stdio.h>

int main(void) {
    int a = 48;
    int b = 18;
    int r;
    printf("gcd of %d %d\n", a, b);
    while (b != 0) {
        r = a % b;
        a = b;
        b = r;
    }
    printf("is %d\n", a);
    return 0;
}
