#ifndef RTFSED_H__
#define RTFSED_H__


// INCLUDES
#include <stdio.h>
#include <stdbool.h>
#include "STATIC/cpgtou.h"



// DEFINES
#define   RAW_BUFFER_SIZE   65536  // Raw processing buffer
#define   TXT_BUFFER_SIZE    2048  // Text processing buffer
#define   CMD_BUFFER_SIZE    2048  // Command processing buffer

#define   NOMATCH              -1
#define   PARTIAL               0
#define   MATCH                 1


//STRUCTURE DEFINITIONS

// TODO: font table entries
// 
// typedef struct ftentry {
//     int64_t k;
//     int64_t v;
// } dictentry;
// 
// typedef struct font {
//     int8_t  N;
//     cpg_t   cpg;
// } font;



typedef struct rtfattr {
    size_t uc;
    size_t skipbytes;   // WTF does this mean?
    bool   skippable;   // WTF does this mean?
    cpg_t  cpg;
    // TODO: Indicate currently active fonttbl entry

    struct rtfattr *outer;
} rtfattr;



typedef struct rtfobj {
    FILE         *  fin;          // RTF file-in
    FILE         *  fout;         // RTF file-out

    int             fatalerr;     // Cf. ERRNO

    size_t          ri;           // raw/txt/cmd iterators, buffer
    size_t          ti;           // sizes, and buffers
    size_t          ci;
    size_t          rawz;
    size_t          txtz;
    size_t          cmdz;
    char            raw[RAW_BUFFER_SIZE];
    char            txt[TXT_BUFFER_SIZE];
    char            cmd[CMD_BUFFER_SIZE];

    size_t          srchz;        // srch & replace pairs
    size_t          srch_max_keylen;
    size_t          srch_match; 
    const char  **  srch_key;
    const char  **  srch_val;
    
    rtfattr      *  attr;         // Attribute stack
} rtfobj;



// FUNCTION DECLARATIONS
rtfobj *new_rtfobj(FILE *fin, FILE *fout, const char **replacements);
void    delete_rtfobj(rtfobj *R);
void    rtfreplace(rtfobj *R);



#endif
