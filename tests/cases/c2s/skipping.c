/* 'continue' in a counting for.
 *
 * Safe here, and only here: Shalimar's for steps its own counter, so a
 * continue cannot skip the step. The same loop written where the counter is
 * read afterwards has to lower to a while instead, and there the continue
 * WOULD skip the step - so that combination is refused rather than lowered.
 * cases/beyond/lowering.c holds it.
 */

#include <stdio.h>

int main(void)
{
    int i;
    int sum = 0;

    for (i = 0; i < 10; i++) {
        if (i % 3 == 0) {
            continue;
        }
        sum = sum + i;
    }

    printf("sum %d\n", sum);
    return 0;
}
