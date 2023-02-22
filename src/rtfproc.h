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

#ifndef RTFPROC_H__
#define RTFPROC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include "cpgtou.h"



#define   RAW_BUFFER_SIZE   65536  // Raw processing buffer
#define   TXT_BUFFER_SIZE    2048  // Text processing buffer
#define   CMD_BUFFER_SIZE    2048  // Command processing buffer
#define   FONTTBL_SIZE        512  // Number of fonttbl entries

#define   NOMATCH              -1
#define   PARTIAL               0
#define   MATCH                 1

#define   RTF_PROC_START       -1
#define   RTF_PROC_STEP         0
#define   RTF_PROC_END          1


// ATTRIBUTE STACK ENTRY
typedef struct rtfattr {
    size_t          uc;          // # bytes of text to skip after \u cmd
    size_t          uccountdown; // Iterate from uc to 0 after \u cmd

    bool            fonttbl;     // Currently defining a font table
    bool            blkoptional; // Block is optional due to \*
    bool            nocmd;       // Do not process commands in this block
    bool            notxt;       // Do not process data as text in this block
    int32_t         fonttbl_defn_idx;

    uint8_t         xtra;

    cpg_t           codepage;    // Principally for WordPad, Pages, TextEdit

    struct          rtfattr *outer;
} rtfattr;


// RTF OBJECT
typedef struct rtfobj {
    // Processing variables
    FILE         *  fin;          // RTF file-in
    FILE         *  fout;         // RTF file-out
    FILE         *  ftxt;         // RTF text file-out
    size_t          ri;           // raw/txt/cmd iterators, buffer
    size_t          ti;           // sizes, and buffers
    size_t          ci;
    size_t          rawz;
    size_t          txtz;
    size_t          cmdz;
    char            raw[RAW_BUFFER_SIZE];
    char            txt[TXT_BUFFER_SIZE];
    char            cmd[CMD_BUFFER_SIZE];
    size_t          txtrawmap[TXT_BUFFER_SIZE];

    // Font table and code page
    size_t          fonttbl_n; 
    size_t          fonttbl_z;
    int32_t         fonttbl_f[FONTTBL_SIZE];
    int32_t         fonttbl_charset[FONTTBL_SIZE];
    int32_t         defaultfont;
    cpg_t           documentcodepage;

    // Current/temporary status variables
    int             fatalerr;     // Cf. ERRNO. E.g., EIO, ENOMEM, etc.
    int32_t         highsurrogate;

    // Search & replace tokens (key-value pair)
    size_t          srchz;        // srch & replace pairs
    size_t          srch_match; 
    char        **  srch_key;
    char        **  srch_val;
    
    // Attribute stack
    rtfattr         topattr;      // Attribute stack
    rtfattr      *  attr;         // Attribute stack
} rtfobj;



// FUNCTION DECLARATIONS
rtfobj *new_rtfobj(FILE *fin, FILE *fout, FILE *ftxt);
size_t  add_rtfobj_replacements(rtfobj *R, const char **replacements);
size_t  add_one_rtfobj_replacement(rtfobj *R, const char *key, const char *val);
void    delete_rtfobj(rtfobj *R);
void    rtfreplace(rtfobj *R);
void    rtfprocess(rtfobj *R, void (*processfunction)(rtfobj *, void *, int), void *data);

void    reset_raw_buffer_by(rtfobj *R, size_t amt);
void    reset_txt_buffer_by(rtfobj *R, size_t amt);
void    reset_cmd_buffer_by(rtfobj *R, size_t amt);


#ifdef __cplusplus
}
#endif

#endif
