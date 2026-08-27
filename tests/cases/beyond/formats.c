/* Formats with no expression in Shalimar's print list.
 *
 * A precision on anything but an 'f': '%.3d' is zero-padding and '%.3s' is a
 * truncation, and neither is what prec(n) means - prec is decimal places and
 * nothing else. '%.5f' itself does carry, being exactly that; that is in
 * cases/c2s/precision.c.
 *
 * Text running straight on from a value is NOT here. It converts, with a
 * warning and one space the C did not write, and cases/spacing/punctuation.c
 * is where that is checked - a difference the converter makes and says it
 * makes, rather than one it refuses over.
 */

#include <stdio.h>

int main(void)
{
    int n = 5;

    printf("padded %.3d\n", n);
    printf("cut %.2s\n", "abcdef");
    return 0;
}
