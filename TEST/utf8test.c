#include <stdio.h>
#include <assert.h>
#include <string.h>
// including the source file is necessary
// to conduct unit tests on static functions
#include "../rtfsed.c"

#define ENCOD(x,y) (utf8_from_cdpt(y,x))
#define CHECK(x,y) (assert(STREQ(x,y)))
#define STREQ(x,y) (!strcmp(x,y))

int main(void) {
    char s[5];

    ENCOD(s, 97);
    CHECK(s, u8"a");

    ENCOD(s, 0x1F600);
    CHECK(s, u8"😀");

    ENCOD(s, 0x1F608);
    CHECK(s, u8"😈");

    ENCOD(s, 0x2000B);
    CHECK(s, u8"𠀋");

    ENCOD(s, 0x2B8B8);
    CHECK(s, u8"𫢸");

    ENCOD(s, 0x0000);
    CHECK(s, u8"\0");

    ENCOD(s, (0 - 0x7FFFFFFF));
    if (!STREQ(s, u8"\0")) {
        fprintf(stderr, "Strings not equal! ");
        fprintf(stderr, "s is { ");
        for (size_t i = 0; s[i] != 0; i++) fprintf(stderr, "%x, ", s[i]);
        fprintf(stderr, "%x }\n", 0);
        fprintf(stderr, "                         ");
        exit(1);
    }

    ENCOD(s, (int32_t)0xFFFFFFFF);
    if (!STREQ(s, u8"\0")) {
        fprintf(stderr, "Strings not equal! \'%s\' =/= \'%s\'\n", s, u8"\0");
        fprintf(stderr, "s is { ");
        for (size_t i = 0; s[i] != 0; i++) fprintf(stderr, "%x, ", s[i]);
        fprintf(stderr, "%x }\n", 0);
        fprintf(stderr, "                         ");
        exit(1);
    }

    return 0;
}
