/* Every construct here lands somewhere the map has to get right: a
   declaration whose initialiser is hoisted away from it, a loop whose body is
   indented under a header the printer writes, a call that becomes a function
   of its own, and a return. The .expect file names each one. */
#include <stdio.h>

int gcd(int a, int b)
{
    while (b != 0) {
        int t = b;
        b = a % b;
        a = t;
    }
    return a;
}

int main(void)
{
    int x = 84;
    int y = 36;
    printf("gcd = %d\n", gcd(x, y));
    return 0;
}
