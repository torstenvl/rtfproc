// ---------------------------------------------------------------------------- 
// LIMITATIONS
//
//   - We use a static buffer size for the raw RTF code, the resulting text,
//     and any RTF commands we come across.  In theory, we could end up with
//     a buffer overflow.  We check this, but it's brittle. 
//
//   - Later we will wrap getting an input character to buffer it with a
//     larger read buffer and improve performance.
//
//   - This library does not adjust for the actual code page set in the RTF
//     document, but instead assumes that all specified codes via \'xx are
//     Unicode (mostly-ish compatible with code page Windows-1252).
//     These are encoded as UTF-8 internally for comparison/text output.
//
//   - When working with Unicode code points, this library assumes that 
//     the source text is normalized equivalently to the replacement table.
//     For software that uses the document itself to prompt the user for
//     replacements, this should not be an issue; however, it may cause
//     issues in other software using this library.  Future versions will
//     normalize to NFC before comparison. 
//
//   - This library is not aware of an exhaustive list of commands that take
//     arguments that should be treated as raw data and not text data.  This
//     means it's possible that we could end up performing a replacement in 
//     the middle of, e.g., an embedded audio clip or something.  
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
#include <assert.h>
#include "rtfsed.h"
#include "re.h"

#define DIE(s) (die(s, __func__, __LINE__))

void dispatch_scope(int c, rtfobj *R);
void dispatch_text(int c, rtfobj *R);
void dispatch_command(int c, rtfobj *R);

void read_command(int c, rtfobj *R);
void proc_command(rtfobj *R);

void push_attr(rtfobj *R);
void pop_attr(rtfobj *R);

int pattern_match(rtfobj *R);

void output_match(rtfobj *R);
void output_raw(rtfobj *R);

void add_to_txt(int c, rtfobj *R);
void add_to_cmd(int c, rtfobj *R);
void add_to_raw(int c, rtfobj *R);

void reset_buffers(rtfobj *R);
void reset_raw_buffer(rtfobj *R);
void reset_txt_buffer(rtfobj *R);
void reset_cmd_buffer(rtfobj *R);

void skip_thru_block(rtfobj *R);
void encode_utf8(int32_t c, char *buf);
void memzero(void *const p, const size_t n);
void die(const char *s, const char *f, size_t l);

// #define DEBUG

#ifdef DEBUG
#define DBUG(...) (fprintf(stderr, __VA_ARGS__))
#else
#define DBUG(...) ((void)0)
#endif







//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                   CONSTRUCTOR FOR NEW RTF OBJECTS                    ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////


rtfobj *new_rtfobj(FILE *fin, FILE *fout, const char **dict) {
    size_t i;

    rtfobj *R = malloc(sizeof *R);
    if (!R) DIE("Failed allocating new RTF Object.");

    R->fin = fin;
    R->fout = fout;
    R->ftxt = NULL;

    #ifdef DEBUG
        // Try to open text log for plain text debugging output.
        // Do NOT handle error conditions, since all code will ignore
        // this FILE* if it is NULL anyway.  If the file fails to open,
        // then we will fail silently and gracefully. 
        R->ftxt = fopen("rtfsed-debug.txt", "wb");
    #endif 
    
    // Loop through and find out how many of these replacement fuckers
    // we have to turn into a proper dictionary form via parallel 
    // arrays for keys, values, and viability flags. 
    for (i=0; dict[i] != NULL; i++); 
    R->dictz = i/2;
    R->dict_max_keylen = 0;
    R->dict_match = 0;

    R->dict_key = malloc(R->dictz * sizeof *R->dict_key);
    R->dict_val = malloc(R->dictz * sizeof *R->dict_val);
    if (!R->dict_key) DIE("Failed allocating dictionary key pointer array.");
    if (!R->dict_val) DIE("Failed allocating dictionary value pointer array.");

    for (i=0; i < R->dictz; i++) {
        assert(dict[2*i] != NULL);
        assert(dict[2*i+1] != NULL);
        R->dict_key[i] = dict[2*i];
        R->dict_val[i] = dict[2*i+1];
        if (strlen(R->dict_key[i]) > R->dict_max_keylen) {
            R->dict_max_keylen = strlen(R->dict_key[i]);
        }
    }

    R->attr = NULL;

    R->ri = 0UL;
    R->rawz = RAW_BUFFER_SIZE;
    memzero(R->raw, R->rawz);

    R->ti = 0UL;
    R->txtz = TXT_BUFFER_SIZE;
    memzero(R->txt, R->txtz);

    R->ci = 0UL;
    R->cmdz = CMD_BUFFER_SIZE;
    memzero(R->cmd, R->cmdz);
    
    return R;
}







//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                           MAIN LOGIC LOOP                            ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////


void rtfreplace(rtfobj *R) {
    int c;

    while ((c = fgetc(R->fin)) != EOF) {
        switch (c) {
            case '{':           dispatch_scope(c, R);      break;
            case '}':           dispatch_scope(c, R);      break;
            case '\\':          dispatch_command(c, R);    break;
            default:            dispatch_text(c, R);       break;
        }
        switch (pattern_match(R)) {
            case PARTIAL:        /* Keep reading */        break;
            case MATCH:          output_match(R);          break;
            case NOMATCH:        output_raw(R);            break;
        }
    }

    // We hit EOF, output remaining raw buffer.  Since it didn't match so far,
    // and we have no more input, it will never match. 
    output_raw(R);
}







//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                         DISPATCH FUNCTIONS                           ////
////                                                                      ////
////     Deal with different categories of action from the main loop      ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////


/*-----------------------------------------\
|                                          |
|   Deal with changing RTF scope blocks.   |
|                                          |
\-----------------------------------------*/
void dispatch_scope(int c, rtfobj *R) {
    R->raw[R->ri++] = c;

    // Open a new scope
    if (c == '{') {
        push_attr(R);
    } 
    
    // Close the scope
    else if (c == '}') {
        pop_attr(R);
    }
}


/*----------------------------------------------------------------\
|                                                                 |
|   Deal RTF commands. Complicated - will use helper functions.   |
|                                                                 |
\----------------------------------------------------------------*/
void dispatch_command(int c, rtfobj *R) {
    read_command(c, R);
    proc_command(R);
}


/*------------------------------------------\
|                                           |
|   Deal with plain text in the RTF file.   |
|                                           |
\------------------------------------------*/
void dispatch_text(int c, rtfobj *R) {
    // Ignore newlines in RTF code
    if (c == '\n') {
        add_to_raw(c, R);
    } 
    
    // Ignore carriage returns in RTF code
    else if (c == '\r') {
        add_to_raw(c, R);
    } 
    
    // Consider tabs in RTF code to be interchangeable with spaces
    else if (c == '\t') {
        add_to_txt(' ', R);
        add_to_raw(c, R);
    } 
    
    // Consider vertical tabs in RTF code to be interchangeable with spaces
    else if (c == '\v') {
        add_to_txt(' ', R);
        add_to_raw(c, R);
    } 
    
    // Treat all other text literally, add it to the text buffer as-is
    else {
        add_to_txt(c, R);
        add_to_raw(c, R);
    }
}


/*--------------------------------------------------------\
|                                                         |
|   Check whether the current plain text buffer matches   |
|   any of the replacement tokens we're looking for.      |
|                                                         |
\--------------------------------------------------------*/
int pattern_match(rtfobj *R) {
    size_t i;

    // This part is easy.  If we have shit in the raw buffer, but there is
    // NO TEXTUAL DATA, then we need to shuttle that shit out to the file.
    if (R->ri > 0 && R->ti == 0) return NOMATCH;

    // Check for complete matches
    for (i = 0; i < R->dictz; i++) {
        if (!strcmp(R->txt, R->dict_key[i])) {
            R->dict_match = i;
            return MATCH;
        }
    }

    // Check for partial matches
    for (i = 0; i < R->dictz; i++) {
        if (!strncmp(R->txt, R->dict_key[i], R->ti)) {
            return PARTIAL;
        }
    }

    // TODO:  Check for late partial matches by iterating through the text
    //        buffer and trying the same strncmp() loop as above with 
    //        increasing offsets.  If we get a partial match with an offset,
    //        then we should do a partial buffer flush up to the offset.
    //        This will prevent situations where an actual match happens
    //        after a false start for a different token.  Example:
    //           file = ... «ll«clname» ...
    //           txt  = «ll«
    //        Currently, we would invalidate the match on the second « 
    //        and return NOMATCH, flushing the entire text buffer and 
    //        preventing a future MATCH against «clname».  By iterating
    //        through and looking for partial matches, we would see that
    //           strncmp(&R->txt[3], R->dict_key[?], R->ti-3)
    //        is promising, and we would only flush the first four
    //        characters of the txt buffer.

    return NOMATCH;
}






//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                     DISPATCH HELPER FUNCTIONS                        ////
////                                                                      ////
////     Read commands and process a limited subset of them (most are     ////
////     not needed for the purposes of simple search-and-replace.        ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////


/*-----------------------------------------------------------------\
|                                                                  |
|   Just read in the command - easy, since very few use special    |
|   symbols and the rest are alphanumeric in the primary token.    |
|                                                                  |
\-----------------------------------------------------------------*/
void read_command(int c, rtfobj *R) {
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
        case '\'':
            add_to_cmd(c, R);
            add_to_raw(c, R);
            assert((c = fgetc(R->fin)) != EOF);
            // Fall through and get the next complete alphanumeric string
            // (which should be a two-character hex string encoding a non-
            // ASCII code point). 
        default:
            assert(isalnum(c));
            add_to_cmd(c, R);
            add_to_raw(c, R);
            while(isalnum(c = fgetc(R->fin))) {
                add_to_cmd(c, R);
                add_to_raw(c, R);
            }
            if (c != EOF) {
                if (isspace(c)) {
                    add_to_raw(c, R);
                } else {
                    ungetc(c, R->fin);
                }
            }
            break;
    }
}


/*-----------------------------------------------------------------\
|                                                                  |
|   Process a limited subset of commands.  In some situations,     |
|   that might mean just ignoring everything in the block that     |
|   comes after them, so that we don't treat it as text.           |
|                                                                  |
|   (I am embarrassed about the amount of time I spent debugging   |
|   my own mistaken use of the regular expressions API...)         |
|                                                                  |
\-----------------------------------------------------------------*/
void proc_command(rtfobj *R) {
    int reml;

    // COMMAND CATEGORY: ESCAPED CHARACTER LITERALS
    if (
    (!strcmp(R->cmd, "\\\\"))  ||
    (!strcmp(R->cmd, "\\{"))   ||
    (!strcmp(R->cmd, "\\}"))   ){
        add_to_txt(R->cmd[1], R);
        return;
    } 

    // COMMAND: MARK NEXT COMMAND AS OPTIONAL
    if (!strcmp(R->cmd, "\\*")) {
        R->attr->skippable = true;
        return;
    } 

    // COMMAND: SET THE UNICODE SKIP-BYTE COUNT
    if (re_match("\\\\uc.*", R->cmd, &reml) > -1) {
        R->attr->uc = strtoul(&R->cmd[3], NULL, 10);
        return;
    } 

    // COMMAND: UNICODE CODE POINT SPECIFICATION
    if (re_match("\\\\u[0-9]*$", R->cmd, &reml) > -1) {
        char utf8[5] = { '\0' };
        int32_t c;
        char *p;

        c = strtoul(&R->cmd[2], NULL, 10);
        encode_utf8(c, utf8);
        for (p = utf8; *p != '\0'; p++) add_to_txt(*p, R);

        // We need to implement the other end of this in the add_to_txt function
        R->attr->skipbytes = R->attr->uc;

        return;
    } 
    
    // COMMAND: CODE PAGE CODE POINT SPECIFICATION
    //    TODO: Take the actual code page into account instead of just praying
    if (re_match("\\\\\\\'[0-9A-Fa-f][0-9A-Fa-f]$", R->cmd, &reml) > -1) {
        char utf8[5] = { '\0' };
        int32_t c;
        char *p;

        c = strtoul(&R->cmd[2], NULL, 16);
        encode_utf8(c, utf8);
        for (p = utf8; *p != '\0'; p++) add_to_txt(*p, R);

        return;
    } 

    // COMMAND CATEGORY: ALL THE SKIPPABLE SHIT I DON'T CARE ABOUT WITH 
    // WEIRD PARAMETER DATA THAT WILL FUCK UP MY TEXT-MATCHING
    if (
    (re_match("\\\\fonttbl", R->cmd, &reml) > -1)     ||
    (re_match("\\\\pict", R->cmd, &reml) > -1)        ||
    (re_match("\\\\colortbl", R->cmd, &reml) > -1)    ||
    (re_match("\\\\stylesheet", R->cmd, &reml) > -1)  ||
    (re_match("\\\\operator", R->cmd, &reml) > -1)    ){
        skip_thru_block(R);
        return;
    } 

    // COMMAND CATEGORY: EVERYTHING WE WANT TO SHOW UP AS A LINE BREAK
    // IN THE PLAIN TEXT EXTRACTION OF THIS RTF FILE.
    if (
    (re_match("\\\\line", R->cmd, &reml) > -1)  ||
    (re_match("\\\\par", R->cmd, &reml) > -1)   ||
    (re_match("\\\\pard", R->cmd, &reml) > -1)  ){
        add_to_txt('\n', R);
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


void add_to_txt(int c, rtfobj *R) {
    if (R->ftxt) fputc(c, R->ftxt); 
    if (R->ftxt) fflush(R->ftxt);

    if (R->ti > R->txtz - 2) {
        DBUG("We have exhausted the txt buffer.\n");
        DBUG("R->ti = %lu\n", R->ti);
        DBUG("Last txt data:\n");
        DBUG("%s\n", &R->txt[R->ti - 80]);
        DIE("Assertion failed: (R->ti < R->txtz - 1)");
    }
    R->txt[R->ti++] = c;
}


void add_to_cmd(int c, rtfobj *R) {
    if (R->ci > R->cmdz - 2) {
        DBUG("We have exhausted the cmd buffer.\n");
        DBUG("R->ci = %lu\n", R->ci);
        DBUG("Last cmd data:\n");
        DBUG("%s\n", &R->cmd[R->ci - 80]);
        DIE("Assertion failed: (R->ci < R->cmdz - 1)");
    }
    R->cmd[R->ci++] = c;
}


void add_to_raw(int c, rtfobj *R) {
    if (R->ri > R->rawz - 2) {
        DBUG("We have exhausted the raw buffer.\n");
        DBUG("R->ri = %lu\n", R->ri);
        DBUG("Last raw data:\n");
        DBUG("%s\n", &R->raw[R->ri - 80]);
        DIE("Assertion failed: (R->ri < R->rawz - 1)");
    }
    R->raw[R->ri++] = c;
}


void reset_buffers(rtfobj *R) {
    reset_raw_buffer(R);
    reset_txt_buffer(R);
    reset_cmd_buffer(R);
}


void reset_raw_buffer(rtfobj *R) {
    R->ri = 0UL;
    memzero(R->raw, R->rawz);
}


void reset_txt_buffer(rtfobj *R) {
    R->ti = 0UL;
    memzero(R->txt, R->txtz);
}


void reset_cmd_buffer(rtfobj *R) {
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

    if (!newattr) DIE("Failed allocating new attribute scope.");

    if (R->attr == NULL) {
        newattr->uc = 0;
        newattr->skipbytes = 0;
        newattr->cp = CP_1252;
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

    if (!R->attr) DIE("Attempt to pop non-existent attribute set off stack!");

    oldattr = R->attr;             // Point it at the current attribute set
    R->attr = oldattr->outer;      // Modify structure to point to outer scope
    free(oldattr);                 // Delete the old attribute set
}







//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                          UTILITY FUNCTIONS                           ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////


void encode_utf8(int32_t c, char *utf8) {
    if (c < 0x80) {
        utf8[0] = (c>>0  &  0b01111111)  |  0b00000000; 
    }
    else if (c < 0x0800) {
        utf8[0] = (c>>6  &  0b00011111)  |  0b11000000;
        utf8[1] = (c>>0  &  0b00111111)  |  0b10000000;
    }
    else if (c < 0x010000) {
        utf8[0] = (c>>12 &  0b00001111)  |  0b11100000;
        utf8[1] = (c>>6  &  0b00111111)  |  0b10000000;
        utf8[2] = (c>>0  &  0b00111111)  |  0b10000000;
    }
    else if (c < 0x110000) {
        utf8[0] = (c>>18 &  0b00000111)  |  0b11110000;
        utf8[1] = (c>>12 &  0b00111111)  |  0b10000000;
        utf8[2] = (c>>6  &  0b00111111)  |  0b10000000;
        utf8[3] = (c>>0  &  0b00111111)  |  0b10000000;
    }
}


void memzero(void *const p, const size_t n) {
    volatile unsigned char *volatile p_ = (volatile unsigned char *volatile) p;
    for (; p_  <  (volatile unsigned char *volatile) p + n; p_++) {
        *p_ = 0U;
    }
}


void die(const char *s, const char *f, size_t l) {
    fprintf(stderr, "Died on line %zu of function %s:\n    %s\n\n\n", l, f, s);
    exit(EXIT_FAILURE);
}
