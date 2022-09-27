#include <stdio.h>
#include <assert.h>
#include <string.h>
// including the source file is necessary
// to conduct unit tests on static functions
#include "../rtfsed.c"

#define ENCOD(x)   (utf8_from_cdpt(x))
#define CHECK(x,y) (assert(STREQ(utf8_from_cdpt(x),y)))
#define STREQ(x,y) (!strcmp((const char *)x,(const char *)y))

int main(void) {
    unsigned char *s;

    CHECK(97,      u8"a");
    CHECK(0x1F600, u8"😀");
    CHECK(0x1F608, u8"😈");
    CHECK(0x2000B, u8"𠀋");
    CHECK(0x2B8B8, u8"𫢸");
    CHECK(0x0000,  u8"\0");

    s = ENCOD(0 - 0x7FFFFFFF);
    if (!STREQ(s, u8"\0")) {
        fprintf(stderr, "Strings not equal! ");
        fprintf(stderr, "s is { ");
        for (size_t i = 0; s[i] != 0; i++) fprintf(stderr, "%x, ", s[i]);
        fprintf(stderr, "%x }\n", 0);
        fprintf(stderr, "                         ");
        exit(1);
    }

    s = ENCOD((int32_t)0xFFFFFFFF);
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
