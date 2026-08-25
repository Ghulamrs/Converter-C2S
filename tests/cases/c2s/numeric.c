/* The scalar arithmetic that has to survive the crossing.
 *
 * Everything here converts with no permission asked: plain int and real
 * arithmetic, the compound assignments spelling themselves out, and the
 * hex and octal literals arriving in decimal. The '%d' of a char - a
 * printing question, not an arithmetic one - stays here too. What left is
 * 'ch + 1': arithmetic on a char is C's silent promotion, which is now a
 * permission, and cases/allow/chararith.c holds that story.
 */

#include <stdio.h>

int main(void)
{
    int n = 10;
    double x = 2.5;
    int h = 0x1F;
    int o = 017;
    char ch = 'A';

    n += 5;
    n -= 2;
    n *= 3;
    n /= 4;
    n %= 7;
    n++;
    --n;

    x = x * 2.0 + 1.0;

    printf("n %d h %d o %d\n", n, h, o);
    printf("x %f trunc %d\n", x, (int)x);
    printf("ch %d \n", ch);
    printf("real %f\n", (double)n / 4.0);
    return 0;
}
