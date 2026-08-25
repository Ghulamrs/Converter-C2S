/* char[] as text, and char as a number.
 *
 * Both spellings of a char have to come out right: '%c' wants the
 * character, '%d' wants its code, and a comparison against a char literal
 * stays a char comparison rather than becoming two codes.
 *
 * The two conditions are nested rather than joined with '&&' on purpose.
 * 's[i] >= 'a' && s[i] <= 'z'' is refused - indexing cannot cross a
 * short-circuit, because Shalimar's '&' evaluates both sides - and that
 * refusal is tested in cases/beyond/lowering.c, not undone here.
 */

#include <stdio.h>

int main(void)
{
    char s[16] = "hello";
    char t[16] = "world";
    char c;
    int i;
    int lower = 0;

    printf("s %s t %s\n", s, t);

    for (i = 0; i < 5; i++) {
        c = s[i];
        if (c >= 'a') {
            if (c <= 'z') {
                lower = lower + 1;
            }
        }
    }
    printf("lower letters %d\n", lower);

    if (s[0] == 'h') {
        printf("starts with h\n");
    }

    c = s[1];
    printf("second %c code %d\n", c, c);
    return 0;
}
