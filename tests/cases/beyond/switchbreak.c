/* A 'break' that leaves a switch from somewhere other than the end of a
 * case - and the switch inside a loop.
 *
 * This case exists because the converter used to accept it and be
 * wrong. C binds a 'break' to the nearest enclosing switch OR loop, and
 * inside a case that is the switch. The converter tracked only loop
 * depth, and a switch did not reset it - so a break inside an if inside
 * a case, with a for outside, emitted a Shalimar 'break', which left the
 * FOR. The program compiled, ran, and stopped iterating three turns
 * early. Nothing was marked and c2s exited 0.
 *
 * That is the shape of fault this whole suite is built to catch: not a
 * refusal to convert, but a conversion that runs and disagrees. Both
 * shapes are here - the switch inside a loop, which was the wrong one,
 * and the switch outside any loop, which was already refused - because
 * the fix is that the two now answer the same way.
 *
 * A loop written INSIDE an arm is a different matter and still converts;
 * cases/c2s/switching.c holds that, along with every ordinary case that
 * ends with its own break.
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
