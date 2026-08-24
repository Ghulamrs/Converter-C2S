/* Types with no Shalimar spelling.
 *
 * Shalimar has three scalars - int, real, char - and the array. Every
 * declaration below is ordinary C89 that cc1 compiles, and none of them has
 * anywhere to land, so each must come back marked rather than narrowed
 * quietly into whichever scalar happens to be nearest.
 */

#include <stdio.h>

typedef int Counter;

struct point { int x; int y; };

union value { int i; double d; };

enum colour { red, green, blue };

int main(void)
{
    Counter n;
    struct point p;
    union value v;
    enum colour c;
    unsigned int u;
    short s;
    long l;
    long double wide;

    n = 3;
    p.x = 1;
    p.y = 2;
    v.i = 9;
    c = green;
    u = 2;
    s = 4;
    l = 5;
    wide = 1.0;

    printf("%d\n", n + p.x + v.i + s);
    return 0;
}
