/* __LINE__ and __FILE__ are supplied by the preprocessor and defined by
   nothing, so they reached the converter as identifiers no macro table knew
   and were emitted unchanged - a program naming a variable that does not
   exist, which only shc complained about, after the fact, in a file the
   author never wrote. Both are expanded now. */
int printf(char *fmt, ...);

int main(void)
{
    printf("%d\n", __LINE__);
    printf("%s\n", __FILE__);
    return 0;
}
