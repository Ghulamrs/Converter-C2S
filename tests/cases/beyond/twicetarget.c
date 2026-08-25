/* Assignments whose rewrite would read the written place back.
 *
 * 'x *= e' becomes 'x : x * e', and 'a = b = c' unchains into 'b : c' and
 * then 'a : b' - both read the target a second time where C read it once.
 * That is invisible until a call hides in the target: 'b[f()]' would run
 * f twice, once per read, and the converted program would quietly disagree
 * with the original. A call in the written-back place is refused; the same
 * shapes with a plain index lower as they always did.
 */

#include <stdio.h>

int calls;
int b[3];

int f(void)
{
    calls = calls + 1;
    return 0;
}

int main(void)
{
    int a;

    b[f()] *= 3;
    a = b[f()] = 5;

    printf("a %d calls %d \n", a, calls);
    return 0;
}
