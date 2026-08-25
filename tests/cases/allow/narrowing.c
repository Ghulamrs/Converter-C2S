/* --allow-narrowing: a C type wider than Shalimar's three scalars.
 *
 * Shalimar has int, real and char. 'unsigned', 'long' and 'short' have
 * no counterpart, and narrowing one to 'int' is a real change of
 * meaning, not a formality: a value above INT_MAX stops being
 * representable, C's defined unsigned wraparound stops existing, and
 * Shalimar makes passing the int limit a runtime error rather than a
 * wrapped value that looks right. So the values here stay well inside
 * int - what is being proved is that the narrowing carries arithmetic
 * and printing faithfully, not that it survives an overflow, which it
 * would not.
 *
 * The formats come with it. Once 'unsigned' is 'int' there is nothing
 * unsigned left to print, so '%u' and the 'l' and 'h' length modifiers
 * describe a type that is no longer there and have to drop. '%x' and
 * '%o' are not here because they stay refused either way - those are a
 * radix and '?' writes decimal.
 *
 * 'float' needs no permission and is here to say so: it widens to real,
 * which loses nothing.
 */

#include <stdio.h>

long total(long a, long b)
{
    return a + b;
}

int main(void)
{
    unsigned int u = 7;
    long l = 90000;
    short s = 3;
    unsigned long ul = 12345;
    float f = 1.5f;
    int i;

    printf("u %u \n", u);
    printf("l %ld \n", l);
    printf("s %hd \n", s);
    printf("ul %lu \n", ul);
    printf("f %f \n", f);
    printf("sum %ld \n", total(l, 10));

    for (i = 0; i < 3; i++) {
        u = u + 1;
        printf("step %u \n", u);
    }

    return 0;
}
