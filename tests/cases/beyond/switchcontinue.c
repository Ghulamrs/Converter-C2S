/* A 'continue' in a case, in a switch that also breaks from the middle.
 *
 * This is the one thing the wrapper costs, and it is the exact mirror of
 * the fault the wrapper was built to fix.
 *
 * A switch that some arm leaves from the middle is wrapped in
 * 'while 1 { ... break }' so that C's break has a Shalimar loop to
 * leave. But a 'continue' written in a case belongs to a loop OUTSIDE
 * the switch, and Shalimar binds continue to the innermost loop - which
 * is now the wrapper. It would run the switch again instead of ending
 * the enclosing turn. The two jumps want different loops and only one
 * can be innermost, so this shape is refused rather than converted into
 * something that runs.
 *
 * Both halves are needed for the refusal, which is why the neighbours
 * matter: cases/c2s/switching.c has a continue in a case and converts,
 * because nothing there breaks from the middle and so no wrapper is
 * built. cases/c2s/switchbreak.c breaks from the middle and converts,
 * because nothing there continues. It is only the two together.
 *
 * A continue inside a loop written WITHIN an arm is that loop's and is
 * not affected; the second loop below is here to hold that line.
 */

#include <stdio.h>

int main(void)
{
    int i;
    int k;
    int j;

    for (i = 0; i < 4; i++) {
        switch (i) {
        case 1:
            if (i == 1) {
                break;
            }
            printf("unreachable %d \n", i);
            break;
        case 2:
            continue;
        default:
            printf("default %d \n", i);
            break;
        }
        printf("after switch %d \n", i);
    }

    for (k = 0; k < 2; k++) {
        switch (k) {
        case 0:
            for (j = 0; j < 3; j++) {
                if (j == 1) {
                    continue;
                }
                printf("inner %d \n", j);
            }
            break;
        default:
            printf("outer %d \n", k);
            break;
        }
    }

    return 0;
}
