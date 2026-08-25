/* File-scope variables, and their initialisers.
 *
 * A global has no statement site for its initialiser to run at - a local's
 * runs where the declaration stood, and a global has no such place - so the
 * value has to ride the declaration itself or be silently dropped. 'scale'
 * is a double for that reason: an int global losing its initialiser reads
 * as zero and zero is what it was set to, which hides the bug.
 */

#include <stdio.h>

int counter = 0;
double scale = 1.5;

int bump(int by)
{
    counter = counter + by;
    return counter;
}

int main(void)
{
    int i;

    for (i = 0; i < 3; i++) {
        bump(i + 1);
    }
    printf("counter %d\n", counter);
    printf("scaled %f\n", counter * scale);
    return 0;
}
