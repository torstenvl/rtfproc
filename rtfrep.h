#ifndef __RTFPROC_H
#define __RTFPROC_H

#include <stdio.h>
#include <stdbool.h>
#include "rtfi18n.h"


#define   RAW_BUFFER_SIZE    8196
#define   TXT_BUFFER_SIZE    2048
#define   CMD_BUFFER_SIZE    2048


// typedef struct rtftoken {
//     char contents[TKN_BUFFER_SIZE];
//     enum { SCOPE_OPEN, SCOPE_CLOSE, IMM_CMD, COMPLEX_CMD, TEXT, FUCK } type;
// } rtftoken;



typedef struct rtfattr {
    size_t uc;
    cdpg_t cp;
    bool   txtable;

    struct rtfattr *parent;
} rtfattr;



typedef struct rtfobj {
    FILE         *  fin;
    FILE         *  fout;
    rtfattr      *  attr;
    char        **  dict;

    size_t          ri;
    size_t          ti;
    size_t          ci;

    size_t          rawz;
    size_t          txtz;
    size_t          cmdz;

    char            raw[RAW_BUFFER_SIZE];
    char            txt[TXT_BUFFER_SIZE];
    char            cmd[CMD_BUFFER_SIZE];
} rtfobj;



rtfobj *new_rtfobj(FILE *, FILE *, char **);

void    rtfreplace(rtfobj *);



#endif
