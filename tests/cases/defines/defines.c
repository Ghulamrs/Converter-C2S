/* The preprocessor lines that are a question for the author.
 *
 * All four here change WHICH PROGRAM THIS IS, and nothing in the file says
 * which one is wanted:
 *
 *   - TEST_VERSION is defined and then tested, so the type of 'test'
 *     depends on a decision that has not been written down.
 *   - #ifdef / #else / #endif are the test itself.
 *   - #undef changes what a name means partway down a file.
 *   - a replacement using '#' is stringify, which has no token-level
 *     equivalent here.
 *
 * A #define that only names a value is NOT in this file - it is not a
 * question, it is a substitution, and cases/c2s/macros.c is where those
 * are. The run must stop with the list, and write nothing.
 */

#include <stdio.h>

#define TEST_VERSION
#define NAME(x) #x
#define SCRATCH 1

#ifdef TEST_VERSION
float test = 0.0f;
#else
double test = 0.0;
#endif

#undef SCRATCH

int main(void)
{
    printf("%f\n", test);
    return 0;
}
