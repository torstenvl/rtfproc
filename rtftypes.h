#ifndef __RTFTYPES_H
#define __RTFTYPES_H

#include <stdbool.h>
#include "cpgtou.h"

#define   RAW_BUFFER_SIZE   65536  // Raw processing buffer
#define   TXT_BUFFER_SIZE    2048  // Text processing buffer
#define   CMD_BUFFER_SIZE    2048  // Command processing buffer


#define   NOMATCH              -1
#define   PARTIAL               0
#define   MATCH                 1



typedef struct rtfattr {
    size_t uc;
    size_t skipbytes;
    cpg_t  cpg;
    bool   skippable; 

    struct rtfattr *outer;
} rtfattr;



typedef struct rtfobj {
    FILE         *  fin;          // RTF file-in
    FILE         *  fout;         // RTF file-out
    FILE         *  ftxt;         // Text file-out for debug
    
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
} rtfobj;



#endif
