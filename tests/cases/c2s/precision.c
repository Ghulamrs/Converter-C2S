/* '%.Nf', which is 'prec(N)' said in C.
 *
 * The two mean the same thing - a fixed number of decimal places - so this
 * carries across with no difference to measure, which is what this case is
 * here to keep true. %f alone is prec(6), being C's default, and that was
 * always here; the precision spelled out was refused until 2026-08-27 and
 * stopped conversions over the commonest format there is.
 *
 * %.0f is included because it is the one that looks like it might not work:
 * C writes no decimal point at all, and so does prec(0).
 *
 * A precision on anything else is not this. cases/beyond/formats.c holds
 * those, along with the format text that cannot be spelled at all.
 */

#include <stdio.h>

int main(void)
{
    double v = 3.14159265;
    double small = 0.125;
    int i;

    printf("five %.5f\n", v);
    printf("two %.2f\n", v);
    printf("none %.0f\n", v);
    printf("default %f\n", v);
    printf("small %.3f\n", small);

    for (i = 1; i <= 3; i++) {
        printf("row %d value %.4f\n", i, v * i);
    }
    return 0;
}
