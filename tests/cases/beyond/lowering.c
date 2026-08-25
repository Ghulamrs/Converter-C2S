/* Valid C89 that the lowerings deliberately decline rather than guess at.
 *
 * Every one of these has a shape the converter handles in a neighbouring
 * form, which is what makes them worth pinning: the refusal is a boundary,
 * not an absence, and a boundary can move without anyone noticing.
 *
 *   - '?:' is lowered where a statement can be expanded around it: the
 *     whole right side of an assignment, and the whole of a return. Inside
 *     a larger expression - an operand, a call argument - there is nowhere
 *     to put the branch, so it is refused. cases/c2s/ternary.c and
 *     cases/c2s/returns.c have the forms that work.
 *   - do-while is peeled into the body once and then a while. A break or a
 *     continue in the body would land in the peeled copy, outside any loop.
 *   - '&&' becomes '&' when the right side is pure, because Shalimar's form
 *     evaluates both sides. Indexing is not pure - 'i < n && a[i]' is the
 *     whole reason short-circuit exists - so it is refused.
 *   - a counting for whose counter is read afterwards has to lower to a
 *     while, and a continue in that while would skip the step. Either alone
 *     is fine; cases/c2s/counters.c and cases/c2s/skipping.c have them.
 */

#include <stdio.h>

int pick(int a, int b)
{
    /* The conditional is an operand of '+', not the whole return, so there
     * is no statement to expand it into. 'return a > b ? a : b;' converts. */
    return 1 + (a > b ? a : b);
}

int announce(int a, int b)
{
    /* And as a call argument, for the same reason. */
    printf("%d\n", a > b ? a : b);
    return 0;
}

int peel(void)
{
    int n = 1;
    int steps = 0;

    do {
        n = n * 3;
        steps++;
        if (n > 100) {
            break;
        }
    } while (n < 1000);

    return steps;
}

int guarded(int a[], int n, int i)
{
    int hits = 0;

    if (i < n && a[i] > 0) {
        hits = hits + 1;
    }
    return hits;
}

int scan(void)
{
    int i;
    int sum = 0;

    for (i = 0; i < 10; i++) {
        if (i % 3 == 0) {
            continue;
        }
        sum = sum + i;
    }
    return sum + i;
}

int main(void)
{
    int v[3];

    v[0] = 1;
    v[1] = 2;
    v[2] = 3;
    printf("%d %d %d %d %d\n", pick(3, 9), announce(3, 9), peel(),
           guarded(v, 3, 1), scan());
    return 0;
}
