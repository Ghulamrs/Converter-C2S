/* printf with more arguments than the format has holes.
 *
 * Legal C89, and C evaluates every one of them - the extras run their side
 * effects and then print nothing. Dropping them changed what the program
 * did with the converter reporting success, and Shalimar has no statement
 * that evaluates a value only to discard it, so the mismatch is refused.
 */

#include <stdio.h>

int bumped;

int bump(void)
{
    bumped = bumped + 1;
    return 7;
}

int main(void)
{
    printf("start \n", bump());
    printf("a %d \n", bumped, bump());
    return 0;
}
