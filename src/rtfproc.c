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


// TODO
//   - Refactor output function to output raw to fout AND output text to
//     ftxt, assuming one or both exist.
//   - Normalize replacement tokens as well as any Unicode in the text before
//     comparison.  Use NFC normalization.


/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                        DECLARATIONS & MACROS                        ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include "rtfproc.h"
#include "trex.h"
#include "cpgtou.h"
#include "utillib.h"

// Internal function declarations
static void dispatch_scope(int c, rtfobj *R);
static void dispatch_text(int c, rtfobj *R);
static void dispatch_command(rtfobj *R);
static void read_command(rtfobj *R);
static void proc_command(rtfobj *R);
static void proc_cmd_escapedliteral(rtfobj *R);
static void proc_cmd_specialstandin(rtfobj *R);
static void proc_cmd_uc(rtfobj *R);
static void proc_cmd_u(rtfobj *R);
static void proc_cmd_apostrophe(rtfobj *R);
static void proc_cmd_fonttbl(rtfobj *R);
static void proc_cmd_f(rtfobj *R);
static void proc_cmd_fcharset(rtfobj *R);
static void proc_cmd_cchs(rtfobj *R);
static void proc_cmd_deff(rtfobj *R);
static void proc_cmd_shuntblock(rtfobj *R);
static void proc_cmd_newpar(rtfobj *R);
static void proc_cmd_newline(rtfobj *R);
static void proc_cmd_unknown(rtfobj *R);
static void push_attr(rtfobj *R);
static void pop_attr(rtfobj *R);
static int  pattern_match(rtfobj *R);
static void output_match(rtfobj *R);
static void output_raw_by(rtfobj *R, size_t amt);
static void add_to_txt(int c, rtfobj *R);
static void add_string_to_txt(const char *s, rtfobj *R);
static void add_to_cmd(int c, rtfobj *R);
static void add_to_raw(int c, rtfobj *R);
static void add_cmdstring_to_raw(const char *s, rtfobj *R);
static int32_t get_num_arg(const char *s);
static uint8_t get_hex_arg(const char *s);

#define RGX_MATCH(x, y)      (rexmatch((const unsigned char *) y, (const unsigned char *) x))
#define CHR_MATCH(x, y)      (x[0] == y && x[1] == 0)

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
#define fgetc(x)             getc_unlocked(x)
#define fputc(x, y)          putc_unlocked(x, y)
#endif

#define reset_raw_buffer(R)  reset_raw_buffer_by(R, R->ri)
#define reset_txt_buffer(R)  reset_txt_buffer_by(R, R->ti)
#define reset_cmd_buffer(R)  reset_cmd_buffer_by(R, R->ci)
#define output_raw(R)        output_raw_by(R, R->ri)



/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////            CONSTRUCTOR/DESTRUCTOR FOR NEW RTF OBJECTS               ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

rtfobj *new_rtfobj(FILE *fin, FILE *fout, FILE *ftxt) {
    rtfobj *R;

    BEGIN_FUNCTION

    R = malloc(sizeof *R);

    if (!R) { FAIL(NULL, "Failed allocating new RTF Object."); }

    // Initialize the whole thing to zero
    memzero(R, sizeof *R);

    // Set up file streams
    R->fin  = fin;
    R->fout = fout;
    R->ftxt = ftxt;

    setvbuf(R->fin,  NULL, _IOFBF, (1<<21));
    if (R->fout) setvbuf(R->fout, NULL, _IOFBF, (1<<21));
    if (R->ftxt) setvbuf(R->ftxt, NULL, _IOFBF, (1<<21));

    R->rawz = RAW_BUFFER_SIZE;
    R->txtz = TXT_BUFFER_SIZE;
    R->cmdz = CMD_BUFFER_SIZE;

    R->fonttbl_z = FONTTBL_SIZE;
    R->defaultfont = -1;

    // RTF 1.9 Spec: "A default of 1 should be assumed if no \ucN keyword has
    // been seen in the current or outer scopes." BUGFIX 22 December 2022.
    //
    // This caused issues with certain insane choices made by LibreOffice,
    // such as using FUCKING SHIFTJIS to display double angle brackets /
    // guillemets.  But... it should be fixed regardless.
    (R->topattr).uc = 1;

    R->attr = &R->topattr;

    RETURN(R);
}



size_t add_rtfobj_replacements(rtfobj *R, const char **replacements) {
    size_t i;
    size_t newitems;
    char  **newkey;
    char  **newval;

    BEGIN_FUNCTION

    // Find out how many new elements there are
    for (newitems=0; replacements[newitems] != NULL; newitems++);
    newitems = newitems / 2;

    // Try to reallocate both arrays
    newkey = realloc(R->srch_key, (R->srchz + newitems) * sizeof *R->srch_key);
    if (!newkey) {
        R->fatalerr = ENOMEM;
        FAIL(0UL, "Out of memory allocating search key/value pointers!");
    } else {
        R->srch_key = newkey;
        newval = realloc(R->srch_val, (R->srchz + newitems) * sizeof *R->srch_val);
        if (!newval) {
            R->fatalerr = ENOMEM;
            FAIL(0UL, "Out of memory allocating search key/value pointers!");
        } else {
            R->srch_val = newval;
        }
    }

    // Add the new elements to the arrays
    for (i = 0; i < newitems; i++) {
        // TODO: Check returns and make sure its safe
        R->srch_key[R->srchz + i] = strdup(replacements[2*i]);
        R->srch_val[R->srchz + i] = strdup(replacements[2*i+1]);
        // R->srch_key[R->srchz + i] = replacements[2*i];
        // R->srch_val[R->srchz + i] = replacements[2*i+1];
    }

    // Set the new size of the arrays
    R->srchz += newitems;

    RETURN(newitems);
}


size_t add_one_rtfobj_replacement(rtfobj *R, const char *key, const char *val) {
    char  **newkey = NULL;
    char  **newval = NULL;
    char   *tmpkey = NULL;
    char   *tmpval = NULL;
    size_t  i;

    BEGIN_FUNCTION

    if (!key) RETURN(0UL);
    if (!val) RETURN(0UL);

    for (i = 0; i < R->srchz; i++) {
        if (!strcmp(key, R->srch_key[i])) {
            tmpval = strdup(val);
            if (!tmpval) {
                R->fatalerr = ENOMEM;
                FAIL(0UL, "Out of memory copying replacement value string!");
            } else {
                free(R->srch_val[i]);
                R->srch_val[i] = tmpval;
                RETURN(1UL);
            }
        }
    }

    // Try to reallocate both arrays
    newkey = realloc(R->srch_key, (R->srchz + 1) * sizeof *R->srch_key);
    if (!newkey) {
        R->fatalerr = ENOMEM;
        FAIL(0UL, "Out of memory allocating search key pointers!");
    }

    R->srch_key = newkey;

    newval = realloc(R->srch_val, (R->srchz + 1) * sizeof *R->srch_val);
    if (!newval) {
        R->fatalerr = ENOMEM;
        FAIL(0UL, "Out of memory allocating replacement value pointers!");
    }

    R->srch_val = newval;

    tmpkey = strdup(key);

    if (!tmpkey) {
        R->fatalerr = ENOMEM;
        FAIL(0UL, "Out of memory copying search key string!");
    }

    tmpval = strdup(val);

    if (!tmpval) {
        R->fatalerr = ENOMEM;
        free(tmpkey);
        FAIL(0UL, "Out of memory copying replacement value string!");
    }

    R->srch_key[R->srchz] = tmpkey;
    R->srch_val[R->srchz] = tmpval;

    R->srchz += 1;

    RETURN(1UL);
}





void delete_rtfobj(rtfobj *R) {

    BEGIN_FUNCTION

    if (R) {
        for (size_t i=0; i < R->srchz; i++) {
            free(R->srch_key[i]);
            free(R->srch_val[i]);
        }
        free(R->srch_key);
        free(R->srch_val);
        while (R->attr->outer) pop_attr(R);
    }
    free(R);

    RETURN();
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////            MAIN PROCESSING LOOPS & LIBRARY ENTRY POINTS             ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

void rtfreplace(rtfobj *R) {
    int c;

    BEGIN_FUNCTION

    while ((c = fgetc(R->fin)) != EOF) {

        switch (c) {
            case '{':           dispatch_scope(c, R);      break;
            case '}':           dispatch_scope(c, R);      break;
            case '\\':          dispatch_command(R);       break;
            default:            dispatch_text(c, R);       break;
        }

        pattern_match(R);

        if (R->fatalerr) {
            output_raw(R);
            FAIL(VOID, "Encountered a fatal error");
        }
    }

    output_raw(R);

    RETURN();
}


void rtfprocess(rtfobj *R, void (*processfunction)(rtfobj *, void *, int), void *passthru) {
    int c;

    BEGIN_FUNCTION

    processfunction(R, passthru, RTF_PROC_START);
    while ((c = fgetc(R->fin)) != EOF) {
        switch (c) {
            case '{':           dispatch_scope(c, R);      break;
            case '}':           dispatch_scope(c, R);      break;
            case '\\':          dispatch_command(R);       break;
            default:            dispatch_text(c, R);       break;
        }

        processfunction(R, passthru, RTF_PROC_STEP);
        if (R->fatalerr) {
            processfunction(R, passthru, RTF_PROC_END);
            FAIL(VOID, "Encountered a fatal error");
        }
    }
    processfunction(R, passthru, RTF_PROC_END);

    RETURN();
}


/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                         DISPATCH FUNCTIONS                          ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static void dispatch_scope(int c, rtfobj *R) {
    BEGIN_FUNCTION

    add_to_raw(c, R);
    if      (c == '{')  push_attr(R);
    else if (c == '}')  pop_attr(R);

    RETURN();
}



static void dispatch_command(rtfobj *R) {
    BEGIN_FUNCTION

    read_command(R);

    if (!R->attr->nocmd) proc_command(R);

    // ----- RAW/TXT BUFFER COORDINATION -----
    // We won't know whether to flush or keep raw data until after we look at
    // the text it contains. Deferring adding content to the raw output buffer
    // until AFTER the text or command is known keeps this cleaner. Then we
    // can, e.g., flush the existing raw buffer the first time text is added,
    // knowing that we won't be flushing the raw RTF code that *corresponds*
    // to the text we just added.
    add_cmdstring_to_raw(R->cmd, R);

    RETURN();
}



static void dispatch_text(int c, rtfobj *R) {
    BEGIN_FUNCTION

    if (R->attr->notxt) { add_to_raw(c, R); RETURN(); }

    // Ignore newlines and carriage returns in RTF code. Consider tabs and
    // vertical tabs to be interchangeable with spaces. Treat everything else
    // literally.
    switch(c) {
        case '\r':
        case '\n':
            break;
        case '\t':
            add_to_txt(0x09, R);
            break;
        case '\v':
            add_to_txt(' ', R);
            break;
        default:
            add_to_txt(c, R);
    }
    add_to_raw(c, R);

    RETURN();
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                       PATTERN MATCH FUNCTION                        ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static int pattern_match(rtfobj *R) {
    size_t offset;
    size_t curkey;
    size_t i;

    BEGIN_FUNCTION

    if (R->ti < 1 || R->attr->notxt) RETURN(PARTIAL);

    for (offset = 0; offset < R->ti; offset++) {
        for (curkey = 0; curkey < R->srchz; curkey++) {

            // Inline early-fail strcmp. Library function is slow on macOS.
            // i = first non-matching character (or index of \0 for both)
            for (i = 0; R->txt[offset+i] == R->srch_key[curkey][i] && R->txt[offset+i] != '\0'; i++);

            if (R->txt[offset+i] == R->srch_key[curkey][i]) { // Therefore both '\0'
                // We reached the end of BOTH strings without finding a mis-match, so...
                // the entirety of both strings at this offset must match.
                if (offset > 0) {
                    output_raw_by(R, R->txtrawmap[ offset ]);
                    reset_raw_buffer_by(R, R->txtrawmap[ offset ]);
                    reset_txt_buffer_by(R, offset);
                }
                R->srch_match = curkey;
                output_match(R);
                reset_raw_buffer(R);
                reset_txt_buffer(R);
                RETURN(MATCH);
            }
            else if (R->txt[offset+i] == '\0') { // Not both '\0' but txt[j] is '\0'
                // Reached the end of the text buffer (the data we've read in so
                // far) without finding a mismatch, but haven't reached the end
                // of the replacement key. That means it's a partial match.
                if (offset > 0) {
                    output_raw_by(R, R->txtrawmap[ offset ]);
                    reset_raw_buffer_by(R, R->txtrawmap[ offset ]);
                    reset_txt_buffer_by(R, offset);
                }
                RETURN(PARTIAL);
            }
        }
    }

    output_raw(R);
    reset_raw_buffer(R);
    reset_txt_buffer(R);

    RETURN(NOMATCH);
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                  COMMAND DISPATCH HELPER FUNCTIONS                  ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static void read_command(rtfobj *R) {
    int c;

    BEGIN_FUNCTION

    reset_cmd_buffer(R);

    add_to_cmd('\\', R);

    if ((c=fgetc(R->fin)) == EOF) { R->fatalerr = EIO; FAIL(VOID, "Unexpected EOF"); }

    switch (c) {
        case '{':  // Escaped literal
        case '}':  // Escaped literal
        case '\\': // Escaped literal
        case '~':  // Special one-character stand-in
        case '_':  // Special one-character stand-in
        case '-':  // Special one-character stand-in
        case '*':  // Special one-character command
        case '\n': // Line Feed --> newline
            add_to_cmd(c, R);
            break;
        case '\r': // Carriage Return --> newline
            add_to_cmd(c, R);

            // Check if next character is a newline, to avoid double newlines
            // for platforms with CRLF line terminators.
            if ((c=fgetc(R->fin)) == EOF) { R->fatalerr = EIO; FAIL(VOID, "EOF after \\\\r"); }

            if (c == '\n') {
                add_to_cmd(c, R);
            }
            else {
                ungetc(c, R->fin);
            }

            break;
        case '\'':
            add_to_cmd(c, R);

            if ((c=fgetc(R->fin)) == EOF) { R->fatalerr = EIO; FAIL(VOID, "EOF AFTER \\' command"); }
            add_to_cmd(c, R);

            if ((c=fgetc(R->fin)) == EOF) { R->fatalerr = EIO; FAIL(VOID, "EOF AFTER \\'_ command"); }
            add_to_cmd(c, R);

            break;
        default:
            if (!isalnum(c)) { R->fatalerr = EINVAL; FAIL(VOID, "Invalid command format |%s|...", R->cmd); }
            add_to_cmd(c, R);

            // Greedily add input bytes to command buffer, so long as they're valid
            while ((c = fgetc(R->fin)) != EOF) {
                if (isalnum(c) || c == '-') add_to_cmd(c, R);
                else break;
            }

            // Stopped getting valid command bytes. What now? Depends on if it's an EOF
            // or a space or something else. "Something else" is probably a backslash
            // for the next command, so we need to put it back on the input stream.
            if (c == EOF)         LOG("Unexpected EOF") && (R->fatalerr = EIO);
            else if (isspace(c))  add_to_cmd(c, R);
            else                  ungetc(c, R->fin);

            break;
    }

    RETURN();
}



static void proc_command(rtfobj *R) {
    // Set pointer for internal use so that we don't have to deal with
    // backslashes at the beginning of our regular expression matching
    char *c = &R->cmd[1];

    BEGIN_FUNCTION

    if (0);
    else if (CHR_MATCH(c,'{'))                   proc_cmd_escapedliteral(R);
    else if (CHR_MATCH(c,'}'))                   proc_cmd_escapedliteral(R);
    else if (CHR_MATCH(c,'\\'))                  proc_cmd_escapedliteral(R);
    else if (CHR_MATCH(c,'~'))                   proc_cmd_specialstandin(R);
    else if (CHR_MATCH(c,'_'))                   proc_cmd_specialstandin(R);
    else if (CHR_MATCH(c,'-'))                   proc_cmd_specialstandin(R);
    else if (CHR_MATCH(c,'\r'))                  proc_cmd_newline(R);
    else if (CHR_MATCH(c,'\n'))                  proc_cmd_newline(R);
    else if (RGX_MATCH(c,"^\'\\x\\x"))           proc_cmd_apostrophe(R);
    else if (RGX_MATCH(c,"^u-?\\d+\\s?$"))       proc_cmd_u(R);
    else if (RGX_MATCH(c,"^uc\\d+\\s?$"))        proc_cmd_uc(R);
    else if (RGX_MATCH(c,"^f\\d+\\s?$"))         proc_cmd_f(R);
    else if (RGX_MATCH(c,"^fcharset\\d+\\s?$"))  proc_cmd_fcharset(R);
    else if (RGX_MATCH(c,"^cchs\\d+\\s?$"))      proc_cmd_cchs(R);
    else if (RGX_MATCH(c,"^deff\\d+\\s?$"))      proc_cmd_deff(R);
    else if (RGX_MATCH(c,"^fonttbl\\s?$"))       proc_cmd_fonttbl(R);
    else if (RGX_MATCH(c,"^par\\s?$"))           proc_cmd_newpar(R);
    else if (RGX_MATCH(c,"^line\\s?$"))          proc_cmd_newline(R);
    else if (RGX_MATCH(c,"^pict\\s?$"))          proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^colortbl\\s?$"))      proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^stylesheet\\s?$"))    proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^title\\s?$"))         proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^subject\\s?$"))       proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^author\\s?$"))        proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^manager\\s?$"))       proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^company\\s?$"))       proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^operator\\s?$"))      proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^category\\s?$"))      proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^keywords\\s?$"))      proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^comment\\s?$"))       proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^doccomm\\s?$"))       proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^hlinkbase\\s?$"))     proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^creatim\\s?$"))       proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^revtim\\s?$"))        proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^printim\\s?$"))       proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^buptim\\s?$"))        proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^userprops\\s?$"))     proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^bin\\s?$"))           proc_cmd_shuntblock(R);
    else                                         proc_cmd_unknown(R);

    // If the command is \* then the entire block is optional, but...
    // if we then recognize the command, revoke any 'optional' status.
    if (RGX_MATCH(c, "^*$"))             R->attr->blkoptional = true;
    else                                 R->attr->blkoptional = false;

    RETURN();
}



static inline void proc_cmd_escapedliteral(rtfobj *R) {
    BEGIN_FUNCTION

    add_to_txt(R->cmd[1], R);

    RETURN();
}



static inline void proc_cmd_specialstandin(rtfobj *R) {
    BEGIN_FUNCTION

    int32_t cdpt = 0;

    if (R->cmd[1] == '~') cdpt = 0x00A0; // Non-breaking space
    if (R->cmd[1] == '_') cdpt = 0x2011; // Non-breaking hyphen
    if (R->cmd[1] == '-') cdpt = 0x00AD; // Soft hyphen

    if (cdpt) add_string_to_txt((const char*)utf8_from_cdpt(cdpt), R);

    RETURN();
}



static inline void proc_cmd_uc(rtfobj *R) {
    BEGIN_FUNCTION

    R->attr->uc = (size_t)get_num_arg(&R->cmd[1]);

    RETURN();
}



static void proc_cmd_u(rtfobj *R) {
    int32_t arg;

    BEGIN_FUNCTION

    arg = get_num_arg(&R->cmd[2]);

    // RTF 1.9 Spec: "Most RTF control words accept signed 16-bit numbers as
    // arguments. For these control words, Unicode values greater than 32767
    // are expressed as negative numbers. For example, the character code
    // U+F020 is given by \u-4064. To get -4064, convert F020 16 to decimal
    // (61472) and subtract 65536."
    if (arg < 0) arg = arg + 65536;

    // RTF 1.9 Spec: RTF supports "[s]urrogate pairs like \u-10187?\u-9138?"
    // Unicode 15.0, Sec. 23.6: "The use of surrogate pairs... is formally
    // equivalent to [what is] defined in ISO/IEC 10646.... The high-surrogate
    // code points are [in]] range U+D800..U+DBFF [and are always] the first
    // element of a surrogate pair.... The low-surrogate code points are [in]
    // range U+DC00..U+DFFF [and are] always the second element...."
    if (0xD800 <= arg && arg <= 0xDBFF) {
        // Argument is in the high surrogate range.  We need to tell the
        // add_to_txt function that we're adding text (so it can do text
        // setup, etc.), even though we don't have anything to add to the
        // text buffer yet.
        R->highsurrogate = arg;
        add_to_txt(0, R);
    } else if (0xDC00 <= arg && arg <= 0xDFFF) {
        // Argument is in the low surrogate range
        int32_t cdpt = cdpt_from_utf16((uint16_t)R->highsurrogate, (uint16_t)arg);
        add_string_to_txt((const char*)utf8_from_cdpt(cdpt), R);
    } else {
        // Argument is likely in the Basic Multilingual Plane
        add_string_to_txt((const char*)utf8_from_cdpt(arg), R);
    }

    R->attr->uccountdown = R->attr->uc;

    RETURN();
}



static void proc_cmd_apostrophe(rtfobj *R) {
    int32_t cdpt;
    uint8_t arg;
    const int32_t *mult = NULL;

    BEGIN_FUNCTION

    cpg_t cpg = (R->attr->codepage)?(R->attr->codepage):(R->documentcodepage);

    if (R->attr->uccountdown) { R->attr->uccountdown--; RETURN(); }

    arg = get_hex_arg(R->cmd);
    cdpt = cpgtou(cpg, arg, &R->attr->xtra, &mult);

    // If we are starting a double-byte sequence, then we need to tell the
    // add_to_txt function that we're adding text (so it can do text setup,
    // etc.), even though we don't actually have anything to add to the
    // text buffer yet.
    if (cdpt == cpDBSQ) {
        add_to_txt(0, R);
    }

    // If we have multiple code points to add, add them one at a time.
    else if (cdpt == cpMULT) {
        for (; *mult != 0; mult++) {
            add_string_to_txt((const char*)utf8_from_cdpt(*mult), R);
        }
    }

    // If there's no match or the code page is unsupported, don't do anything
    // except emit a diagnostic message (if and only if we're in debug mode).
    else if (cdpt == cpNONE) {
        DBUG("cpNONE: Code point %d does not exist code page %d", arg, cpg);
    }
    else if (cdpt == cpUNSP) {
        DBUG("cpUNSP: Code page %d is unsupported", cpg);
    }

    // Lastly, in the general case, convert the code point to UTF-8 and add
    // it to the text buffer.
    else {
        add_string_to_txt((const char*)utf8_from_cdpt(cdpt), R);
    }

    RETURN();
}



static inline void proc_cmd_fonttbl(rtfobj *R) {
    BEGIN_FUNCTION

    R->attr->notxt = true;
    R->attr->fonttbl = true;
    R->attr->fonttbl_defn_idx = -1;

    RETURN();
}



static void proc_cmd_f(rtfobj *R) {
    size_t i;
    int32_t arg;

    BEGIN_FUNCTION

    arg = get_num_arg(&R->cmd[2]);

    if (R->attr->fonttbl) {
        // If defining a fonttbl, look for an existing font entry for f
        for (i = 0; i < R->fonttbl_n; i++)  if (R->fonttbl_f[i] == arg) break;

        // If we're at the end (i.e., not found), then add it
        if (i == R->fonttbl_n) {
            if (R->fonttbl_n + 1 < R->fonttbl_z) {
                R->fonttbl_n++;
                R->fonttbl_f[i]            = arg;
                R->fonttbl_charset[i]      = cpNONE;
                R->attr->fonttbl_defn_idx  = (int32_t)i;
            } else {
                FAIL(VOID, "Out of fonttbl space, not defining f%d\n", arg);
            }
        }

        // Otherwise, we're redefining one; set the definition index
        else {
            R->attr->fonttbl_defn_idx  = (int32_t)i;
        }
    }

    else {
        for (i=0; i<R->fonttbl_n; i++) { if (R->fonttbl_f[i] == arg) break; }

        if (i < R->fonttbl_n) {
             R->attr->codepage = cpgfromcharsetnum(R->fonttbl_charset[i]);
        }
    }

    RETURN();
}



static inline void proc_cmd_fcharset(rtfobj *R) {
    int32_t arg;

    BEGIN_FUNCTION

    arg = get_num_arg(&R->cmd[9]);

    // If we're defining a font table and have a valid definition index...
    if (R->attr->fonttbl && R->attr->fonttbl_defn_idx >= 0) {
        // Add the appropriate character set to the font table
        R->fonttbl_charset[R->attr->fonttbl_defn_idx] = arg;
        // Also, if we're dealing with the default font...
        if (R->fonttbl_f[R->attr->fonttbl_defn_idx] == R->defaultfont) {
            // Set that font's character set as the document code page.
            R->documentcodepage = cpgfromcharsetnum(arg);
        }

    }

    RETURN();
}



static inline void proc_cmd_cchs(rtfobj *R) {
    int32_t arg;

    BEGIN_FUNCTION

    arg = get_num_arg(&R->cmd[5]);
    R->attr->codepage = cpgfromcharsetnum(arg);

    RETURN();
}



static inline void proc_cmd_deff(rtfobj *R) {
    int32_t arg;

    BEGIN_FUNCTION

    arg = get_num_arg(&R->cmd[5]);
    R->defaultfont = arg;

    RETURN();
}


static inline void proc_cmd_shuntblock(rtfobj *R) {
    BEGIN_FUNCTION

    R->attr->nocmd = true;
    R->attr->notxt = true;

    RETURN();
}



static inline void proc_cmd_newpar(rtfobj *R) {
    BEGIN_FUNCTION

    add_to_txt('\n', R);
    add_to_txt('\n', R);

    RETURN();
}



static inline void proc_cmd_newline(rtfobj *R) {
    BEGIN_FUNCTION

    add_to_txt('\n', R);

    RETURN();
}



static inline void proc_cmd_unknown(rtfobj *R) {
    BEGIN_FUNCTION

    if (R->attr->blkoptional) {
        R->attr->nocmd = true;
        R->attr->notxt = true;
    }

    RETURN();
}






/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                     PROCESSING BUFFER FUNCTIONS                     ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static void add_to_raw(int c, rtfobj *R) {
    BEGIN_FUNCTION

    if (R->ri + 1 >= RAW_BUFFER_SIZE) {
        if (R->ti > 0) {
            DBUG("Exhausted raw buffer.");
            DBUG("R->ri = %zu. Last raw data: \'%s\'", R->ri, &R->raw[R->ri-80]);
            DBUG("No match within limits. Flushing buffers, attempting recovery");
            reset_txt_buffer(R);
        }
        ////////////////////////////////////////////////////////////////////
        ////                         WALL OF SHAME                      ////
        ////////////////////////////////////////////////////////////////////
        // output_raw(R);
        // R->ri = 0UL;
        // R->ti = 0UL;
        // R->ci = 0UL;
        // memzero(R->raw, R->ri);     <-- I just fucking reset R->ri
        // memzero(R->txt, R->ti);     <-- I just fucking reset R->ti
        // memzero(R->cmd, R->ci);     <-- I just fucking reset R->ci
        // I wasn't zeroing out my buffers AT ALL
        // No wonder I was getting weird data corruption issues
        ////////////////////////////////////////////////////////////////////
        output_raw(R);
        reset_raw_buffer(R);
    }

    R->raw[R->ri++] = (char)c;

    RETURN();
}



static void add_to_txt(int c, rtfobj *R) {
    // Sometimes we have started adding text, but the first byte can't be
    // added to the actual text buffer (e.g., the first surrogate in a UTF-16
    // surrogate pair, or the first byte in a double-byte sequence).
    // In that case, we can pass in a value of 0 for c. The function will set
    // the deferred flag and do text setup (clean the buffers, output the
    // pre-text raw RTF, set the txtrawmap[], etc.). But no text will be
    // added to the text buffer.
    // On the next run, with the deferred flag set, this function will know
    // that adding text to the text buffer has been deferred, and it shouldn't
    // make assumptions based on the fact that the text buffer is empty, i.e.,
    // shouldn't run text setup again.
    _Thread_local static int deferred = 0;

    BEGIN_FUNCTION

    // If we have to skip byte due to a \uc directive, then decrement
    // the number of bytes to skip and return.
    if (R->attr->uccountdown) { R->attr->uccountdown--; RETURN(); }

    if (!deferred) {
        // ----- RAW/TXT BUFFER COORDINATION -----
        // Output the raw buffer when we FIRST add text to the text buffer.
        if (R->ri > 0   &&   R->ti == 0) {
            output_raw(R);
            reset_raw_buffer(R);
        }

        // Make sure we have enough space
        if (R->ti + 1   >=   R->txtz) {
            // LOG("No match within limits. Flushing buffers. R->ti = %zu. Last txt data: \'%s\'", R->ti, &R->txt[R->ti-80]);
            output_raw(R);
            reset_raw_buffer(R);
            reset_txt_buffer(R);
        }

        // Map the current text start to the current raw location
        R->txtrawmap[ R->ti ]  =  R->ri;
    }

    if (c == 0) {
        deferred = 1;
        RETURN();
    }

    R->txt[ R->ti++ ] = (char)c;
    deferred = 0;

    RETURN();
}



static void add_to_cmd(int c, rtfobj *R) {
    BEGIN_FUNCTION

    assert(R->ci + 1 < R->cmdz);
    R->cmd[R->ci++] = (char)c;

    RETURN();
}



static void add_string_to_txt(const char *s, rtfobj *R) {
    BEGIN_FUNCTION

    if (R->ti+strlen(s) >= R->txtz) {
        ////////////////////////////////////////////////////////////////////
        ////  RECOVERY CODE IS A HACK, NEED TO DO BETTER
        ////////////////////////////////////////////////////////////////////
        LOG("No match within limits. Flushing buffers. R->ti = %zu. Last txt data: \'%s\'", R->ti, &R->txt[R->ti-80]);
        output_raw(R);
        reset_raw_buffer(R);
        reset_txt_buffer(R);
        ////////////////////////////////////////////////////////////////////
        ////  RECOVERY CODE IS A HACK, NEED TO DO BETTER
        ////////////////////////////////////////////////////////////////////
    }

    while (*s) add_to_txt((int)(*s++), R);

    RETURN();
}



static void add_cmdstring_to_raw(const char *s, rtfobj *R) {
    BEGIN_FUNCTION

    if (R->ri+strlen(s) >= R->rawz) {
        DBUG("Exhausted raw buffer.");
        DBUG("R->ri = %zu. Last raw data: \'%s\'", R->ri, &R->raw[R->ri-80]);
        output_raw(R);
        reset_raw_buffer(R);

        // Any text pattern we had is now invalid, so we need to clear that
        // as well
        reset_txt_buffer(R);

        // However, it's very important that we not remove the command that
        // we're trying to add to the raw buffer!!!! (BUGFIX 22 December 2022)
        // If we do, then we'll end up with a raw buffer that doesn't match
        // what we've input from the original file, leading to semi-random
        // corruptions.
        // DO NOT reset_cmd_buffer(R);
    }

    while (*s) R->raw[R->ri++] = *s++;

    RETURN();
}



void reset_raw_buffer_by(rtfobj *R, size_t amt) {
    size_t remaining;

    BEGIN_FUNCTION

    remaining = R->ri - amt;
    memmove(R->raw, &R->raw[amt], remaining);
    R->ri = remaining;
    memzero(&R->raw[remaining], amt);

    RETURN();
}



void reset_txt_buffer_by(rtfobj *R, size_t amt) {
    size_t remaining;

    BEGIN_FUNCTION

    if (R->ftxt) fwrite(R->txt, 1, amt, R->ftxt);

    remaining = R->ti - amt;
    memmove(R->txt, &R->txt[amt], remaining);
    R->ti = remaining;
    memzero(&R->txt[remaining], amt);

    RETURN();
}



void reset_cmd_buffer_by(rtfobj *R, size_t amt) {
    size_t remaining;

    BEGIN_FUNCTION

    remaining = R->ci - amt;
    memmove(R->cmd, &R->cmd[amt], remaining);
    R->ci = remaining;
    memzero(&R->cmd[remaining], amt);

    RETURN();
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                       BUFFER OUTPUT FUNCTIONS                       ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static void output_match(rtfobj *R) {
    const unsigned char *output;
    int32_t cdpt;
    uint16_t hi;
    uint16_t lo;
    int16_t hi_out;
    int16_t lo_out;
    size_t i;
    int nbraces;

    BEGIN_FUNCTION

    output = (const unsigned char *)R->srch_val[R->srch_match];

    if (!R->fout) RETURN();

    for (i = 0; output[i] != '\0'; ) {
        if (output[i] < 128) {
            fputc((int)output[i], R->fout);
            i++;
        } else {
            // Value outside of ASCII range, convert to UTF-16
            cdpt = cdpt_from_utf8(output + i);
            utf16_from_cdpt(cdpt, &hi, &lo);

            // Accommodate RTF's stupid signed integer version
            hi_out = (int16_t)((hi > 32767)?(hi - 65536):hi);
            lo_out = (int16_t)((lo > 32767)?(lo - 65536):lo);

            // Print out the UTF-16 code point (including surrogate pair,
            // if applicable)
            if (hi_out != 0) fprintf(R->fout, "{\\uc0 \\u%d}", hi_out);
            fprintf(R->fout, "{\\uc0 \\u%d}", lo_out);

            // Loop through the rest of any continuation bytes
            i = i + 1;
            while (output[i] != 0  &&  output[i]>>6 == 2) {
                i++;
            }
        }
    }

    // Originally, we output the same # of braces as in our raw buffer
    // However, unnecessary scope changes can cause fonts, etc. to be reset
    // in the following text. Now, we output the NET number of braces, i.e.,
    // any }{}{ nonsense will result in zero braces being output, and the
    // original active control words will remain active.
    //
    // NB: Be careful not to do this with escaped literal brace characters.
    nbraces = 0;
    for (i = 0; i < (R->ri - 1); i++) {
        if      (R->raw[i] == '\\' && R->raw[i+1] == '\\') i++;
        else if (R->raw[i] == '\\' && R->raw[i+1] == '{')  i++;
        else if (R->raw[i] == '\\' && R->raw[i+1] == '}')  i++;
        else if (R->raw[i] == '{') nbraces++;
        else if (R->raw[i] == '}') nbraces--;
    }
    while (nbraces > 0)   {  fputc('{', R->fout);  nbraces--;  }
    while (nbraces < 0)   {  fputc('}', R->fout);  nbraces++;  }

    RETURN();
}



static void output_raw_by(rtfobj *R, size_t amt) {
    BEGIN_FUNCTION

    // Previously tried looping through the R->raw buffer and using
    // putc_unlocked() to speed up performance; however, it's actually faster
    // to use fwrite() most of the time -- not sure why.
    // for (size_t i = 0; i < amt; i++) putc_unlocked(R->raw[i], R->fout);
    // 10 iterations with fputc() takes .22 seconds +/- .01
    // 10 iterations with fwrite() takes .18 seconds +/- .01
    // I.e., fwrite() makes the program about 20% faster.
    if (!R->fout) RETURN();
    fwrite(R->raw, 1, amt, R->fout);

    RETURN();
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                      ATTRIBUTE STACK FUNCTIONS                      ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static void push_attr(rtfobj *R) {
    rtfattr *newattr;

    BEGIN_FUNCTION

    newattr = malloc(sizeof *newattr);

    if (!newattr) {
        R->fatalerr = ENOMEM;
        FAIL(VOID, "Out-of-memory allocating new attribute scope.");
    }

    assert(R->attr != NULL);

    // "If an RTF scope delimiter character (that is, an opening or
    // closing brace) is encountered while scanning skippable data,
    // the skippable data is considered to end before the delimiter."
    R->attr->uccountdown = 0;

    memmove(newattr, R->attr, sizeof *newattr);
    newattr->outer = R->attr;
    R->attr = newattr;

    RETURN();
}



static void pop_attr(rtfobj *R) {
    rtfattr *oldattr = NULL;

    BEGIN_FUNCTION

    assert(R->attr != NULL);

    if (R->attr != &R->topattr) {
        oldattr = R->attr;        // Point it at the current attribute set
        R->attr = oldattr->outer; // Modify structure to point to outer scope
        free(oldattr);            // Delete the old attribute set
    }

    RETURN();
}






/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                          PARSING FUNCTIONS                          ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static int32_t get_num_arg(const char *s) {
    const char *validchars="0123456789-";
    const char *p;
    int32_t retval;

    BEGIN_FUNCTION

    p = s;
    while (!strchr(validchars, *p)) p++;

    sscanf(p, "%"SCNd32, &retval);

    RETURN(retval);
}



static uint8_t get_hex_arg(const char *s) {
    const char *validchars="0123456789ABCDEFabcdef";
    const char *p;
    uint32_t retval;

    BEGIN_FUNCTION

    p = s;
    while (!strchr(validchars, *p)) p++;

    sscanf(p, "%"SCNx32, &retval);

    RETURN((uint8_t)retval);
}
