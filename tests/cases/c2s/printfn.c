/* printf becomes a Shalimar function that prints, and a call to it.
 *
 * The reason is evaluation order. C works out every argument of a printf
 * before it writes any of them. Shalimar's '?' evaluates and writes one
 * item at a time, so an argument that itself prints used to come out
 * interleaved with the line it was an argument to - 'high 3 reach 3'
 * instead of 'reach 3' and then 'high 3 1'. The same values in the wrong
 * order, and nothing to say it had happened.
 *
 * A call is the fix rather than a patch over it: Shalimar evaluates a
 * call's arguments before entering the function, which is what C does at
 * a printf. So 'announce' below - which prints before it answers - must
 * have said its piece before the line it is an argument to appears.
 *
 * Also here:
 *   - the same format used from two places, which must make ONE function
 *     and not two, because the body is a function of the format and the
 *     parameter types alone;
 *   - the same format text with a different argument type, which must
 *     make a SECOND function, because those bodies are not the same;
 *   - a format with no holes, which needs no function - there is nothing
 *     to evaluate and nothing to pass, so it stays an inline '?';
 *   - %s, %c and %f, which fix the parameter's type from the format.
 *     %s is given a literal rather than a char buffer on purpose: a C
 *     string ends at a NUL and Shalimar's text carries its own length,
 *     so a partly filled char[6] prints six characters there and two
 *     here. That mismatch is real and is not what this case is about.
 *
 * Note the spaces around every hole. '?' writes a space after each item,
 * so a format that puts one there too comes out matching; one that does
 * not - 'f(%d)' - differs by that space. That is the known cost of '?'
 * and not something this case is testing.
 */

#include <stdio.h>

int announce(int x)
{
    printf("announce %d \n", x);
    return x * 2;
}

void twice(int n)
{
    printf("shared %d \n", n);
}

int main(void)
{
    char c = 'Z';
    double d = 2.5;
    int i;

    /* the argument prints: it must print BEFORE this line does */
    printf("value %d \n", announce(3));

    /* the same format from two places - one function */
    twice(1);
    printf("shared %d \n", 2);

    /* no holes at all - stays inline */
    printf("plain \n");

    /* the format types */
    printf("text %s \n", "hi");
    printf("letter %c \n", c);
    printf("real %f \n", d);

    /* in a loop: still one function, called many times */
    for (i = 0; i < 3; i++) {
        printf("turn %d \n", i);
    }

    return 0;
}
