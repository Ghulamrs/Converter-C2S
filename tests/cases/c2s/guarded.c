/* The one conditional that decides nothing: a #define wearing its own guard.
 *
 *     #ifndef M_PI
 *     #define M_PI 3.14
 *     #endif
 *
 * Every other #if asks which program this is, and the file does not say -
 * which is why cases/defines/defines.c has to stop. This one does not ask.
 * The guard holds nothing but the definition it guards, and a file being
 * converted has no other translation unit to have defined the name first, so
 * the definition always stood. The two lines are dropped, the middle one is
 * the substitution it always was, and the output says in a comment that they
 * were dropped.
 *
 * Refusing this stopped conversions over a header idiom that is in almost
 * every file wanting a constant - M_PI most of all, which MSVC hides behind
 * _USE_MATH_DEFINES and everyone therefore defines by hand.
 *
 * Three shapes here: the classic guard, one whose block is written with the
 * define indented and a comment on it, and a guard around a function-like
 * macro, which is the same question and the same answer.
 *
 * **Each one warns, because this file includes a header.** A dropped guard is
 * only provably right when nothing else could have defined the name, and the
 * converter translates nothing from a header and cannot see what one defines.
 * The pair that proves it is <math.h> and M_PI: math.h defines M_PI as the
 * full pi, so a C program takes that and never the 3.14 written here, while
 * the conversion takes the 3.14 - and the two then compute different numbers.
 * <stdio.h> defines none of these three, which is why this case runs
 * identically either way and is here rather than in beyond/.
 */

#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14
#endif

#ifndef LIMIT
    #define LIMIT 4   /* how far the loop counts */
#endif

#ifndef SQUARE
#define SQUARE(x) ((x) * (x))
#endif

int main(void)
{
    int i;
    int total = 0;
    double circle;

    for (i = 1; i <= LIMIT; i++) {
        total = total + SQUARE(i);
    }
    circle = M_PI * SQUARE(2.0);

    printf("total %d\n", total);
    printf("circle %f\n", circle);
    return 0;
}
