/* '?:' as the whole right side of an assignment, which lowers to if / else.
 * Anywhere else it is a refusal - see cases/beyond/lowering.c.
 */

#include <stdio.h>

int main(void)
{
    int a = 7;
    int b = 12;
    int max;
    int min;
    int sign;

    max = a > b ? a : b;
    min = a < b ? a : b;
    sign = a - b > 0 ? 1 : -1;

    printf("max %d min %d sign %d\n", max, min, sign);
    return 0;
}
