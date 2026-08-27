/* The shapes that look like the guard in cases/c2s/guarded.c and are not.
 *
 * That one is dropped because it decides nothing: an #ifndef holding only the
 * #define it guards, in a file with no other translation unit beside it, is a
 * definition that always stood. Each of these asks a real question, and the
 * answer is not written down anywhere in the file - so the run must stop with
 * the list, exactly as it did before guards were understood at all.
 *
 *   - WITH_ELSE picks one of two values. Dropping the guard would pick one
 *     of them silently, which is choosing the program on the author's behalf.
 *   - CARRIED guards a declaration as well as its #define, so what the guard
 *     holds is not only the definition.
 *   - #ifdef IS_SET means "define it only if somebody else already did",
 *     which is the opposite question and has no answer here either.
 *   - MISMATCH defines a different name from the one it tests, so the block
 *     is not a guard around its own definition.
 *
 * The narrowness is the point. A conditional is a question until it is
 * provably not one.
 */

#include <stdio.h>

#ifndef WITH_ELSE
#define WITH_ELSE 1
#else
#define WITH_ELSE 2
#endif

#ifndef CARRIED
#define CARRIED 3
int carried_along = 1;
#endif

#ifdef IS_SET
#define IS_SET 4
#endif

#ifndef MISMATCH
#define SOMETHING_ELSE 5
#endif

int main(void)
{
    printf("%d\n", WITH_ELSE + CARRIED);
    return 0;
}
