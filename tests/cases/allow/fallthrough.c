/* --allow-fall-through: a switch case that runs on into the next.
 *
 * The if/elseif chain the ordinary lowering builds cannot say "and then
 * the one after": every branch of a Shalimar 'if' is exclusive. So this
 * permission trades the chain for an entry index and a done flag, which
 * can.
 *
 * The shapes here are the ones that lowering can get wrong, and each is
 * a different wrongness:
 *
 *   - a 'default' in the MIDDLE of the list. C picks it only when no
 *     case label matched, yet a case above it can still fall INTO it.
 *     A lowering that tested the labels in order as it ran the bodies
 *     would get one of those two right and the other wrong.
 *   - an arm that returns rather than breaks, which ends the switch as
 *     surely.
 *   - grouped labels ('case 0: case 1:') ahead of a falling body.
 *   - a selector matching nothing, with and without a default present.
 *   - a switch inside a loop, which is where the hoisted temporaries
 *     would be declared in the wrong place if they were minted during
 *     the statement walk.
 */

#include <stdio.h>

int grade(int n)
{
    int score = 0;

    switch (n) {
    case 0:
        score = score + 1;
    default:
        score = score + 10;
        break;
    case 5:
        score = score + 100;
        return score;
    case 9:
        score = score + 1000;
    }
    return score;
}

int nodefault(int n)
{
    int score = 0;

    switch (n) {
    case 1:
        score = score + 1;
    case 2:
        score = score + 10;
        break;
    case 3:
        score = score + 100;
    }
    return score;
}

int main(void)
{
    int i;

    for (i = 0; i < 11; i++) {
        printf("grade %d %d \n", i, grade(i));
    }
    for (i = 0; i < 5; i++) {
        printf("nodefault %d %d \n", i, nodefault(i));
    }

    for (i = 0; i < 4; i++) {
        switch (i) {
        case 0:
        case 1:
            printf("low %d \n", i);
        case 2:
            printf("mid %d \n", i);
            break;
        default:
            printf("high %d \n", i);
        }
    }

    return 0;
}
