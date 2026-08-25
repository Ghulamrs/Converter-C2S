/* Arrays of one and two dimensions, and an array parameter.
 *
 * Shalimar arrays carry their own extents, so 'total(a, 5)' passes a length
 * C needs and Shalimar already knows; what matters here is that the two
 * agree on every element either way.
 */

#include <stdio.h>

int total(int v[], int n)
{
    int k;
    int s = 0;

    for (k = 0; k < n; k++) {
        s = s + v[k];
    }
    return s;
}

int main(void)
{
    int a[5];
    int grid[2][3];
    int i;
    int j;

    for (i = 0; i < 5; i++) {
        a[i] = i * i;
    }
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            grid[i][j] = i * 10 + j;
        }
    }

    printf("total %d\n", total(a, 5));
    for (i = 0; i < 2; i++) {
        printf("row %d %d %d\n", grid[i][0], grid[i][1], grid[i][2]);
    }
    return 0;
}
