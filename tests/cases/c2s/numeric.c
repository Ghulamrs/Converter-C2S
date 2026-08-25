/* The scalar arithmetic that has to survive the crossing.
 *
 * The char lines are the point of the file. C promotes a char to int the
 * moment it is used arithmetically and says nothing about it; Shalimar has
 * no such rule and refuses '+' on a char outright. So the promotion has to
 * be written into the output as int() - and where it is missed on a '%d',
 * shc accepts the result and prints the character where C printed its code.
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
    printf("ch %d next %d\n", ch, ch + 1);
    printf("real %f\n", (double)n / 4.0);
    return 0;
}
