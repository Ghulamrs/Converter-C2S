/* '&&' and '||' where the right side is pure, so Shalimar's '&' and '|' -
 * which evaluate both sides - mean the same thing. An impure right side is
 * refused instead; cases/beyond/lowering.c holds that one.
 *
 * '!b' is here too: it lowers to 'b = 0', which is exact, because ':' is
 * assignment in Shalimar and '=' is the equality test.
 */

#include <stdio.h>

int main(void)
{
    int a = 3;
    int b = 0;
    int c = 5;

    if (a > 0 && c > 4) {
        printf("and yes\n");
    } else {
        printf("and no\n");
    }
    if (b != 0 || c == 5) {
        printf("or yes\n");
    } else {
        printf("or no\n");
    }
    if (!b) {
        printf("not b\n");
    }

    printf("%d %d\n", a > 0 && b > 0, a > 0 || b > 0);
    return 0;
}
