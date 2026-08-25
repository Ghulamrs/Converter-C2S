/* Two things that used to convert, compile, run, and answer differently.
 *
 * Neither was refused, because neither looks like a gap: both have a
 * Shalimar spelling that reads like the right one. That is what made
 * them worse than the constructs with no spelling at all - a marker
 * says "finish this by hand", and these said nothing.
 *
 * FIRST: comparing whole arrays. In C89 's == "abc"' compares two
 * addresses and is false for two distinct objects; Shalimar's char[] is
 * text and '=' compares what it holds. C printed 'diff' and the
 * conversion printed 'same'. There is no faithful fallback either -
 * Shalimar has no addresses to compare - so it is a refusal, with the
 * by-hand rewrite named. Comparing ELEMENTS is untouched and still
 * converts: cases/c2s/strings.c walks a string that way.
 *
 * SECOND: '%g' and '%e'. Both were carried straight through to a '?'
 * item, and '?' writes a real one way, so C's '0.5' and '5.000000e-01'
 * both came out '0.5000000'. '%f' survives because prec(6) matches its
 * six places exactly; the other two are a choice of notation and of
 * significant digits, and prec is neither of those. Refused until
 * Shalimar has something to say them with.
 */

#include <stdio.h>

int main(void)
{
    char s[4];
    double d = 0.5;

    s[0] = 'a';
    s[1] = 'b';
    s[2] = 'c';
    s[3] = 0;

    if (s == "abc") {
        printf("same \n");
    } else {
        printf("diff \n");
    }

    printf("g %g \n", d);
    printf("e %e \n", d);

    return 0;
}
