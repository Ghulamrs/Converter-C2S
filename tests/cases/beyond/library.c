/* The C library, of which Shalimar has twenty builtins and no more.
 *
 * <math.h> is the one header with real coverage. Everything here - memory,
 * strings, characters, and above all input, which Shalimar has no facility
 * for whatsoever - has no translation and must say so by name.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int main(void)
{
    char buffer[32];
    int n;
    int *heap;

    strcpy(buffer, "abc");
    n = (int)strlen(buffer);
    if (strcmp(buffer, "abc") == 0) {
        n = n + 1;
    }
    buffer[0] = (char)toupper(buffer[0]);

    heap = (int *)malloc(sizeof(int) * 4);
    free(heap);

    scanf("%d", &n);

    printf("%s %d\n", buffer, n);
    return 0;
}
