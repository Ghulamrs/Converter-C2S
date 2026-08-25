/* --allow-char-arithmetic: a char reaching an arithmetic operator.
 *
 * C promotes the char to its code the moment '+' or '-' touches it and
 * says nothing. Shalimar refuses arithmetic on a char outright, so the
 * conversion has to write the promotion out as int() - a real change in
 * what the program says, which is why it is asked for by name. Both
 * directions are exercised: the code leaving the char ('c - '0'' landing
 * in an int) and the result coming back ('c + 1' landing in a char again,
 * which needs char() around the int the arithmetic made).
 */

#include <stdio.h>

int main(void)
{
    char c = '7';
    int d;

    d = c - '0';
    printf("d %d \n", d);

    c = c + 1;
    printf("c %c \n", c);
    return 0;
}
