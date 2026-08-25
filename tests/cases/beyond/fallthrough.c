/* Where the fall-through rewrite stops, with the permission GIVEN.
 *
 * The entry/done lowering can express a case running on into the next,
 * because that is a question about which arm control entered at. It
 * cannot express a jump out of the MIDDLE of an arm: a 'break' inside
 * an 'if' inside a case says "leave the switch from here", and there is
 * no statement in Shalimar to leave. Nothing in the arm bodies is a
 * loop, so the break is bound to the switch and not to something the
 * lowering could carry.
 *
 * cases/allow/fallthrough.c holds every arm shape that does convert,
 * including arms that end with a break and arms that end with a return.
 */

#include <stdio.h>

int main(void)
{
    int k = 1;

    switch (k) {
    case 0:
        printf("zero \n");
        if (k == 0) {
            break;
        }
        printf("still zero \n");
    case 1:
        printf("one \n");
    default:
        printf("other \n");
    }

    return 0;
}
