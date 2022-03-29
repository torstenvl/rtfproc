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


// STRUCTURE DEFINITIONS
typedef struct rtfattr {
    size_t uc;
    size_t skipbytes;
    cpg_t  cpg;
    // Indicate which font
    bool   skippable; 

    struct rtfattr *outer;
} rtfattr;


typedef struct font {
    int8_t  N;
    cpg_t   cpg;
} font;


typedef struct rtfobj {
    FILE         *  fin;          // RTF file-in
    FILE         *  fout;         // RTF file-out
    
    size_t          dictz;
    size_t          dict_max_keylen;
    size_t          dict_match; 
    const char  **  dict_key;
    const char  **  dict_val;
    
    rtfattr      *  attr;         // Attribute stack

    size_t          ri;           // Raw processing iterator
    size_t          rawz;         // Raw processing buffer size
    char            raw[RAW_BUFFER_SIZE];  // Raw processing buffer
    
    size_t          ti;           // Text processing iterator
    size_t          txtz;         // Text processing buffer size
    char            txt[TXT_BUFFER_SIZE];  // Text processing buffer

    size_t          ci;           // Command processing iterator
    size_t          cmdz;         // Command processing buffer size
    char            cmd[CMD_BUFFER_SIZE];  // Command processing buffer

    int             fatalerr;     // Quick way to trigger exit of rtf 
                                  // processing without taking down the whole
                                  // program. 

    // FONT TABLE
} rtfobj;



// FUNCTION DECLARATIONS
rtfobj *new_rtfobj(FILE *fin, FILE *fout, const char **replacements);
void    delete_rtfobj(rtfobj *R);

void    rtfreplace(rtfobj *R);



#endif
