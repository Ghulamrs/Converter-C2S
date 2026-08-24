#include <stdio.h>
#include <math.h>

int classify(int n) {
    int kind;
    switch (n % 3) {
    case 0:
        kind = 10;
        break;
    case 1:
    case 2:
        kind = 20;
        break;
    default:
        kind = 30;
        break;
    }
    return kind;
}

int main(void) {
    int i;
    int total = 0;
    double x = 2.0;
    for (i = 1; i <= 5; i++) {
        total += i * i;
    }
    printf("sum of squares %d\n", total);
    for (i = 10; i > 0; i -= 3) {
        total = total - i;
    }
    printf("after countdown %d\n", total);
    i = 0;
    do {
        i++;
    } while (i < 4);
    printf("do-while ran to %d\n", i);
    printf("classify %d %d %d\n", classify(3), classify(4), classify(5));
    printf("sqrt %f\n", sqrt(x));
    if (total > 100) {
        printf("big\n");
    } else if (total > 10) {
        printf("medium\n");
    } else {
        printf("small\n");
    }
    return 0;
}
