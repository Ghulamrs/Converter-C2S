#include <stdio.h>

struct point { int x; int y; };

int main(void) {
    int a = 5;
    int *p = &a;
    unsigned long big = 7ul;
    a = a << 2;
    goto done;
done:
    printf("%d\n", a);
    return 0;
}
