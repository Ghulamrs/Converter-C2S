/* C names that Shalimar used to take away.
 *
 * Every one of the twenty builtins occupied the function namespace, and
 * pi and e could not be variables at all - so a C program with its own
 * max() came out as max_v(), and a variable named e as e_v. Correct
 * output that read wrong, and for names people reach for first.
 *
 * A builtin is now what a name means when nothing has claimed it, so
 * these arrive as themselves. The reserved list here is down to the
 * fifteen actual keywords and prec.
 *
 * sqrt is called as well as defined-over: this program's max is its
 * own, and its sqrt is the language's, in the same file.
 */

#include <stdio.h>
#include <math.h>

int max(int a, int b)
{
    if (a > b) {
        return a;
    }
    return b;
}

int len(int n)
{
    return n * 2;
}

int main(void)
{
    int e = 5;
    int min = 1;
    double abs = 2.5;

    printf("max %d \n", max(2, 9));
    printf("len %d \n", len(4));
    printf("e %d min %d \n", e, min);
    printf("abs %f \n", abs);
    printf("sqrt %f \n", sqrt(16.0));

    return 0;
}
