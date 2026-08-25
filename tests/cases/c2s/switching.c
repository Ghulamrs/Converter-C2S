/* A switch inside a loop.
 *
 * Two things this settles that a switch at the top of a function does not.
 * The selector temporary has to be declared at the top of the FUNCTION, not
 * where the switch stands - Shalimar allows a declaration nowhere else, and
 * a switch at function level satisfies that by accident. And the two breaks
 * mean different things: the one in the case ends the switch, the one after
 * it ends the loop.
 *
 * The selector is a call, so the count says whether it was evaluated once
 * per iteration as C does, or once per case tested.
 */

#include <stdio.h>

int calls = 0;

int sel(int n)
{
    calls = calls + 1;
    return n % 4;
}

int main(void)
{
    int i;
    int acc = 0;

    for (i = 0; i < 6; i++) {
        switch (sel(i)) {
        case 0:
            acc = acc + 1;
            break;
        case 1:
            acc = acc + 10;
            break;
        default:
            acc = acc + 100;
            break;
        }
        if (acc > 200) {
            break;
        }
    }

    printf("acc %d calls %d i %d\n", acc, calls, i);
    return 0;
}
