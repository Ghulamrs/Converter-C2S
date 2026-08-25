/* Falling through, and an arm that leaves from the middle.
 *
 * The two rewrites have to compose. Fall-through turns the arms into
 * guarded blocks tested by an entry index, and a mid-arm break has to
 * escape all of them at once rather than just the block it is in - so
 * the wrapper loop goes around the whole lowering, not around an arm.
 *
 * Here case 0 breaks out of an if in its middle, and then, had it not,
 * would have fallen into case 1 and on into the default. Both facts are
 * being tested at once: the run must show 'one' and 'other' for k = 1
 * and nothing at all from case 0's tail.
 *
 * This was a refusal in cases/beyond/ until the wrapper existed. It is
 * here rather than deleted because the boundary moved, and a case that
 * moves from refused to converted is worth keeping on the converted
 * side to say so.
 */

#include <stdio.h>

int main(void)
{
    int k = 1;

    switch (k) {
    case 0:
        printf("zero \n");
        if (k == 0) {
            break;
        }
        printf("still zero \n");
    case 1:
        printf("one \n");
    default:
        printf("other \n");
    }

    return 0;
}
