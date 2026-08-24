#include <stdio.h>
#define LIMIT 10
#define SQUARE(x) ((x) * (x))
#ifdef DEBUG
#define TRACE 1
#endif

int main(void) {
    printf("%d\n", SQUARE(LIMIT));
    return 0;
}
