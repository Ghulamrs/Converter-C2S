/* A 'break' that leaves a switch from somewhere other than the end of a
 * case, with the switch inside a loop and again outside one.
 *
 * This case exists because the converter used to accept the first shape
 * and be wrong. C binds a 'break' to the nearest enclosing switch OR
 * loop, and inside a case that is the switch. The converter tracked only
 * loop depth and a switch did not reset it, so a break inside an if
 * inside a case, with a for outside, emitted a Shalimar 'break' - which
 * left the FOR. Four iterations became one, nothing was marked, and c2s
 * exited 0.
 *
 * What carries it now is a loop that always ends its first turn:
 *
 *     while 1 {
 *       <the arms>
 *       break
 *     }
 *
 * which is what a switch is - a block entered once and left at a point
 * of its choosing. Every break in a case binds to that wrapper, and the
 * wrapper holds exactly this switch, so leaving it and leaving the
 * switch are the same jump. The enclosing for never sees it, which is
 * the line 'after switch 1' below: the break fired, the switch ended,
 * and the loop went on to 2 and 3.
 *
 * The wrapper is built only for a switch that needs one. Every ordinary
 * case ending in its own break is in cases/c2s/switching.c and still
 * lowers to a plain if/elseif chain with no loop around it.
 */

#include <stdio.h>

int main(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        switch (i) {
        case 1:
            if (i == 1) {
                break;
            }
            printf("unreachable %d \n", i);
            break;
        default:
            printf("default %d \n", i);
            break;
        }
        printf("after switch %d \n", i);
    }

    switch (i) {
    case 4:
        if (i == 4) {
            break;
        }
        printf("also unreachable \n");
        break;
    default:
        printf("outside a loop \n");
        break;
    }

    return 0;
}
