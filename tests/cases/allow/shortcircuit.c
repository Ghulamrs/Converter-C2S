/* --allow-short-circuit: '&&' and '||' whose right side is not pure.
 *
 * Shalimar's '&' and '|' ask both sides before either is answered, so
 * 'i < n & a[i] = 0' indexes a[n] on the last turn. That is the whole
 * reason C's short circuit exists, and it is why this rewrite is off
 * unless asked for by name: the version that compiles without it does
 * not mean the same thing.
 *
 * Every guarded right side here would fault or be observable if it ran
 * when C says it must not - an index past the end, a division by zero,
 * a call that prints. So the two programs agreeing is not a formality:
 * it is the proof that the guard held.
 *
 * cases/beyond/shortcircuit.c holds the other half - the positions where
 * there is no statement to expand the rewrite into, which stay refused
 * even with the permission given.
 */

#include <stdio.h>

int noisy(int x)
{
    printf("noisy %d \n", x);
    return x > 2;
}

int main(void)
{
    int a[4];
    int i;
    int n = 4;
    int zero = 0;
    int r;

    for (i = 0; i < 4; i++) {
        a[i] = i;
    }

    /* division by zero on the right, guarded by the left */
    if (zero != 0 && 100 / zero > 3) {
        printf("divided \n");
    } else {
        printf("guarded \n");
    }

    /* a chain, every side impure - '(a && b) && c', so the rewrite has
     * to reach through its own left side rather than refuse there */
    if (noisy(1) && noisy(4) && noisy(5)) {
        printf("chain yes \n");
    } else {
        printf("chain no \n");
    }

    /* '||' stops at a true left side */
    if (noisy(9) || noisy(0)) {
        printf("or taken \n");
    }

    /* '||' that has to ask the right side */
    if (noisy(0) || noisy(7)) {
        printf("or second \n");
    }

    /* the whole right side of an assignment */
    r = (zero != 0 && 100 / zero > 3);
    printf("r %d \n", r);
    r = (zero == 0 || 100 / zero > 3);
    printf("r %d \n", r);

    /* a while, where the condition is asked again every turn - and a
     * 'continue' in the body, which must return to the top and have the
     * whole condition recomputed, not skip it */
    i = 0;
    while (i < n && a[i] < 3) {
        i++;
        if (i == 2) {
            continue;
        }
        printf("loop %d \n", i);
    }
    printf("end %d \n", i);

    return 0;
}
