/*═════════════════════════════════════════════════════════════════════════*\
║                                                                           ║
║  RTFPROC - RTF Processing Library                                         ║
║  Copyright (c) 2019-2023, Joshua Lee Ockert                               ║
║                                                                           ║
║  THIS WORK IS PROVIDED 'AS IS' WITH NO WARRANTY OF ANY KIND. THE IMPLIED  ║
║  WARRANTIES OF MERCHANTABILITY, FITNESS, NON-INFRINGEMENT, AND TITLE ARE  ║
║  EXPRESSLY DISCLAIMED. NO AUTHOR SHALL BE LIABLE UNDER ANY THEORY OF LAW  ║
║  FOR ANY DAMAGES OF ANY KIND RESULTING FROM THE USE OF THIS WORK.         ║
║                                                                           ║
║  Permission to use, copy, modify, and/or distribute this work for any     ║
║  purpose is hereby granted, provided this notice appears in all copies.   ║
║                                                                           ║
\*═════════════════════════════════════════════════════════════════════════*/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "utillib.h"

#ifndef NDEBUG
#define STREQ(x,y) (!strcmp((const char *)x,(const char *)y))
#define CHECK(x,y) (assert(STREQ(utf8_from_cdpt(x),y)))
#else
#define CHECK(x,y) ((void)0)
#endif

int main(void) {
    // unsigned char *s;

    CHECK(97,      u8"a");
    // s = utf8_from_cdpt(0x1F600);
    // fprintf(stderr, "Code point 0x1F600 is |%s|\n", s);
    // fprintf(stderr, "s is { ");
    // for (size_t i = 0; s[i] != 0; i++) fprintf(stderr, "%x, ", s[i]);
    // fprintf(stderr, "%x }\n", 0);
    // fprintf(stderr, "                         ");
    CHECK(0x1F600, u8"😀");
    CHECK(0x1F608, u8"😈");
    CHECK(0x2000B, u8"𠀋");
    CHECK(0x2B8B8, u8"𫢸");
    CHECK(0x0000,  u8"\0");

    CHECK(0 - 0x7FFFFFFF, u8"\0");

    CHECK((int32_t)0xFFFFFFFF, u8"\0");

    return 0;
}
