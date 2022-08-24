#include <stdio.h>
#include <assert.h>
#include <string.h>
// including the source file is necessary
// to conduct unit tests on static functions
#include "../rtfsed.c"

#define ENCOD(x,y) (encode_utf8(y,x))
#define CHECK(x,y) (assert(!strcmp(x,y)))

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

    return 0;
}
