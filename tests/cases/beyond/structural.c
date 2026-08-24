/* Shapes a whole program can take that Shalimar's cannot.
 *
 * Shalimar's 'main' takes nothing and returns nothing, a function's outputs
 * are scalars, and there are no variadic functions - so each of these is
 * refused at the declaration rather than anywhere inside a body.
 *
 * 'total' is declared and never defined on purpose. Defining it needs
 * va_list, and va_list is a typedef from a header this converter drops
 * unread, so the declaration would not parse - see the note in
 * docs/ANALYSIS.md about names a header would have introduced. A prototype
 * and a call reach the same refusal without standing on that.
 */

#include <stdio.h>

struct pair { int a; int b; };

int total(int count, ...);

struct pair make(int a, int b)
{
    struct pair result;
    result.a = a;
    result.b = b;
    return result;
}

int main(int argc, char **argv)
{
    struct pair p;

    p = make(1, 2);
    printf("%d %d %d %s\n", p.a, p.b, total(2, 10, 20), argv[0]);
    return argc - argc;
}
