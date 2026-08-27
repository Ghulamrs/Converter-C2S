/* Formats that have no expression in Shalimar's print list.
 *
 * '?' writes every item followed by a single space and has no way not to -
 * the language says so - so a format whose text runs straight on from a hole
 * cannot be spelled at all. "value %d." would have to come out as
 * `? "value" n "."`, which writes `value 5 . ` where the C wrote `value 5.`.
 * That converted silently and printed differently until 2026-08-27, which is
 * the one thing a converter must not do, and no case here had punctuation
 * against a hole to catch it.
 *
 * A precision on anything but an 'f' is the other one: '%.3d' is
 * zero-padding and '%.3s' is a truncation, and neither is what prec(n)
 * means. '%.5f' itself does carry - cases/c2s/precision.c is where.
 */

#include <stdio.h>

int main(void)
{
    int n = 5;
    double v = 3.14159;

    printf("value %d.\n", n);
    printf("%d%d\n", n, n);
    printf("padded %.3d\n", n);
    printf("cut %.2s\n", "abcdef");
    printf("both %.5f:\n", v);
    return 0;
}
