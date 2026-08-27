/* A name that is a library function in one function and a variable in another.
 *
 * Legal C: `hyp` calls sqrt(), `main` has a local called sqrt, and the two do
 * not meet. Shalimar's `uses` is per FILE, so the borrow that hyp needs takes
 * the name away from every variable in the output - and the converter has to
 * see that coming, because it renames names long before, or long after, it
 * meets the call that creates the borrow.
 *
 * Before this was handled the conversion came out as valid-looking Shalimar
 * that shc refused outright: "'sqrt' is borrowed on line 1 - drop the borrow
 * or use another name". Valid C in, invalid Shalimar out, which is the one
 * result this converter must never produce.
 *
 * Three things at once, and each would hide the others if split up:
 *
 *   sqrt  borrowed AND wanted as a name -> the variable has to be renamed
 *   fmod  called with two arguments, which becomes the `%` operator and
 *         borrows nothing - so a variable called fmod must be LEFT ALONE
 *   ceil  never called at all, so nothing is borrowed and the name is free
 *
 * The middle one is the trap. Renaming every borrowable name would pass the
 * first case and quietly fail this one, and `uses` exists precisely so that a
 * name costs nothing until it is asked for.
 */

#include <stdio.h>
#include <math.h>

double hyp(double a, double b)
{
    return sqrt(a * a + b * b);
}

double rem2(double a, double b)
{
    return fmod(a, b);
}

int main(void)
{
    double sqrt = 3.0;
    double fmod = 1.5;
    double ceil = 7.25;

    printf("%.4f %.4f %.4f\n", sqrt, fmod, ceil);
    printf("%.4f %.4f\n", hyp(3.0, 4.0), rem2(7.5, 2.0));
    return 0;
}
