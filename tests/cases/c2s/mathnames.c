/* Every C89 maths call this converter recognises, so that the list it works
 * from cannot drift from Shalimar's table again.
 *
 * It had drifted: builtinFor() held a hand-written fourteen names while the
 * table held twenty, so hypot, round and trunc converted to nothing at all
 * while being perfectly available. It asks the table now, and this is what
 * would notice if somebody wrote the list out by hand a second time.
 *
 * **C89 only, and the line is not where you would guess.** cc1 is a C89
 * compiler and its <math.h> declares sinh, cosh, tanh and log10 - those are
 * older than the standard - but NOT hypot, round, trunc, cbrt or log2, which
 * are C99. So a Shalimar program may borrow those five and no C program here
 * can reach them; they are covered by ../../../Compiler-S/tests/cases/uses_more.shm
 * instead.
 *
 * fmod is absent on purpose: the converter turns it into Shalimar's `%`, which
 * is the same operation and needs no borrow.
 */
#include <stdio.h>
#include <math.h>

int main(void)
{
    double x = 2.0;
    printf("%f %f \n", sqrt(x), fabs(0.0 - x));
    printf("%f %f %f \n", sin(x), cos(x), tan(x));
    printf("%f %f %f \n", asin(0.5), acos(0.5), atan(0.5));
    printf("%f %f \n", atan2(1.0, 2.0), pow(x, 3.0));
    printf("%f %f \n", ceil(1.2), floor(1.8));
    printf("%f %f \n", log(x), exp(1.0));
    printf("%f %f %f \n", sinh(x), cosh(x), tanh(x));
    printf("%f \n", log10(1000.0));
    printf("%f \n", fmod(7.5, 2.0));
    return 0;
}
