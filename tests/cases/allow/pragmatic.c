/* --pragmatic: all four permissions at once, in one program.
 *
 * Each of the three rewrites has a case of its own; this is the one
 * that says they compose. They interact in ways worth pinning: the
 * fall-through lowering puts arm bodies inside 'if' blocks, and a
 * short-circuit rewrite inside one of those arms has to lift its
 * statements into that block rather than to the top of the function.
 * A narrowed 'long' counter then runs the loop around all of it.
 */

#include <stdio.h>

int reach(int a[], int i, int n)
{
    printf("reach %d \n", i);
    return i < n && a[i] > 0;
}

int main(void)
{
    int a[4];
    unsigned int k;
    long n = 4;
    int i;

    for (i = 0; i < 4; i++) {
        a[i] = i;
    }

    for (k = 0; k < 5; k++) {
        switch ((int)k) {
        case 0:
        case 1:
            if (k < (unsigned int)n && a[k] < 3) {
                printf("guarded %u \n", k);
            }
        case 2:
            printf("mid %u \n", k);
            break;
        default:
            printf("high %u %d \n", k, reach(a, (int)k, (int)n));
        }
    }

    return 0;
}
