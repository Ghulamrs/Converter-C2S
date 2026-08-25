/* What a for loop leaves behind in its counter.
 *
 * C leaves the variable holding whatever ended the loop - the limit it
 * failed, or the value it broke at - and reading it there is how a search
 * reports its index. Shalimar's 'for i : a to b' binds its own counter and
 * leaves the outer variable untouched, so the counting form is only
 * faithful when nothing reads the counter afterwards.
 *
 * Both exits are here: one loop breaks, one runs to completion.
 */

#include <stdio.h>

int main(void)
{
    int i;
    int j;
    int found = -1;

    for (i = 0; i < 5; i++) {
        if (i == 3) {
            found = i;
            break;
        }
    }
    printf("after break i %d found %d\n", i, found);

    for (j = 0; j < 4; j++) {
        found = found + j;
    }
    printf("after full run j %d found %d\n", j, found);

    return 0;
}
