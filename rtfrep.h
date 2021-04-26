#ifndef RTFPROC_H
#define RTFPROC_H

#include <stdio.h>



#define   TKN_BUFFER_SIZE    512
#define   RAW_BUFFER_SIZE    8196
#define   MTC_BUFFER_SIZE    1024



typedef enum {
    ANSI,        ////
    MAC,         ////
    PC,          //// Same as CP_437
    PCA,         //// Same as CP_850
    CP_437,      // United States IBM
    CP_708,      // Arabic (ASMO 708)
    CP_709,      // Arabic (ASMO 449+, BCON V4)
    CP_710,      // Arabic (transparent Arabic)
    CP_711,      // Arabic (Nafitha Enhanced)
    CP_720,      // Arabic (transparent ASMO)
    CP_819,      // Windows 3.1 (United States and Western Europe)
    CP_850,      // IBM multilingual
    CP_852,      // Eastern European
    CP_860,      // Portuguese
    CP_862,      // Hebrew
    CP_863,      // French Canadian
    CP_864,      // Arabic
    CP_865,      // Norwegian
    CP_866,      // Soviet Union
    CP_874,      // Thai
    CP_932,      // Japanese
    CP_936,      // Simplified Chinese
    CP_949,      // Korean
    CP_950,      // Traditional Chinese
    CP_1250,     // Eastern European
    CP_1251,     // Cyrillic
    CP_1252,     // Western European
    CP_1253,     // Greek
    CP_1254,     // Turkish
    CP_1255,     // Hebrew
    CP_1256,     // Arabic
    CP_1257,     // Baltic
    CP_1258,     // Vietnamese
    CP_1361,     // Johab
    CP_10000,    // MAC Roman
    CP_10001,    // MAC Japan
    CP_10004,    // MAC Arabic
    CP_10005,    // MAC Hebrew
    CP_10006,    // MAC Greek
    CP_10007,    // MAC Cyrillic
    CP_10029,    // MAC Latin2
    CP_10081,    // MAC Turkish
    CP_57002,    // Devanagari
    CP_57003,    // Bengali
    CP_57004,    // Tamil
    CP_57005,    // Telugu
    CP_57006,    // Assamese
    CP_57007,    // Oriya
    CP_57008,    // Kannada
    CP_57009,    // Malayalam
    CP_57010,    // Gujarati
    CP_57011     // Punjabi
} cdpg_t;




typedef struct rtftoken {
    char contents[TKN_BUFFER_SIZE];
    enum { SCOPE_OPEN, SCOPE_CLOSE, IMM_CMD, COMPLEX_CMD, TEXT, FUCK } type;
} rtftoken;



typedef struct rtfattribs {
    size_t uc;
    cdpg_t cp;

    // NB: `parent` means the outer RTF attribute scope, which is down the
    // stack, ergo next in the linked list of RTF attribute sets.
    // For programmers who aren't used to thinking of a stack growing
    // downward, e.g., the function call stack, this may seem a bit odd.
    struct rtfattribs *parent;
} rtfattribs;



typedef struct rtfobj {
    FILE         *  is;
    FILE         *  os;
    rtfattribs   *  attr;
    char        **  dict;
    char            raw[RAW_BUFFER_SIZE];
    char            mtc[MTC_BUFFER_SIZE];
} rtfobj;



rtfobj *new_rtfobj(FILE *, FILE *, char **);

void    rtf_replace(rtfobj *);



#endif
