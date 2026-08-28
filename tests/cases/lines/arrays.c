/* A second shape: file-scope function bodies with array declarations, an if
   chain, and a for loop the converter lowers into Shalimar's counting form. */
#include <stdio.h>

int main(void)
{
    int a[4];
    int i;
    int total = 0;

    for (i = 0; i < 4; i++) {
        a[i] = i * i;
    }
    for (i = 0; i < 4; i++) {
        if (a[i] > 1) {
            total = total + a[i];
        }
    }
    printf("%d\n", total);
    return 0;
}
