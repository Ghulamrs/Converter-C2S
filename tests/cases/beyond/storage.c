/* Storage classes and linkage.
 *
 * A Shalimar program is whole - there is no separate compilation, so there
 * is nothing for 'extern' to refer to and nothing for 'static' to hide
 * from. 'register' and 'auto' name a storage decision the language does not
 * let anyone make.
 */

#include <stdio.h>

extern int errno;

static int hidden = 4;

static int twice(int n)
{
    return n * 2;
}

int main(void)
{
    register int fast;
    auto int ordinary;
    static int kept = 1;

    fast = 2;
    ordinary = 3;
    kept = kept + 1;

    printf("%d\n", twice(hidden) + fast + ordinary + kept);
    return 0;
}
