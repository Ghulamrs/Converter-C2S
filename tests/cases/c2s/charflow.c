/* A char fed from int-valued expressions, and asked int-shaped questions.
 *
 * Three places shc wants the conversion written down and C writes nothing:
 * an int expression stored into a char needs char() around it ('a' + 1 is
 * int arithmetic on two literals - no char operand, so no permission
 * involved); '!' of a char compares its code, so the code must be asked
 * for with int(); and a switch selector is promoted to int before the case
 * values see it, so the saved selector writes that promotion as int() too.
 * Each of these once converted with exit 0 into a file shc refused.
 */

#include <stdio.h>

/* Added 2026-08-25: a function that ANSWERS with a char. Every other
 * store into a char wraps; this one had nothing to ask, because the C
 * tree carries no types and a return cannot see its own function's. It
 * emitted the bare code point into a 'fun <char>' and shc refused the
 * output while c2s had already exited 0.
 */
char grade(int n)
{
    if (n > 5) {
        return 'A';
    }
    return 66;
}

int main(void)
{
    char c;
    char d;

    c = 'a' + 1;
    d = c;
    if (!d) {
        printf("empty \n");
    } else {
        printf("full \n");
    }

    switch (c) {
        case 'a':
            printf("A \n");
            break;
        case 'b':
            printf("B \n");
            break;
        default:
            printf("other \n");
            break;
    }

    printf("c %c d %c \n", c, d);
    printf("graded %c %c \n", grade(9), grade(1));
    return 0;
}
