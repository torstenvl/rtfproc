// ---------------------------------------------------------------------------- 
// LIMITATIONS
//
//   - We use a fixed buffer sizes for raw RTF code, text, and RTF control
//     words. Right now this is brittle.  TODO: If we reach end of buffer
//     then emit message, output raw RTF code, and reset buffers.  This 
//     prevents memory errors and corruption and, at worst, will just fail
//     to work with overly long tokens. 
//
// TODO
//   - Add attributes for \ansicpgN, font tables with \fcharsetN and/or
//     \cpgN, and \cchsN. Interpret \'xx according to current character set
//     or code page. 
//
//   - Normalize replacement tokens as well as any Unicode in the text before
//     comparison.  Use NFC normalization.
//   
//   - Add additional areas where we skip blocks for large data types. 
// ---------------------------------------------------------------------------- 





//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                       DECLARATIONS & MACROS                          ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include "rtfsed.h"
#include "STATIC/re.h"

// Internal function declarations
static void dispatch_scope(int c, rtfobj *R);
static void dispatch_text(int c, rtfobj *R);
static void dispatch_command(int c, rtfobj *R);
static void read_command(int c, rtfobj *R);
static void proc_command(rtfobj *R);
static void push_attr(rtfobj *R);
static void pop_attr(rtfobj *R);
static int  pattern_match(rtfobj *R);
static void output_match(rtfobj *R);
static void output_raw(rtfobj *R);
static void add_to_txt(int c, rtfobj *R);
static void add_to_cmd(int c, rtfobj *R);
static void add_to_raw(int c, rtfobj *R);
static void reset_buffers(rtfobj *R);
static void reset_raw_buffer(rtfobj *R);
static void reset_txt_buffer(rtfobj *R);
static void reset_cmd_buffer(rtfobj *R);
static void skip_thru_block(rtfobj *R);
static void encode_utf8(int32_t c, char *buf);
static void memzero(void *const p, const size_t n);

// Macros
#define FLOG        (stderr)
#define LOGPFX      (fprintf(FLOG, "In rtfsed.c:%s() on line %d: ", \
                     __func__, __LINE__))
#define LOGBDY(...) (fprintf(FLOG, __VA_ARGS__ ))
#define LOGSFX      (fprintf(FLOG, "\n" ))
#define LOG(...)    (LOGPFX && LOGBDY(__VA_ARGS__) && LOGSFX)

#define ABORT(...)      {LOG(__VA_ARGS__); return;}

#define MAX(x, y)   ((x>y)?x:y)

// #define DEBUG
#ifdef DEBUG
    #define DBUG(...) (LOG(__VA_ARGS__))
#else
    #define DBUG(...) ((void)0)
#endif





//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////             CONSTRUCTOR/DESTRUCTOR FOR NEW RTF OBJECTS               ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////

rtfobj *new_rtfobj(FILE *fin, FILE *fout, const char **dict) {
    size_t i;
    rtfobj *R = malloc(sizeof *R);

    if (!R) {
        LOG("Failed allocating new RTF Object.");
        return NULL;
    }

    // Set up file streams    
    R->fin = fin;
    R->fout = fout;

    // Set up replacement dictionary
    for (i=0; dict[i] != NULL; i++); 
    R->dictz = i/2;
    R->dict_max_keylen = 0UL;
    R->dict_match = 0UL; 

    // Allocate two lists of pointers
    R->dict_key = malloc(R->dictz * sizeof *R->dict_key);
    R->dict_val = malloc(R->dictz * sizeof *R->dict_val);
    if ((!R->dict_key) || (!R->dict_val)) { 
        delete_rtfobj(R);
        LOG("Out of memory allocating key/value pointers!");
        return NULL; 
    }

    // Assign those pointers to the strings we were passed. Also track
    // which key has the longest length (to make comparisons more efficient).
    for (i=0; i < R->dictz; i++) {
        R->dict_key[i] = dict[2*i];
        R->dict_val[i] = dict[2*i+1];
        R->dict_max_keylen = MAX(R->dict_max_keylen, strlen(R->dict_key[i]));
    }

    R->attr = NULL;
    R->ri = 0UL;
    R->rawz = RAW_BUFFER_SIZE;
    memzero(R->raw, RAW_BUFFER_SIZE);

    R->ti = 0UL;
    R->txtz = TXT_BUFFER_SIZE;
    memzero(R->txt, TXT_BUFFER_SIZE);

    R->ci = 0UL;
    R->cmdz = CMD_BUFFER_SIZE;
    memzero(R->cmd, CMD_BUFFER_SIZE);
    
    R->fatalerr = 0;

    return R;
}

void delete_rtfobj(rtfobj *R) {
    if (R) {
        free(R->dict_key);
        free(R->dict_val);
        while (R->attr) {
            pop_attr(R);
        }
    }
    free(R);
}





//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                           MAIN LOGIC LOOP                            ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////

void rtfreplace(rtfobj *R) {
    int c;

    // Loop until we reach the end of the input file
    while ((c = fgetc(R->fin)) != EOF) {
        switch (c) {
            case '{':           dispatch_scope(c, R);      break;
            case '}':           dispatch_scope(c, R);      break;
            case '\\':          dispatch_command(c, R);    break;
            default:            dispatch_text(c, R);       break;
        }
        if (R->fatalerr) ABORT("Encountered a fatal error");

        switch (pattern_match(R)) {
            case PARTIAL:        /* Keep reading */        break;
            case MATCH:          output_match(R);          break;
            case NOMATCH:        output_raw(R);            break;
        }
        if (R->fatalerr) ABORT("Encountered a fatal error");
    }

    output_raw(R);
}





//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                         DISPATCH FUNCTIONS                           ////
////                                                                      ////
////     Deal with different categories of action from the main loop      ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////

static void dispatch_scope(int c, rtfobj *R) {
    R->raw[R->ri++] = (char)c;
    if (c == '{') {          push_attr(R);     } 
    else if (c == '}') {     pop_attr(R);      }
}


static void dispatch_command(int c, rtfobj *R) {
    read_command(c, R);
    proc_command(R);
}


static void dispatch_text(int c, rtfobj *R) {
    // Ignore newlines and carriage returns in RTF code. Consider tabs and
    // vertical tabs to be interchangeable with spaces. Treat everything else
    // literally. 
    if (c == '\n') {          add_to_raw(c, R);                              }
    else if (c == '\r') {     add_to_raw(c, R);                              } 
    else if (c == '\t') {     add_to_raw(c, R);      add_to_txt(' ', R);     }
    else if (c == '\v') {     add_to_raw(c, R);      add_to_txt(' ', R);     }
    else {                    add_to_raw(c, R);      add_to_txt( c,  R);     }
}





//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                       PATTERN MATCH FUNCTION                         ////
////                                                                      ////
////     Check to see whether we have a match for any of the tokens in    ////
////     our replacement dictionary.  If NOMATCH, then we should only     ////
////     invalidate as much as we are SURE could not be part of some      ////
////     other match. Cf. KMP/Boyer-Moore.                                ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////
static int pattern_match(rtfobj *R) {
    size_t i;

    // All raw RTF code without a text counterpart is per se a NOMATCH
    if (R->ri > 0 && R->ti == 0) return NOMATCH;

    // Check for complete matches
    for (i = 0; i < R->dictz; i++) {
        if (!strcmp(R->txt, R->dict_key[i])) {
            R->dict_match = i;
            return MATCH;
        }
    }

    // Check for partial matches at current offset
    for (i = 0; i < R->dictz; i++) {
        if (!strncmp(R->txt, R->dict_key[i], R->ti)) {
            return PARTIAL;
        }
    }

    // TODO:  Deal with late partial matches. Example:
    //             "ATTORNEY" => "Mr. Smith"
    //             "TORTLOCATION" => "December 12, 2021"
    //        Text has 
    //             "THEY CONVENED ATTORTLOCATION..." [sic]
    //        Current design would fail to replace TORTLOCATION because
    //        the entirety of the raw buffer corresponding to "ATTORT" would
    //        be discarded (since the partial match for ATTORNEY was
    //        invalidated).
    //
    //        Instead, we could iterate through and discovery that 
    //             strncmp(&R->txt[2], R->dict_key[?], R->ti-2)
    //
    //        However, unclear what to do then since we don't track what
    //        part of the text buffer corresponds to what part of the raw
    //        buffer. 

    // DBUG("R->txt is \'%s\'\n", R->txt);
    return NOMATCH;
}





//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                  COMMAND DISPATCH HELPER FUNCTIONS                   ////
////                                                                      ////
////     Read commands and process a limited subset of them (most are     ////
////     not needed for the purposes of simple search-and-replace.        ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////

static void read_command(int c, rtfobj *R) {
    R->ci = 0UL;
    memzero(R->cmd, R->cmdz);

    add_to_cmd(c, R);
    add_to_raw(c, R);
    assert((c = fgetc(R->fin)) != EOF);

    switch (c) {
        case '{':  // Escaped literal
        case '}':  // Escaped literal
        case '\\': // Escaped literal
            add_to_cmd(c, R);
            add_to_raw(c, R);
            break;
        case '*':  // Special one-character command
            add_to_cmd(c, R);
            add_to_raw(c, R);
            break;
        case '\r':
            add_to_cmd('p', R);
            add_to_cmd('a', R);
            add_to_cmd('r', R);
            add_to_raw('\r', R);
            break;
        case '\n':
            add_to_cmd('p', R);
            add_to_cmd('a', R);
            add_to_cmd('r', R);
            add_to_raw('\n', R);
            break;
        case '\'':
            add_to_cmd(c, R);
            add_to_raw(c, R);
            // Attempt to get two hex digits
            c = fgetc(R->fin);
            if (c==EOF) { LOG("Unexpected EOF"); R->fatalerr = EIO; break; }
            add_to_cmd(c, R);
            add_to_raw(c, R);
            c = fgetc(R->fin);
            if (c==EOF) { LOG("Unexpected EOF"); R->fatalerr = EIO; break; }
            add_to_cmd(c, R);
            add_to_raw(c, R);
            break;
        default:
            if (!isalnum(c)) { LOG("Invalid command format..."); break; }
            add_to_cmd(c, R);
            add_to_raw(c, R);

            while(isalnum(c = fgetc(R->fin))) {
                add_to_cmd(c, R);
                add_to_raw(c, R);
            }

            if (c==EOF) { LOG("Unexpected EOF"); R->fatalerr = EIO; break; }

            if (isspace(c)) {     add_to_raw(c, R);      }
            else {                ungetc(c, R->fin);     }
            break;
    }
}


static void proc_command(rtfobj *R) {
    int reml;
    char *cmd = &R->cmd[1];

    DBUG("Processing RTF control word \'%s\'", R->cmd);

    // COMMAND CATEGORY: ESCAPED CHARACTER LITERALS
    if (
    (!strcmp(cmd, "\\"))  ||
    (!strcmp(cmd, "{"))   ||
    (!strcmp(cmd, "}"))   ){
        add_to_txt(cmd[0], R);
        R->attr->skippable = false;
        return;
    } 

    // COMMAND: MARK NEXT COMMAND AS OPTIONAL
    if (!strcmp(cmd, "*")) {
        R->attr->skippable = true;
        return;
    } 

    // COMMAND: SET THE UNICODE SKIP-BYTE COUNT
    if (re_match("uc[0-9]*$", cmd, &reml) > -1) {
        DBUG("Got a \\ucN control word: \'%s\'", R->cmd);
        R->attr->uc = strtoul(&cmd[2], NULL, 10);
        R->attr->skippable = false;
        return;
    } 

    // COMMAND: UNICODE CODE POINT SPECIFICATION
    if (re_match("u[0-9]*$", cmd, &reml) > -1) {
        char utf8[8] = { 0 };
        char *p;

        DBUG("Got a \\uN control word: \'%s\'", R->cmd);
        encode_utf8(strtoul(&cmd[1], NULL, 10), utf8);
        R->attr->skipbytes = 0;
        for (p = utf8; *p != '\0'; p++) add_to_txt(*p, R);
        R->attr->skipbytes = R->attr->uc;
        // TODO: Implement skipbytes in add_to_txt function

        R->attr->skippable = false;
        return;
    } 
    
    // COMMAND: CODE PAGE CODE POINT SPECIFICATION
    if (re_match("\'[0-9A-Fa-f][0-9A-Fa-f]$", cmd, &reml) > -1) {
        char utf8[5] = { '\0' };
        int32_t c;
        char *p;
        
        DBUG("Got a \\'xx control word: \'%s\'", R->cmd);
        c = strtoul(&R->cmd[2], NULL, 16);
        encode_utf8(c, utf8);
        for (p = utf8; *p != '\0'; p++) add_to_txt(*p, R);

        R->attr->skippable = false;
        return;
    } 

    // COMMAND CATEGORY: ALL THE SKIPPABLE SHIT I DON'T CARE ABOUT WITH 
    // WEIRD PARAMETER DATA THAT WILL FUCK UP MY TEXT-MATCHING
    if (
    (re_match("fonttbl$",    cmd, &reml) > -1)  ||
    (re_match("pict$",       cmd, &reml) > -1)  ||
    (re_match("colortbl$",   cmd, &reml) > -1)  ||
    (re_match("stylesheet$", cmd, &reml) > -1)  ||
    (re_match("operator$",   cmd, &reml) > -1)  ){
        skip_thru_block(R);
        R->attr->skippable = false;
        return;
    } 

    // COMMAND CATEGORY: EVERYTHING ~~ LINE BREAK
    if (
    (re_match("line$", cmd, &reml) > -1)  ||
    (re_match("par$",  cmd, &reml) > -1)  ||
    (re_match("pard$", cmd, &reml) > -1)  ){
        DBUG("Got a newline-indicating control word: \'%s\'", R->cmd);
        add_to_txt('\n', R);
        R->attr->skippable = false;
        return;
    } 

    // UNKNOWN COMMANDS
    else {
        if (R->attr->skippable) {
            skip_thru_block(R);
            R->attr->skippable = false;
        }
    }
}





//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                     PROCESSING BUFFER FUNCTIONS                      ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////

static void add_to_txt(int c, rtfobj *R) {
    if (!(R->ti < R->txtz - 1)) {
        LOG("Exhausted txt buffer.");
        LOG("R->ti = %zu. Last txt data: \'%s\'", R->ti, &R->txt[R->ti-80]);
        LOG("No match within limits. Flushing buffers, attempting recovery");
        output_raw(R);
        reset_buffers(R);
    }
    R->txt[R->ti++] = (char)c;
}


static void add_to_cmd(int c, rtfobj *R) {
    if (!(R->ci < R->cmdz - 1)) {
        LOG("Exhausted cmd buffer.");
        LOG("R->ci = %zu. Last cmd data: \'%s\'", R->ci, &R->cmd[R->ci-80]);
        LOG("No match within limits. Flushing buffers, attempting recovery");
        output_raw(R);
        reset_buffers(R);
    }
    R->cmd[R->ci++] = (char)c;
}


static void add_to_raw(int c, rtfobj *R) {
    if (!(R->ri < R->rawz - 1)) {
        LOG("Exhausted raw buffer.");
        LOG("R->ri = %zu. Last raw data: \'%s\'", R->ri, &R->raw[R->ri-80]);
        LOG("No match within limits. Flushing buffers, attempting recovery");
        output_raw(R);
        reset_buffers(R);
    }
    R->raw[R->ri++] = (char)c;
}


static void reset_buffers(rtfobj *R) {
    reset_raw_buffer(R);
    reset_txt_buffer(R);
    reset_cmd_buffer(R);
}


static void reset_raw_buffer(rtfobj *R) {
    R->ri = 0UL;
    memzero(R->raw, R->rawz);
}


static void reset_txt_buffer(rtfobj *R) {
    R->ti = 0UL;
    memzero(R->txt, R->txtz);
}


static void reset_cmd_buffer(rtfobj *R) {
    R->ci = 0UL;
    memzero(R->cmd, R->cmdz);
}







//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                       BUFFER OUTPUT FUNCTIONS                        ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////


void output_match(rtfobj *R) {
    size_t len = strlen(R->dict_val[R->dict_match]);

    fwrite(R->dict_val[R->dict_match], 1, len, R->fout);

    // Output the same number of braces we had in our raw buffer
    // Otherwise, if the text matching our replacement key had a font change
    // something, we could end up with mismatched braces and a mildly
    // corrupted RTF file.  NB: Be careful not to do this with escaped
    // literal brace characters. 
    for (size_t i = 0UL; i < (R->ri - 1); i++) {
        if (R->raw[i] == '\\' && R->raw[i+1] == '{') i++;
        else if (R->raw[i] == '\\' && R->raw[i+1] == '}') i++;
        else if (R->raw[i] == '{') fputc('{', R->fout);
        else if (R->raw[i] == '}') fputc('}', R->fout);
    }

    reset_buffers(R);
}


void output_raw(rtfobj *R) {
    fwrite(R->raw, 1, R->ri, R->fout);
    reset_buffers(R);
}


void skip_thru_block(rtfobj *R) {
    int braces = 1;
    int c;
    int clast = '\0';
    
    output_raw(R);
    reset_buffers(R);

    while (braces != 0) {
        c = fgetc(R->fin);
        assert(c != EOF);
        if (c == '{' && clast != '\\') braces++;
        if (c == '}' && clast != '\\') braces--;
        fputc(c, R->fout);
        clast = c;
    }
}







//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                      ATTRIBUTE STACK FUNCTIONS                       ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////

void push_attr(rtfobj *R) {
    rtfattr *newattr = malloc(sizeof *newattr);

    if (!newattr) {
        LOG("Out-of-memory allocating new attribute scope.");
        R->fatalerr = ENOMEM;
        return;
    }

    if (R->attr == NULL) {
        newattr->uc = 0;
        newattr->skipbytes = 0;
        newattr->cpg = CPG_1252;
        newattr->skippable = false;
        newattr->outer = NULL;
        R->attr = newattr;
    } else {
        memmove(newattr, R->attr, sizeof *newattr);
        newattr->outer = R->attr;
        R->attr = newattr;
    }
}


void pop_attr(rtfobj *R) {
    rtfattr *oldattr;

    if (!R->attr) {
        LOG("Attempt to pop non-existent attribute set off stack!");
        LOG("Ignoring likely off-by-one error and hoping for the best");
        return;
    } else {
        oldattr = R->attr;        // Point it at the current attribute set
        R->attr = oldattr->outer; // Modify structure to point to outer scope
        free(oldattr);            // Delete the old attribute set
    }
}







//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                          UTILITY FUNCTIONS                           ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////


void encode_utf8(int32_t c, char *utf8) {
    if (c < 0x80) {
        // (__ & 01111111)|00000000  ==> 0xxx xxxx
        utf8[0] = (char)(c>>0  &  0x7F) | 0x00;  
    }
    else if (c < 0x0800) {
        // (__ & 00011111)|11000000  ==> 110x xxxx
        // (__ & 00111111)|10000000  ==> 10xx xxxx
        utf8[0] = (char)(c>>6 & 0x1F) | 0xC0;
        utf8[1] = (char)(c>>0 & 0x3F) | 0x80;
    }
    else if (c < 0x010000) {
        // (__ & 00001111)|11100000  ==> 1110 1111
        // (__ & 00111111)|10000000  ==> 10xx xxxx
        // (__ & 00111111)|10000000  ==> 10xx xxxx
        utf8[0] = (char)(c>>12 &  0x0F) | 0xE0; 
        utf8[1] = (char)(c>>6  &  0x3F) | 0x80;
        utf8[2] = (char)(c>>0  &  0x3F) | 0x80;
    }
    else if (c < 0x110000) {
        // (__ & 00000111)|11110000  ==> 1111 0111
        // (__ & 00111111)|10000000  ==> 10xx xxxx
        // (__ & 00111111)|10000000  ==> 10xx xxxx
        // (__ & 00111111)|10000000  ==> 10xx xxxx
        utf8[0] = (char)(c>>18 &  0x07) | 0xF0;
        utf8[1] = (char)(c>>12 &  0x3F) | 0x80;
        utf8[2] = (char)(c>>6  &  0x3F) | 0x80;
        utf8[3] = (char)(c>>0  &  0x3F) | 0x80;
    }
}


void memzero(void *const p, const size_t n) {
    volatile unsigned char *volatile p_ = (volatile unsigned char *volatile) p;
    for (; p_  <  (volatile unsigned char *volatile) p + n; p_++) {
        *p_ = 0U;
    }
}
