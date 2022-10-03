// --------------------------------------------------------------------------
// TODO
//   - Normalize replacement tokens as well as any Unicode in the text before
//     comparison.  Use NFC normalization.
//
//   - Add support for late partial matches. Example:
//         Replacements
//             "ATTORNEY"     => "Mr. Smith"
//             "TORTLOCATION" => "Colorado Springs"
//         Text has
//             "THEY MET ATTORTLOCATION..." [sic]
//         Problem (Portmanteaus)
//             The partial match ATTOR will be invalidated and flushed when
//             we get ATTORT, and a new comparison will start with LOCATION.
//             The match for TORTLOCATION will never be found.
//         Solution
//             Brute Force: Iterate for each replacement individually.
//
//             Better: Upon failing a match with ti > 1, ungetc() all of the
//             raw buffer and increment uc0i IOT skip an output byte IOT avoid
//             the same partial match we just invalidated.
//
//             Best (?): Use a parallel array of size_ts to map each byte of
//             txt[] to the index in raw[] where it starts, i.e., the code
//             that results in txt[i] starts at raw[txtrawmap[i]]. Then,
//             when we invalidate a match, loop i from 0 to ti, checking if
//               !strncmp(&R->txt[i], R->srch_key[__], R->ti-i)
//             Then we can output raw[0] through raw[txtrawmap[i]], and
//             memmove() raw[] and txt[] (and adjust ri and ti) to begin
//             processing our new partial match.
// --------------------------------------------------------------------------


/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                        DECLARATIONS & MACROS                        ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include "rtfsed.h"
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
static void proc_cmd_uc(rtfobj *R);
static void proc_cmd_u(rtfobj *R);
static void proc_cmd_apostrophe(rtfobj *R);
static void proc_cmd_fonttbl(rtfobj *R);
static void proc_cmd_f(rtfobj *R);
static void proc_cmd_fcharset(rtfobj *R);
static void proc_cmd_cchs(rtfobj *R);
static void proc_cmd_shuntblock(rtfobj *R);
static void proc_cmd_newpar(rtfobj *R);
static void proc_cmd_newline(rtfobj *R);
static void proc_cmd_unknown(rtfobj *R);
static void push_attr(rtfobj *R);
static void pop_attr(rtfobj *R);
static int  pattern_match(rtfobj *R);
static void output_match(rtfobj *R);
static void output_raw(rtfobj *R);
static void add_to_txt(int c, rtfobj *R);
static void add_string_to_txt(const char *s, rtfobj *R);
static void add_to_cmd(int c, rtfobj *R);
static void add_to_raw(int c, rtfobj *R);
static void add_string_to_raw(const char *s, rtfobj *R);
static void reset_raw_buffer(rtfobj *R);
static void reset_txt_buffer(rtfobj *R);
static void reset_cmd_buffer(rtfobj *R);
static int32_t get_num_arg(const char *s);
static uint8_t get_hex_arg(const char *s);

#define RGX_MATCH(x, y)    (regexmatch((const unsigned char *) y, (const unsigned char *) x))
#define CHR_MATCH(x, y)    (x[0] == y && x[1] == 0)

#define fgetc(x)           getc_unlocked(x)
#define putc(x, y)         putc_unlocked(x, y)

/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////            CONSTRUCTOR/DESTRUCTOR FOR NEW RTF OBJECTS               ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

rtfobj *new_rtfobj(FILE *fin, FILE *fout, const char **srch) {
    FUNC_ENTER;
    size_t i;
    rtfobj *R = malloc(sizeof *R);

    if (!R) { LOG("Failed allocating new RTF Object."); FUNC_RETURN(NULL); }

    // Initialize the whole thing to zero
    memzero(R, sizeof *R);

    // Set up file streams
    R->fin  = fin;
    R->fout = fout;

    R->rawz = RAW_BUFFER_SIZE;
    R->txtz = TXT_BUFFER_SIZE;
    R->cmdz = CMD_BUFFER_SIZE;

    R->fonttbl_z = FONTTBL_SIZE;

    // Set up replacement dictionary
    for (i=0; srch[i] != NULL; i++);
    R->srchz = i/2;
    R->srch_match = 0UL;
    R->srch_key = malloc(R->srchz * sizeof *R->srch_key);
    R->srch_val = malloc(R->srchz * sizeof *R->srch_val);
    if ((!R->srch_key) || (!R->srch_val)) {
        delete_rtfobj(R);
        LOG("Out of memory allocating search key/value pointers!");
        FUNC_RETURN(NULL);
    }
    for (i=0; i < R->srchz; i++) {
        R->srch_key[i] = srch[2*i];
        R->srch_val[i] = srch[2*i+1];
    }

    R->attr = &R->topattr;

    FUNC_RETURN(R);
}



void delete_rtfobj(rtfobj *R) {
    FUNC_ENTER;
    if (R) {
        free(R->srch_key);
        free(R->srch_val);

        while (R->attr->outer) {
            pop_attr(R);
        }
    }
    free(R);
    FUNC_VOID_RETURN;
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////               MAIN PROCESSING LOOP & LIBRARY ENTRY                  ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

void rtfreplace(rtfobj *R) {
    FUNC_ENTER;
    int c;

    while ((c = fgetc(R->fin)) != EOF) {

        switch (c) {
            case '{':           dispatch_scope(c, R);      break;
            case '}':           dispatch_scope(c, R);      break;
            case '\\':          dispatch_command(R);       break;
            default:            dispatch_text(c, R);       break;
        }

        // MAYBETODO: Replace with a generalized callback system
        if (R->ti>0 && !R->attr->notxt) {
            switch (pattern_match(R)) {
                case PARTIAL:        /* Keep reading */        break;
                case MATCH:          output_match(R);
                                     reset_raw_buffer(R);
                                     reset_txt_buffer(R);      break;
                case NOMATCH:        output_raw(R);
                                     reset_raw_buffer(R);
                                     reset_txt_buffer(R);      break;
            }
        }

        if (R->fatalerr) { FUNC_VOID_DIE("Encountered a fatal error"); }
    }

    output_raw(R);
    FUNC_VOID_RETURN;
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                         DISPATCH FUNCTIONS                          ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static void dispatch_scope(int c, rtfobj *R) {
    FUNC_ENTER;
    add_to_raw(c, R);
    if      (c == '{')  push_attr(R);
    else if (c == '}')  pop_attr(R);
    FUNC_VOID_RETURN;
}



static void dispatch_command(rtfobj *R) {
    FUNC_ENTER;

    read_command(R);

    if (!R->attr->nocmd) proc_command(R);

    // ----- RAW/TXT BUFFER COORDINATION -----
    // We won't know whether to flush or keep raw data until after we look at
    // the text it contains. Deferring adding content to the raw output buffer
    // until AFTER the text or command is known keeps this cleaner. Then we
    // can, e.g., flush the existing raw buffer the first time text is added,
    // knowing that we won't be flushing the raw RTF code that *corresponds*
    // to the text we just added.
    add_string_to_raw(R->cmd, R);

    FUNC_VOID_RETURN;
}



static void dispatch_text(int c, rtfobj *R) {
    FUNC_ENTER;

    if (R->attr->notxt) { add_to_raw(c, R); FUNC_VOID_RETURN; }

    // Ignore newlines and carriage returns in RTF code. Consider tabs and
    // vertical tabs to be interchangeable with spaces. Treat everything else
    // literally.
    switch(c) {
        case '\r':
        case '\n':
            break;
        case '\t':
        case '\v':
            add_to_txt(' ', R);
            break;
        default:
            add_to_txt(c, R);
    }
    add_to_raw(c, R);
    FUNC_VOID_RETURN;
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                       PATTERN MATCH FUNCTION                        ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static int pattern_match(rtfobj *R) {
    FUNC_ENTER;
    size_t i, j;
    int partialmatch = 0;

    // Check each replacement pair for a match against R->txt
    for (i = 0; i < R->srchz; i++) {

        // Inline early-fail strcmp. Library function is slow on macOS.
        // j = first non-matching character (or index of \0 for both)
        for (j = 0;
                    R->txt[j] == R->srch_key[i][j]  &&
                    R->txt[j]           !=   '\0'    ;
                                                      j++);

        if (R->txt[j] == R->srch_key[i][j]) { // Therefore both '\0'
            // We reached the end of BOTH strings finding a mis-match, so...
            // the entirety of both strings must match.
            R->srch_match = i;
            FUNC_RETURN(MATCH);
        }
        else if (R->txt[j] == '\0') { // Not both '\0' but txt[j] is '\0'
            // Reached the end of the text buffer (the data we've read in so
            // far) without finding a mismatch, but haven't reached the end
            // of the replacement key. That means it's a partial match.
            partialmatch = 1;
        }
    }

    if (partialmatch) FUNC_RETURN(PARTIAL);
    FUNC_RETURN(NOMATCH);
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                  COMMAND DISPATCH HELPER FUNCTIONS                  ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static void read_command(rtfobj *R) {
    FUNC_ENTER;
    int c;

    reset_cmd_buffer(R);

    add_to_cmd('\\', R);

    if ((c=fgetc(R->fin)) == EOF) { LOG("Unexpected EOF"); R->fatalerr = EIO; FUNC_VOID_RETURN; }

    switch (c) {
        case '{':  // Escaped literal
        case '}':  // Escaped literal
        case '\\': // Escaped literal
        case '*':  // Special one-character command
        case '\n': // Line Feed --> newline
            add_to_cmd(c, R);
            break;
        case '\r': // Carriage Return --> newline
            add_to_cmd(c, R);

            // Check if next character is a newline, to avoid double newlines
            // for platforms with CRLF line terminators.
            if ((c=fgetc(R->fin)) == EOF) { FUNC_VOID_DIE("EOF after carriage return"); R->fatalerr = EIO; break; }

            if (c == '\n') {
                add_to_cmd(c, R);
            }
            else {
                ungetc(c, R->fin);
            }

            break;
        case '\'':
            add_to_cmd(c, R);

            if ((c=fgetc(R->fin)) == EOF) { LOG("EOF AFTER \\' command"); R->fatalerr = EIO; break; }
            add_to_cmd(c, R);

            if ((c=fgetc(R->fin)) == EOF) { LOG("EOF AFTER \\'_ command"); R->fatalerr = EIO; break; }
            add_to_cmd(c, R);

            break;
        default:
            if (!isalnum(c)) { LOG("Invalid command format..."); R->fatalerr = EINVAL; break; }
            add_to_cmd(c, R);

            // Greedily add input bytes to command buffer, so long as they're valid
            while(isalnum(c = fgetc(R->fin))) {
                add_to_cmd(c, R);
            }

            // Stopped getting valid command bytes. What now? Depends on if it's an EOF
            // or a space or something else. "Something else" is probably a backslash
            // for the next command, so we need to put it back on the input stream.
            if (c == EOF)         LOG("Unexpected EOF") && (R->fatalerr = EIO);
            else if (isspace(c))  add_to_cmd(c, R);
            else ungetc(c, R->fin);

            break;
    }
    FUNC_VOID_RETURN;
}



static void proc_command(rtfobj *R) {
    FUNC_ENTER;
    // Set pointer for internal use so that we don't have to deal with
    // backslashes at the beginning of our regular expression matching
    char *c = &R->cmd[1];

    if (0);
    else if (CHR_MATCH(c,'{'))                 proc_cmd_escapedliteral(R);
    else if (CHR_MATCH(c,'}'))                 proc_cmd_escapedliteral(R);
    else if (CHR_MATCH(c,'\\'))                proc_cmd_escapedliteral(R);
    else if (CHR_MATCH(c,'\r'))                proc_cmd_newline(R);
    else if (CHR_MATCH(c,'\n'))                proc_cmd_newline(R);
    else if (RGX_MATCH(c,"^\'\\x\\x"))         proc_cmd_apostrophe(R);
    else if (RGX_MATCH(c,"^u-?\\d+ ?$"))       proc_cmd_u(R);
    else if (RGX_MATCH(c,"^uc\\d+ ?$"))        proc_cmd_uc(R);
    else if (RGX_MATCH(c,"^f\\d+ ?$"))         proc_cmd_f(R);
    else if (RGX_MATCH(c,"^fcharset\\d+ ?$"))  proc_cmd_fcharset(R);
    else if (RGX_MATCH(c,"^cchs\\d+ ?$"))      proc_cmd_cchs(R);
    else if (RGX_MATCH(c,"^fonttbl ?$"))       proc_cmd_fonttbl(R);
    else if (RGX_MATCH(c,"^par ?$"))           proc_cmd_newpar(R);
    else if (RGX_MATCH(c,"^line ?$"))          proc_cmd_newline(R);
    else if (RGX_MATCH(c,"^pict ?$"))          proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^colortbl ?$"))      proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^stylesheet ?$"))    proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^operator ?$"))      proc_cmd_shuntblock(R);
    else if (RGX_MATCH(c,"^bin ?$"))           proc_cmd_shuntblock(R);
    else                                       proc_cmd_unknown(R);

    // If the command is \* then the entire block is optional, but...
    // if we then recognize the command, revoke any 'optional' status.
    if (RGX_MATCH(c, "^*$"))             R->attr->blkoptional = true;
    else                                 R->attr->blkoptional = false;

    FUNC_VOID_RETURN;
}



static inline void proc_cmd_escapedliteral(rtfobj *R) {
    FUNC_ENTER;
    add_to_txt(R->cmd[1], R);
    FUNC_VOID_RETURN;
}



static inline void proc_cmd_uc(rtfobj *R) {
    FUNC_ENTER;
    R->attr->uc = (size_t)get_num_arg(&R->cmd[1]);
    FUNC_VOID_RETURN;
}



static void proc_cmd_u(rtfobj *R) {
    FUNC_ENTER;
    int32_t arg = get_num_arg(&R->cmd[2]);

    // RTF 1.9 Spec: "Most RTF control words accept signed 16-bit numbers as
    // arguments. For these control words, Unicode values greater than 32767
    // are expressed as negative numbers. For example, the character code
    // U+F020 is given by \u-4064. To get -4064, convert F020 16 to decimal
    // (61472) and subtract 65536."
    if (arg < 0) arg = arg + 65536;

    // RTF 1.9 Spec: RTF supports "[s]urrogate pairs like \u-10187?\u-9138?"
    // Unicode 15.0, Sec. 23.6: The use of surrogate pairs... is formally
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

    R->attr->uc0i = R->attr->uc;
    FUNC_VOID_RETURN;
}



static void proc_cmd_apostrophe(rtfobj *R) {
    FUNC_ENTER;
    int32_t cdpt;
    uint8_t arg;
    const int32_t *mult = 0;

    DBUG("Processing |%s|", R->cmd);

    if (R->attr->uc0i) { R->attr->uc0i--; FUNC_VOID_RETURN; }

    arg = get_hex_arg(R->cmd);
    cdpt = cpgtou(R->attr->codepage, arg, &R->attr->xtra, &mult);

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
    else if (cdpt < 0) {
        DBUG("cpNONE or cpUNSP");
    }

    // Lastly, in the general case, convert the code point to UTF-8 and add
    // it to the text buffer.
    else {
        add_string_to_txt((const char*)utf8_from_cdpt(cdpt), R);
    }

    FUNC_VOID_RETURN;
}



static inline void proc_cmd_fonttbl(rtfobj *R) {
    FUNC_ENTER;
    R->attr->notxt = true;
    R->attr->fonttbl = true;
    R->attr->fonttbl_defn_idx = -1;
    FUNC_VOID_RETURN;
}



static void proc_cmd_f(rtfobj *R) {
    FUNC_ENTER;
    size_t i;
    int32_t arg = get_num_arg(&R->cmd[2]);

    // If defining a fonttbl, look for an existing font entry for f
    // If it can't be found, create it (assuming there's space)
    if (R->attr->fonttbl) {
        for (i = 0; i < R->fonttbl_n; i++)  if (R->fonttbl_f[i] == arg) break;

        if (i == R->fonttbl_n) {
            if (R->fonttbl_n + 1 < R->fonttbl_z) {
                R->fonttbl_n++;
                R->fonttbl_f[i]            = arg;
                R->fonttbl_charset[i]      = cpNONE;
                R->attr->fonttbl_defn_idx  = (int32_t)i;
            } else {
                FUNC_VOID_DIE("Out of fonttbl space, not defining f%d\n", arg);
            }
        }
    }

    else {
        for (i=0; i<R->fonttbl_n; i++) { if (R->fonttbl_f[i] == arg) break; }

        if (i < R->fonttbl_n) {
             R->attr->codepage = cpgfromcharsetnum(R->fonttbl_charset[i]);
        }
    }

    FUNC_VOID_RETURN;
}



static void proc_cmd_fcharset(rtfobj *R) {
    FUNC_ENTER;
    int32_t arg = get_num_arg(&R->cmd[9]);

    if (R->attr->fonttbl && R->attr->fonttbl_defn_idx >= 0) {
        R->fonttbl_charset[R->attr->fonttbl_defn_idx] = arg;
    }
    FUNC_VOID_RETURN;
}



static void proc_cmd_cchs(rtfobj *R) {
    FUNC_ENTER;
    int32_t arg = get_num_arg(&R->cmd[5]);

    R->attr->codepage = cpgfromcharsetnum(arg);
    FUNC_VOID_RETURN;
}



static void proc_cmd_shuntblock(rtfobj *R) {
    FUNC_ENTER;
    R->attr->nocmd = true;
    R->attr->notxt = true;
    FUNC_VOID_RETURN;
}



static void proc_cmd_newpar(rtfobj *R) {
    FUNC_ENTER;
    add_to_txt('\n', R);
    add_to_txt('\n', R);
    FUNC_VOID_RETURN;
}



static void proc_cmd_newline(rtfobj *R) {
    FUNC_ENTER;
    add_to_txt('\n', R);
    FUNC_VOID_RETURN;
}



static void proc_cmd_unknown(rtfobj *R) {
    FUNC_ENTER;
    if (R->attr->blkoptional) {
        R->attr->nocmd = true;
        R->attr->notxt = true;
    }
    FUNC_VOID_RETURN;
}






/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                     PROCESSING BUFFER FUNCTIONS                     ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static void add_to_raw(int c, rtfobj *R) {
    FUNC_ENTER;
    if (R->ri + 1 >= RAW_BUFFER_SIZE) {
        if (R->ti > 0) {
            DBUG("Exhausted raw buffer.");
            DBUG("R->ri = %zu. Last raw data: \'%s\'", R->ri, &R->raw[R->ri-80]);
            DBUG("No match within limits. Flushing buffers, attempting recovery");
            memzero(R->txt, R->ti);
            R->ti = 0UL;
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
        memzero(R->raw, R->ri);
        R->ri = 0UL;
    }
    R->raw[R->ri++] = (char)c;
    FUNC_VOID_RETURN;
}



static void add_to_txt(int c, rtfobj *R) {
    FUNC_ENTER;

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

    // If we have to skip byte due to a \uc directive, then decrement
    // the number of bytes to skip and return.
    if (R->attr->uc0i) { R->attr->uc0i--; FUNC_VOID_RETURN; }

    if (!deferred) {
        // ----- RAW/TXT BUFFER COORDINATION -----
        // Output the raw buffer when we FIRST add text to the text buffer.
        if (R->ri > 0   &&   R->ti == 0) {
            output_raw(R);
            reset_raw_buffer(R);
        }

        // Make sure we have enough space
        if (R->ti + 1   >=   R->txtz) {
            LOG("No match within limits. Flushing buffers. R->ti = %zu. Last txt data: \'%s\'", R->ti, &R->txt[R->ti-80]);
            output_raw(R);
            reset_raw_buffer(R);
            reset_txt_buffer(R);
        }

        // Map the current text start to the current raw location
        R->txtrawmap[ R->ti ]  =  R->ri;
    }

    if (c == 0) { deferred = 1; FUNC_VOID_RETURN; }

    R->txt[ R->ti++ ] = (char)c;
    deferred = 0;

    FUNC_VOID_RETURN;
}



static void add_to_cmd(int c, rtfobj *R) {
    FUNC_ENTER;
    assert(R->ci + 1 < R->cmdz);
    R->cmd[R->ci++] = (char)c;
    FUNC_VOID_RETURN;
}



static void add_string_to_txt(const char *s, rtfobj *R) {
    FUNC_ENTER;
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
    FUNC_VOID_RETURN;
}



static void add_string_to_raw(const char *s, rtfobj *R) {
    FUNC_ENTER;
    if (R->ri+strlen(s) >= R->rawz) {
        ////////////////////////////////////////////////////////////////////
        ////  RECOVERY CODE IS A HACK, NEED TO DO BETTER
        ////////////////////////////////////////////////////////////////////
        DBUG("Exhausted raw buffer.");
        DBUG("R->ri = %zu. Last raw data: \'%s\'", R->ri, &R->raw[R->ri-80]);
        output_raw(R);
        reset_raw_buffer(R);
        reset_txt_buffer(R);
        reset_cmd_buffer(R);
        ////////////////////////////////////////////////////////////////////
        ////  RECOVERY CODE IS A HACK, NEED TO DO BETTER
        ////////////////////////////////////////////////////////////////////
    }
    while (*s) R->raw[R->ri++] = *s++;
    FUNC_VOID_RETURN;
}



static void reset_raw_buffer(rtfobj *R) {
    FUNC_ENTER;
    memzero(R->raw, R->ri);
    R->ri = 0UL;
    FUNC_VOID_RETURN;
}



static void reset_txt_buffer(rtfobj *R) {
    FUNC_ENTER;
    memzero(R->txt, R->ti);
    R->ti = 0UL;
    FUNC_VOID_RETURN;
}



static void reset_cmd_buffer(rtfobj *R) {
    FUNC_ENTER;
    memzero(R->cmd, R->ci);
    R->ci = 0UL;
    FUNC_VOID_RETURN;
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                       BUFFER OUTPUT FUNCTIONS                       ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static void output_match(rtfobj *R) {
    FUNC_ENTER;
    size_t len = strlen(R->srch_val[R->srch_match]);

    // TODO: Convert non-ASCII to appropriate RTF \u and \' code
    fwrite(R->srch_val[R->srch_match], 1, len, R->fout);

    // Output the same number of braces we had in our raw buffer
    // Otherwise, if the text matching our replacement key had a font change
    // something, we could end up with mismatched braces and a mildly
    // corrupted RTF file.  NB: Be careful not to do this with escaped
    // literal brace characters.
    for (size_t i = 0UL; i < (R->ri - 1); i++) {
        if      (R->raw[i] == '\\' && R->raw[i+1] == '\\') i++;
        else if (R->raw[i] == '\\' && R->raw[i+1] == '{')  i++;
        else if (R->raw[i] == '\\' && R->raw[i+1] == '}')  i++;
        else if (R->raw[i] == '{') putc('{', R->fout);
        else if (R->raw[i] == '}') putc('}', R->fout);
    }
    FUNC_VOID_RETURN;
}



static void output_raw(rtfobj *R) {
    FUNC_ENTER;
    // Previously tried looping through the R->raw buffer and using
    // putc_unlocked() to speed up performance; however, it's actually faster
    // to use fwrite() most of the time -- not sure why.
    // for (size_t i = 0; i < R->ri; i++) putc_unlocked(R->raw[i], R->fout);
    // 10 iterations with fputc() takes .22 seconds +/- .01
    // 10 iterations with fwrite() takes .18 seconds +/- .01
    // I.e., fwrite() makes the program about 20% faster.
    fwrite(R->raw, 1, R->ri, R->fout);
    FUNC_VOID_RETURN;
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                      ATTRIBUTE STACK FUNCTIONS                      ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static void push_attr(rtfobj *R) {
    FUNC_ENTER;
    rtfattr *newattr = malloc(sizeof *newattr);

    if (!newattr) {
        R->fatalerr = ENOMEM;
        FUNC_VOID_DIE("Out-of-memory allocating new attribute scope.");
    }

    assert(R->attr != NULL);

    // "If an RTF scope delimiter character (that is, an opening or
    // closing brace) is encountered while scanning skippable data,
    // the skippable data is considered to end before the delimiter."
    R->attr->uc0i = 0;

    memmove(newattr, R->attr, sizeof *newattr);
    newattr->outer = R->attr;
    R->attr = newattr;

    FUNC_VOID_RETURN;
}



static void pop_attr(rtfobj *R) {
    FUNC_ENTER;
    rtfattr *oldattr = NULL;

    assert(R->attr != NULL);

    if (R->attr != &R->topattr) {
        oldattr = R->attr;        // Point it at the current attribute set
        R->attr = oldattr->outer; // Modify structure to point to outer scope
        free(oldattr);            // Delete the old attribute set
    }
    FUNC_VOID_RETURN;
}






/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                          PARSING FUNCTIONS                          ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static int32_t get_num_arg(const char *s) {
    FUNC_ENTER;
    const char *validchars="0123456789-";
    const char *p;
    int32_t retval;

    p = s;
    while (!strchr(validchars, *p)) p++;

    sscanf(p, "%"SCNd32, &retval);

    FUNC_RETURN(retval);
}



static uint8_t get_hex_arg(const char *s) {
    FUNC_ENTER;
    const char *validchars="0123456789ABCDEFabcdef";
    const char *p;
    uint32_t retval;

    p = s;
    while (!strchr(validchars, *p)) p++;

    sscanf(p, "%"SCNx32, &retval);

    FUNC_RETURN((uint8_t)retval);
}
