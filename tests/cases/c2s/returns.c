/* '?:' as the whole of a return.
 *
 * There is no target to write twice the way an assignment has one, but
 * there are two returns, which says the same thing. Three shapes matter:
 *
 *   - the plain one;
 *   - a chain, which must come out as an 'else if' ladder rather than nesting
 *     one 'if' inside the next 'else', so that the Shalimar reads branch for
 *     branch like the C it came from;
 *   - a conditional in the 'then' arm, which nests, and may.
 *
 * A conditional that is only part of a larger expression is still refused;
 * cases/beyond/lowering.c holds that.
 */

#include <stdio.h>

int pick(int a, int b)
{
    return a > b ? a : b;
}

int grade(int n)
{
    return n >= 90 ? 1 : n >= 75 ? 2 : n >= 50 ? 3 : 4;
}

int inner(int a, int b, int c)
{
    return a > 0 ? (b > c ? b : c) : 0;
}

double half(int n)
{
    return n % 2 == 0 ? n / 2.0 : 0.0;
}

int main(void)
{
    printf("pick %d %d\n", pick(3, 9), pick(9, 3));
    printf("grades %d %d %d %d\n", grade(95), grade(80), grade(60), grade(20));
    printf("inner %d %d\n", inner(1, 4, 7), inner(-1, 4, 7));
    printf("half %f %f\n", half(8), half(7));
    return 0;
}
