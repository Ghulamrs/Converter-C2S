/* Folding a declaration's opening assignment back into the declaration, and
 * the three shapes where it must NOT happen. Every one of these is here
 * because folding it would be wrong in a way that still compiles.
 */
#include <stdio.h>

int tick = 0;
int f(void) { tick = tick * 10 + 1; return tick; }
int g(void) { tick = tick * 10 + 2; return tick; }

/* Folds: leading, in order, reading only what is already set. */
int plain(void)
{
    int a = 1;
    int b = a + 4;
    int c = b * 2;
    return a + b + c;
}

/* Must not fold the inner one: it runs once per turn, not once at entry. */
int inLoop(void)
{
    int i;
    int total = 0;
    for (i = 0; i < 3; i++) {
        int acc = 100;
        acc = acc + i;
        total = total + acc;
    }
    return total;
}

/* Must not fold both: declared a then b but assigned b then a, so folding
 * both would run g() before f() and reverse the side effects. */
int outOfOrder(void)
{
    int a;
    int b;
    b = f();
    a = g();
    return a * 100 + b;
}

int main(void)
{
    printf("plain %d \n", plain());
    printf("inLoop %d \n", inLoop());
    printf("outOfOrder %d tick %d \n", outOfOrder(), tick);
    return 0;
}
