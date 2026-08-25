/* Where the short-circuit rewrite stops, with the permission GIVEN.
 *
 * The rewrite is a temporary and a nest of ifs, and ifs are statements.
 * So it works exactly where a statement can be expanded around the
 * expression, and nowhere else - the same boundary as '?:', for the
 * same reason and on purpose. cases/allow/shortcircuit.c holds the
 * positions that do work.
 *
 * A boundary can move without anyone noticing, which is what this case
 * is for. Each refusal below has a neighbour a few lines away in the
 * allow case that converts.
 *
 * The right sides here are all impure - a call, an index, a division -
 * because a pure right side needs no rewrite at all and becomes a plain
 * Shalimar '&' or '|'.
 */

#include <stdio.h>

int noisy(int x)
{
    return x > 2;
}

int main(void)
{
    int a[4];
    int i = 0;
    int n = 4;
    int r = 0;
    int zero = 0;

    a[0] = 1;

    /* a call argument: there is no statement between the argument and
     * the call to put the branch in */
    printf("%d \n", i < n && a[i] > 0);

    /* an operand of a larger expression: the branch would have to run
     * in the middle of evaluating the '+' */
    r = 1 + (i < n && a[i] > 0);

    /* a chained 'else if'. The first condition lifts happily; this one
     * must not be evaluated until the one above it has failed, and
     * statements hoisted ahead of the whole if would run regardless */
    if (r > 100) {
        printf("big \n");
    } else if (zero != 0 && 100 / zero > 3) {
        printf("guarded \n");
    }

    /* a for condition, which is asked again every turn like a while's -
     * but the for lowering has nowhere to put the recomputation */
    for (i = 0; i < n && noisy(a[i]); i++) {
        printf("for %d \n", i);
    }

    return 0;
}
