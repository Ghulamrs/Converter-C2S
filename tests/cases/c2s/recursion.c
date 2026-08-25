/* Recursion, and more than one function calling another.
 *
 * Single spaces between the holes: '?' writes one space after every item,
 * so a format with two spaces in it comes out with one, and the outputs
 * would differ over something that is not a bug. The suite compares up to
 * the trailing space only.
 */

#include <stdio.h>

int fact(int n)
{
    if (n <= 1) {
        return 1;
    }
    return n * fact(n - 1);
}

int fib(int n)
{
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

int main(void)
{
    int i;

    for (i = 1; i <= 6; i++) {
        printf("fact %d = %d fib = %d\n", i, fact(i), fib(i));
    }
    return 0;
}
