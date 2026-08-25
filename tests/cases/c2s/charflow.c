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
    return 0;
}
