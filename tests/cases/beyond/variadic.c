/* A variadic function, which Shalimar has no form for.
 *
 * 'total' is declared and never defined on purpose. Defining it needs
 * va_list, and va_list is a typedef from a header this converter drops
 * unread - so 'va_list args;' reaches the parser as two identifiers and
 * comes back a syntax error rather than a refusal. That is a real limit of
 * the "headers are not converted" rule: it holds for calls, which C89 lets
 * you read without a prototype, and not for names a header would have
 * introduced as types.
 *
 * Worth knowing what the refusal actually says: the call is turned down for
 * being a name that is neither defined here nor one of the twenty builtins.
 * Nothing in the message mentions the '...', because nothing needs to - the
 * name has no definition to convert either way.
 */

#include <stdio.h>

int total(int count, ...);

int main(void)
{
    int n;

    n = total(2, 10, 20);
    printf("%d\n", n);
    return 0;
}
