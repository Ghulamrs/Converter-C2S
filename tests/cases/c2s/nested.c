/* Nested loops with a continue in the inner one, and a '?:' assignment
 * inside both. Neither counter is read after its loop, so both keep
 * Shalimar's counting for - which is the case counters.c does not cover.
 */

#include <stdio.h>

int main(void)
{
    int i;
    int j;
    int count = 0;
    int best = 0;

    for (i = 1; i <= 4; i++) {
        for (j = 1; j <= 4; j++) {
            if (i * j > 6) {
                continue;
            }
            count = count + 1;
            best = i * j > best ? i * j : best;
        }
    }

    printf("count %d best %d\n", count, best);
    return 0;
}
