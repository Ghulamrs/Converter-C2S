/* Operators C has and Shalimar does not.
 *
 * The bitwise family is the dangerous one: '&' and '|' exist in Shalimar as
 * logical operators and '^' exists as power, so a mechanical translation
 * would compile and mean something else entirely. They must be refused, not
 * mapped.
 *
 * Three lines here are deliberately NOT refused, and are in this file to
 * keep it that way. '!a' lowers to 'a = 0', which is exact - ':' is
 * assignment in Shalimar and '=' is the equality test. Unary '+' is the
 * identity. And 'c = a = b' lifts into two statements in order. All three
 * are listed under 6.3 in docs/ANALYSIS.md as refusals; the converter does
 * better than the document, and the document is what is out of date.
 */

#include <stdio.h>

int main(void)
{
    int a;
    int b;
    int c;
    int mask;

    a = 12;
    b = 10;

    mask = a & b;
    mask = a | b;
    mask = a ^ b;
    mask = ~a;
    mask = a << 2;
    mask = a >> 1;
    mask = !a;
    mask = +a;
    mask = sizeof(int);

    c = (a, b);
    c = a = b;

    printf("%d %d\n", mask, c);
    return 0;
}
