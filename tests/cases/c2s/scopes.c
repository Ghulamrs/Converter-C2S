/* Block scoping, which Shalimar does not have.
 *
 * Every declaration hoists to the top of the function, so three different
 * 'x' become three different names. The renaming was never the hard part -
 * deciding which 'x' a given reference means is, and getting that wrong
 * does not fail to compile. It prints 215 instead of 118.
 */

#include <stdio.h>

int main(void)
{
    int x = 1;
    int total = 0;

    {
        int x = 10;
        total = total + x;
    }
    {
        int x = 100;
        int y = 5;
        total = total + x + y;
    }
    if (x == 1) {
        int y = 2;
        total = total + y;
    }
    total = total + x;

    printf("total %d\n", total);
    return 0;
}
