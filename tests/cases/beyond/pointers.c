/* Pointers, of which Shalimar has none.
 *
 * There is no indirection in the language at all: no address, no
 * dereference, no pointer arithmetic, no null. An array is passed by name
 * and carries its own extents, which is the only thing near a reference
 * that Shalimar offers.
 */

#include <stdio.h>
#include <stdlib.h>

struct node { int value; struct node *next; };

int main(void)
{
    int a[4];
    int *p;
    int *q;
    struct node *head;
    char *text;

    a[0] = 1;
    a[1] = 2;
    a[2] = 3;
    a[3] = 4;

    p = &a[0];
    q = p + 2;
    text = "hello";
    head = NULL;

    printf("%d %d %s\n", *p, *q, text);
    if (head != NULL) {
        printf("%d\n", head->value);
    }
    return 0;
}
