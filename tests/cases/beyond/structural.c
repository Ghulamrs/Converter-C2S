/* Shapes a whole program can take that Shalimar's cannot.
 *
 * Shalimar's 'main' takes nothing and returns nothing, and a function's
 * outputs are scalars - so each of these is refused at the declaration
 * rather than anywhere inside a body.
 *
 * Note what refusing a parameter costs: 'main' loses its whole body, since
 * there is no converted signature for the statements to hang under. The
 * marker names the parameter and the body simply does not appear, which is
 * why this case asserts nothing about what is inside it.
 */

#include <stdio.h>

struct pair { int a; int b; };

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
    printf("%d %d %s\n", p.a, p.b, argv[0]);
    return argc - argc;
}
