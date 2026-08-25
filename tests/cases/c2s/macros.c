/* #defines that name a value rather than decide a program.
 *
 * These are not a question for the author: expanding them is exactly what a
 * preprocessor would do and what the author meant, so the converter does it
 * rather than stopping to ask. Object-like and function-like both, a macro
 * used inside another macro, and one whose name is also a Shalimar builtin
 * constant - 'PI' expands to its number before any name could collide.
 *
 * A function-like macro's name is only a call when a '(' follows it, which
 * is why 'SQUARE' can be mentioned in a comment and 'LIMIT' can end a loop
 * bound. cases/defines/defines.c holds the ones that do have to be asked
 * about.
 */

#include <stdio.h>

#define PI 3.14159
#define LIMIT 5
#define SQUARE(x) ((x) * (x))
#define AREA(r) (PI * SQUARE(r))
#define MAXOF(a, b) ((a) > (b) ? (a) : (b))
#define GREET "hi"

int main(void)
{
    int i;
    int total = 0;
    int bigger;
    double a;

    for (i = 1; i <= LIMIT; i++) {
        total = total + SQUARE(i);
    }
    a = AREA(2.0);
    bigger = MAXOF(3, 9);

    printf("%s\n", GREET);
    printf("total %d\n", total);
    printf("area %f\n", a);
    printf("bigger %d\n", bigger);
    return 0;
}
