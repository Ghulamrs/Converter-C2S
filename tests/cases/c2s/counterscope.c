/* Two ways a for counter is read that a scan of one function body in
 * source order does not see. Both produced a program that compiled,
 * ran, and printed a different number.
 *
 * cases/c2s/counters.c holds the ordinary shape - a counter read after
 * its loop, in the same function, below it - which was already caught.
 * These two are the holes beside it.
 *
 * FIRST: the counter is a global. Shalimar's 'for i : a to b' binds its
 * own counter and leaves the outer variable alone; C's leaves it holding
 * what ended the loop. The check for that read only the function being
 * converted, so a read from ANOTHER function was invisible - report()
 * below printed 0 where C prints 3. A global is now treated as read,
 * because one function body is all this walk can see and unprovable has
 * to mean escaped.
 *
 * SECOND: the read is above the loop, but the loop is inside a loop. So
 * 'before' is a fact about the source and not about the run: on the
 * second turn the read above the for is reading exactly what the for
 * left behind. Source order alone called that safe, and the converted
 * program printed 100 twice where C prints 100 and then 3.
 *
 * Both now fall back to the while lowering, which assigns the variable
 * C's way. The cost is the counting form's readability in these shapes,
 * which is the right thing to lose.
 */

#include <stdio.h>

int shared;

void report(void)
{
    printf("global after %d \n", shared);
}

int main(void)
{
    int i = 100;
    int turn = 0;

    for (shared = 0; shared < 3; shared++) {
        printf("global in %d \n", shared);
    }
    report();

    while (turn < 2) {
        printf("before %d \n", i);
        for (i = 0; i < 3; i++) {
            printf("in %d \n", i);
        }
        turn = turn + 1;
    }
    printf("after %d \n", i);

    return 0;
}
