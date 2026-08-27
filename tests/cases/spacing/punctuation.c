/* Text that runs straight on from a value, which is the one thing the
 * converter changes.
 *
 * '?' writes a space after every item and the language has no way to say
 * otherwise. Nor can the line be built as a single item instead: Shalimar has
 * no string concatenation and no number-to-text builtin, so there is nothing
 * to assemble. `printf("value %d.\n", n)` can only become
 * `? "value" n "."`, which writes `value 5 .` where the C wrote `value 5.`.
 *
 * It converts, warns, and is checked here against the C with runs of spaces
 * squeezed - so the numbers, the words and the lines all still have to match,
 * and only the spacing is forgiven. Refusing this instead stops a conversion
 * over one space; printing it silently is what a converter must never do.
 *
 * A space in the format pays for the one '?' adds, which is why the last line
 * here needs no warning of its own and comes out exactly.
 */

#include <stdio.h>

int main(void)
{
    int n = 5;
    int m = 7;
    double v = 3.14159;

    printf("value %d.\n", n);
    printf("Hello %.5f:\n", v);
    printf("%d%d\n", n, m);
    printf("(%d)\n", n);
    printf("plain %d and %d\n", n, m);
    return 0;
}
