// ----------------------------------------------------------------------------
// TODO
//   - Implement a font table. Because document fonttbl entries are not
//     guaranteed to be in consecutive ordinal order, it may be highly space-
//     inefficient to store font table entries in a sparse array, though it
//     would give O(1) access. The next best way might be to store them in a
//     dense sorted array of fonttbl entries, with binary search-insertion and
//     binary search retrieval. This should give O(log N) access.
//
//   - Add attributes for \ansicpgN, font tables with \fcharsetN and/or
//     \cpgN, and \cchsN. Interpret \'xx according to current character set
//     or code page. 
//         NOTE: Different processors recognize these differently, and end-
//         users will be VERY disappointed if we don't match based on what
//         they see in the word processor.  We should, at some point, try to
//         divine the word processor used and mimic its interpretation of
//         how (whether) to recognize these different character set specs.
//
//   - Normalize replacement tokens as well as any Unicode in the text before
//     comparison.  Use NFC (or maybe NFD?) normalization.
//   
//   - Add support for late partial matches. Example:
//         Replacements
//             "ATTORNEY"     => "Mr. Smith"
//             "TORTLOCATION" => "December 12, 2021"
//         Text has 
//             "THEY MET ATTORTLOCATION..." [sic]
//         Problem
//             Once the partial match ATTOR is invalidated with ATTORT,
//             the raw buffer will be flushed and matching will begin
//             with LOCATION.  The match for TORTLOCATION will never be
//             found.
//         Solution
//             Brute force
//             -----------
//             Iterate through file multiple times, using intermediate temp
//             files, and only doing one replacement at a time.
//
//             Less-brute force
//             ----------------
//             Upon failing a match, ungetc() all of the raw buffer, set 
//             skipbytes/uc0i to 1 to skip one byte in the 'output' but
//             not in the raw RTF code, reset the buffers, and get ready
//             to fuckin' Rock 'n' Roll. 
//
//             More clever
//             -----------
//             Use a size_t txtrawmap[] with the same no. of elements as
//             txt[], where raw[txtrawmap[i]] is the start of txt[i] in
//             the RTF code. Loop size_t offset to ti and discover that
//               !strncmp(&R->txt[offset], R->srch_key[__], R->ti-offset)
//               (when offset == 2)
//             Then we can output raw[0] through raw[txtrawmap[i]], and
//             memmove() raw[] and txt[] (and adjust ri and ti) to begin
//             processing our new partial match. 
// ---------------------------------------------------------------------------- 


/*-------------------------------------------------------------------------*\
|                                                                           |
|                           DECLARATIONS & MACROS                           |
|                                                                           |
\*-------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "rtfsed.h"
#include "STATIC/re.h"
#include "STATIC/cpgtou.h"

// Internal function declarations
static void dispatch_scope(int c, rtfobj *R);
static void dispatch_text(int c, rtfobj *R);
static void dispatch_command(rtfobj *R);
static void read_command(rtfobj *R);
static void proc_command(rtfobj *R);
static void proc_cmd_escapedliteral(rtfobj *R);
static void proc_cmd_asterisk(rtfobj *R);
static void proc_cmd_uc(rtfobj *R);
static void proc_cmd_u(rtfobj *R);
static void proc_cmd_apostrophe(rtfobj *R);
static void proc_cmd_fonttbl(rtfobj *R);
static void proc_cmd_f(rtfobj *R);
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
static void reset_raw_buffer(rtfobj *R);
static void reset_txt_buffer(rtfobj *R);
static void reset_cmd_buffer(rtfobj *R);

static void encode_utf8(int32_t c, char utf8[5]);
static int32_t get_num_arg(const char *s);
static int32_t get_hex_arg(const char *s);

// Macros
// #define RTFDEBUG
#define REGEX_MATCH(x, y)    (re_match(y, x, NULL) > -1)
#define memzero(x, y)        (memset(x, 0, y))
#ifdef RTFDEBUG
    #define DBUG(...) (LOG(__VA_ARGS__))
#else
    #define DBUG(...) ((void)0)
#endif
#define LOG(...) (fprintf(stderr, "In rtfsed.c:%s() on line %d: ", \
                  __func__, __LINE__) \
                  && fprintf(stderr, __VA_ARGS__ ) \
                  && fprintf(stderr, "\n"))



/*-------------------------------------------------------------------------*\
|                                                                           |
|               CONSTRUCTOR/DESTRUCTOR FOR NEW RTF OBJECTS                  |
|                                                                           |
\*-------------------------------------------------------------------------*/
rtfobj *new_rtfobj(FILE *fin, FILE *fout, const char **srch) {
    size_t i;
    rtfobj *R = malloc(sizeof *R);

    if (!R) { LOG("Failed allocating new RTF Object."); return NULL; }

    // Set up file streams    
    R->fin = fin;
    R->fout = fout;

    R->fatalerr = 0;

    R->ri = 0UL;
    R->ti = 0UL;
    R->ci = 0UL;
    R->rawz = RAW_BUFFER_SIZE;
    R->txtz = TXT_BUFFER_SIZE;
    R->cmdz = CMD_BUFFER_SIZE;
    memzero(R->raw, R->rawz);
    memzero(R->txt, R->txtz);
    memzero(R->cmd, R->cmdz);

    // Set up replacement dictionary
    for (i=0; srch[i] != NULL; i++); 
    R->srchz = i/2;
    R->srch_match = 0UL; 
    R->srch_key = malloc(R->srchz * sizeof *R->srch_key);
    R->srch_val = malloc(R->srchz * sizeof *R->srch_val);
    if ((!R->srch_key) || (!R->srch_val)) { 
        delete_rtfobj(R);
        LOG("Out of memory allocating search key/value pointers!");
        return NULL; 
    }
    for (i=0; i < R->srchz; i++) {
        R->srch_key[i] = srch[2*i];
        R->srch_val[i] = srch[2*i+1];
    }

    R->attr = NULL;

    return R;
}

void delete_rtfobj(rtfobj *R) {
    if (R) {
        free(R->srch_key);
        free(R->srch_val);
        while (R->attr) {
            pop_attr(R);
        }
    }
    free(R);
}





/*-------------------------------------------------------------------------*\
|                                                                           |
|                  MAIN PROCESSING LOOP & LIBRARY ENTRY                     |
|                                                                           |
\*-------------------------------------------------------------------------*/
void rtfreplace(rtfobj *R) {
    int c;

    // Loop until we reach the end of the input file
    while ((c = fgetc(R->fin)) != EOF) {
        switch (c) {
            case '{':           dispatch_scope(c, R);      break;
            case '}':           dispatch_scope(c, R);      break;
            case '\\':          dispatch_command(R);       break;
            default:            dispatch_text(c, R);       break;
        }

        switch (pattern_match(R)) {
            case PARTIAL:        /* Keep reading */        break;
            case MATCH:          output_match(R);
                                 reset_raw_buffer(R);
                                 reset_txt_buffer(R);      break;
            case NOMATCH:        output_raw(R);
                                 reset_raw_buffer(R);
                                 reset_txt_buffer(R);      break;
        }
        
        if (R->fatalerr)  {  LOG("Encountered a fatal error");  return;  }
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
    add_to_raw(c, R);
    if      (c == '{')  push_attr(R);
    else if (c == '}')  pop_attr(R);
}


static void dispatch_command(rtfobj *R) {
    read_command(R);
    proc_command(R);
    // Add command to raw buffer. This isn't done during read_command()
    // IOT ensure all text is added to R->txt before corresponding code is
    //     added to R->raw
    // IOT allow us to defer flushing R->raw until first addition to R->txt
    //     instead of doing it every iteration
    //     BUT not accidentally including text-generating RTF code when
    //         the text it generates should be replaced. 
    for (char *s = R->cmd; *s != '\0'; s++) {
        add_to_raw((int)(*s), R);
    }

}


static void dispatch_text(int c, rtfobj *R) {
    // Ignore newlines and carriage returns in RTF code. Consider tabs and
    // vertical tabs to be interchangeable with spaces. Treat everything else
    // literally. 
    if (R->attr && R->attr->notxt) {                  add_to_raw(c, R);     }
    else if (c == '\n') {                             add_to_raw(c, R);     }
    else if (c == '\r') {                             add_to_raw(c, R);     } 
    else if (c == '\t') {     add_to_txt(' ', R);     add_to_raw(c, R);     }
    else if (c == '\v') {     add_to_txt(' ', R);     add_to_raw(c, R);     }
    else {                    add_to_txt(c, R);       add_to_raw(c, R);     }
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
    size_t i, j;

    // Check for complete matches
    for (i = 0; i < R->srchz; i++) {
        for (j = 0; R->txt[j] != '\0' && R->txt[j] == R->srch_key[i][j]; j++); 
        if (R->txt[j] == R->srch_key[i][j]) { R->srch_match = i; return MATCH; }
    }

    // Check for partial matches at current offset. This includes an empty
    // text buffer with data in the raw buffer. To prevent discarding raw 
    // RTF that's important when there's a match, we output the raw buffer
    // when we FIRST add to the text buffer (see below) rather than on every
    // input byte that doesn't contain text. 
    for (i = 0; i < R->srchz; i++) {
        for (j = 0; j < R->ti && R->txt[j] == R->srch_key[i][j]; j++); 
        if (j == R->ti) { return PARTIAL; }
    }

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

static void read_command(rtfobj *R) {
    int c;

    reset_cmd_buffer(R);

    add_to_cmd('\\', R);

    if ((c=fgetc(R->fin)) == EOF) { LOG("Unexpected EOF"); R->fatalerr = EIO; return; }

    switch (c) {
        case '{':  // Escaped literal
        case '}':  // Escaped literal
        case '\\': // Escaped literal
        case '*':  // Special one-character command
            add_to_cmd(c, R);
            break;
        case '\r':
            add_to_cmd('\r', R);
            break;
        case '\n':
            add_to_cmd('\n', R);
            break;
        case '\'':
            add_to_cmd(c, R);

            if ((c=fgetc(R->fin)) == EOF) { LOG("Unexpected EOF"); R->fatalerr = EIO; break; }
            add_to_cmd(c, R);

            if ((c=fgetc(R->fin)) == EOF) { LOG("Unexpected EOF"); R->fatalerr = EIO; break; }
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
            else                  ungetc(c, R->fin);
            break;
    }
}


static void proc_command(rtfobj *R) {
    // Set pointer for internal use so that we don't have to deal with 
    // backslashes at the beginning of our regular expression matching
    char *cmd = &R->cmd[1];

    if (0); 
    else if (REGEX_MATCH(cmd, "^\\$"))                         proc_cmd_escapedliteral(R);
    else if (REGEX_MATCH(cmd, "^{$"))                          proc_cmd_escapedliteral(R);
    else if (REGEX_MATCH(cmd, "^}$"))                          proc_cmd_escapedliteral(R);
    else if (REGEX_MATCH(cmd, "^uc[0-9]+ ?$"))                 proc_cmd_uc(R);
    else if (REGEX_MATCH(cmd, "^u-?[0-9]+ ?$"))                proc_cmd_u(R);
    else if (REGEX_MATCH(cmd, "^\'[0-9A-Fa-f][0-9A-Fa-f] ?$")) proc_cmd_apostrophe(R);
    else if (REGEX_MATCH(cmd, "^fonttbl ?$"))                  proc_cmd_fonttbl(R);
    else if (REGEX_MATCH(cmd, "^f[0-9]+ ?$"))                  proc_cmd_f(R);
    else if (REGEX_MATCH(cmd, "^pict ?$"))                     proc_cmd_shuntblock(R);
    else if (REGEX_MATCH(cmd, "^colortbl ?$"))                 proc_cmd_shuntblock(R);
    else if (REGEX_MATCH(cmd, "^stylesheet ?$"))               proc_cmd_shuntblock(R);
    else if (REGEX_MATCH(cmd, "^operator ?$"))                 proc_cmd_shuntblock(R);
    else if (REGEX_MATCH(cmd, "^bin ?$"))                      proc_cmd_shuntblock(R);
    else if (REGEX_MATCH(cmd, "^par ?$"))                      proc_cmd_newpar(R);
    else if (REGEX_MATCH(cmd, "^line ?$"))                     proc_cmd_newline(R);
    else if (1)                                                proc_cmd_unknown(R);
    
    // If the command is \* then the entire block is optional, but... if we 
    // then recognize the command, revoke that 'optional' status.
    if (REGEX_MATCH(R->cmd, "^*$")) R->attr->blkoptional = true;
    else                            R->attr->blkoptional = false;
}


static void proc_cmd_escapedliteral(rtfobj *R) {
    add_to_txt(R->cmd[1], R);
}


static void proc_cmd_uc(rtfobj *R) {
    R->attr->uc = (size_t)get_num_arg(&R->cmd[1]);
}


static void proc_cmd_u(rtfobj *R) {
    char utf8[5];
    encode_utf8(get_num_arg(&R->cmd[1]), utf8);
    add_string_to_txt(utf8, R);
    R->attr->uc0i = R->attr->uc;
}


static void proc_cmd_apostrophe(rtfobj *R) {
    char utf8[5];
    if (R->attr->uc0i) {
        R->attr->uc0i--;
    } else {
        // TODO: CONVERT FROM CODE PAGE TO UNICODE (SEE HEADER TODO)
        encode_utf8(get_hex_arg(&R->cmd[1]), utf8);
        add_string_to_txt(utf8, R);
    }
}


static void proc_cmd_fonttbl(rtfobj *R) {
    R->attr->fonttbl = true;
    R->attr->notxt = true;
}


static void proc_cmd_f(rtfobj *R) {
    if (R->attr->fonttbl) {
        // Define a new font (with default fcharset)
    } else {
        // Set charset to the corresponding fcharset of the font
    }
}


static void proc_cmd_shuntblock(rtfobj *R) {
    R->attr->nocmd = true;
    R->attr->notxt = true;
}


static void proc_cmd_newpar(rtfobj *R) {
    add_to_txt('\n', R);
    add_to_txt('\n', R);
}


static void proc_cmd_newline(rtfobj *R) {
    add_to_txt('\n', R);
}


static void proc_cmd_unknown(rtfobj *R) {
    if (R->attr->blkoptional) {
        R->attr->nocmd = true;
        R->attr->notxt = true;
    }
}




//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                     PROCESSING BUFFER FUNCTIONS                      ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////

static void add_to_raw(int c, rtfobj *R) {
    if (R->ri+1 >= R->rawz) {
        DBUG("Exhausted raw buffer.");
        DBUG("R->ri = %zu. Last raw data: \'%s\'", R->ri, &R->raw[R->ri-80]);
        DBUG("No match within limits. Flushing buffers, attempting recovery");
        output_raw(R);
        reset_raw_buffer(R);
        reset_txt_buffer(R);
        reset_cmd_buffer(R);
    }
    R->raw[R->ri++] = (char)c;
}

static void add_to_txt(int c, rtfobj *R) {
    // Output the raw buffer when we FIRST add text to the text buffer.
    // This ONLY works if we always add to the text buffer before we add
    // to the text buffer... Meeting this requirement necessitated 
    // reworking the command_dispatch() code so that read_command() 
    // doesn't add the command text to the raw buffer -- only at the end
    // of proc_command() (including potentially adding text to R->txt)
    // do we add to the raw buffer.  This ensures that R->raw doesn't
    // contain anything we need for R->txt when R->txt is first added
    // to... but it is somewhat brittle.  There is probably a better way
    // to refactor this, probably by tracking the beginning and ending
    // index into raw for each token that adds text to R->txt. 
    if (R->ri > 0 && R->ti == 0) {
        output_raw(R);
        reset_raw_buffer(R);
    }
    if (R->ti+1 >= R->txtz) {
        DBUG("Exhausted txt buffer.");
        DBUG("R->ti = %zu. Last txt data: \'%s\'", R->ti, &R->txt[R->ti-80]);
        DBUG("No match within limits. Flushing buffers, attempting recovery");
        output_raw(R);
        reset_raw_buffer(R);
        reset_txt_buffer(R);
    }
    if (R->attr->uc0i) {
        R->attr->uc0i--;
    } else {
        R->txt[R->ti++] = (char)c;
    }
}

static void add_to_cmd(int c, rtfobj *R) {
    if (R->ci+1 >= R->cmdz) {
        DBUG("Exhausted cmd buffer.");
        DBUG("R->ci = %zu. Last cmd data: \'%s\'", R->ci, &R->cmd[R->ci-80]);
        DBUG("No match within limits. Flushing buffers, attempting recovery");
        output_raw(R);
        reset_raw_buffer(R);
        reset_txt_buffer(R);
        reset_cmd_buffer(R);
    }
    R->cmd[R->ci++] = (char)c;
}

static void add_string_to_txt(const char *s, rtfobj *R) {
    if (R->ti+strlen(s) >= R->txtz) {
        LOG("Exhausted txt buffer.");
        LOG("R->ti = %zu. Last txt data: \'%s\'", R->ti, &R->txt[R->ti-80]);
        LOG("No match within limits. Flushing buffers, attempting recovery");
        output_raw(R);
        reset_raw_buffer(R);
        reset_txt_buffer(R);
    }

    while (*s) add_to_txt((int)(*s++), R);
}


static void reset_raw_buffer(rtfobj *R) {
    memzero(R->raw, R->ri);
    R->ri = 0UL;
}


static void reset_txt_buffer(rtfobj *R) {
    memzero(R->txt, R->ti);
    R->ti = 0UL;
}


static void reset_cmd_buffer(rtfobj *R) {
    memzero(R->cmd, R->ci);
    R->ci = 0UL;
}







//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                       BUFFER OUTPUT FUNCTIONS                        ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////

static void output_match(rtfobj *R) {
    size_t len = strlen(R->srch_val[R->srch_match]);

    fwrite(R->srch_val[R->srch_match], 1, len, R->fout);

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
}


static void output_raw(rtfobj *R) {
    fwrite(R->raw, 1, R->ri, R->fout);
}








//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                      ATTRIBUTE STACK FUNCTIONS                       ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////

static void push_attr(rtfobj *R) {
    rtfattr *newattr = malloc(sizeof *newattr);

    if (!newattr) {
        LOG("Out-of-memory allocating new attribute scope.");
        R->fatalerr = ENOMEM;
        return;
    }

    if (R->attr == NULL) {
        newattr->uc = 0;
        newattr->uc0i = 0;
        newattr->blkoptional = false;
        newattr->nocmd = false;
        newattr->notxt = false;
        newattr->fonttbl = false;
        newattr->outer = NULL;
        R->attr = newattr;
    } else {
        memmove(newattr, R->attr, sizeof *newattr);
        newattr->outer = R->attr;
        R->attr = newattr;
    }
}


static void pop_attr(rtfobj *R) {
    rtfattr *oldattr;

    if (R->attr) {
        oldattr = R->attr;        // Point it at the current attribute set
        R->attr = oldattr->outer; // Modify structure to point to outer scope
        free(oldattr);            // Delete the old attribute set
    } else {
        DBUG("Attempt to pop non-existent attribute set off stack!");
        DBUG("Ignoring likely off-by-one error.");
    }
}







//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                          UTILITY FUNCTIONS                           ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////

static void encode_utf8(int32_t c, char utf8[5]) {
    char *u = utf8;
    if (c < 0b000000000000010000000) { // Up to 7 bits
        u[0]= (char)(c>>0  & 0b01111111 | 0b00000000);  // 7 bits –> 0xxxxxxx
        u[1]= '\0';
    } else
    if (c < 0b000000000100000000000) { // Up to 11 bits
        u[0]= (char)(c>>6  & 0b00011111 | 0b11000000);  // 5 bits –> 110xxxxx
        u[1]= (char)(c>>0  & 0b00111111 | 0b10000000);  // 6 bits –> 10xxxxxx
        u[2]= '\0';
    } else
    if (c < 0b000010000000000000000) { // Up to 16 bits
        u[0]= (char)(c>>12 & 0b00001111 | 0b11100000);  // 4 bits –> 1110xxxx
        u[1]= (char)(c>>6  & 0b00111111 | 0b10000000);  // 6 bits –> 10xxxxxx
        u[2]= (char)(c>>0  & 0b00111111 | 0b10000000);  // 6 bits –> 10xxxxxx
        u[3]= '\0';
    } else
    if (c < 0b100010000000000000000) { // Up to 21 bits
        u[0]= (char)(c>>18 & 0b00000111 | 0b11110000);  // 3 bits –> 11110xxx
        u[1]= (char)(c>>12 & 0b00111111 | 0b10000000);  // 6 bits –> 10xxxxxx
        u[2]= (char)(c>>6  & 0b00111111 | 0b10000000);  // 6 bits –> 10xxxxxx
        u[3]= (char)(c>>0  & 0b00111111 | 0b10000000);  // 6 bits –> 10xxxxxx
        u[4]= '\0';
    } else {
        u[0]= '\0';
    }
}


static int32_t get_num_arg(const char *s) {
    const char *validchars="0123456789-";
    const char *p;
    int32_t retval;

    p = s;
    while (!strchr(validchars, *p)) p++;

    sscanf(p, "%"SCNd32, &retval);

    return retval;
}


static int32_t get_hex_arg(const char *s) {
    const char *validchars="0123456789ABCDEFabcdef-";
    const char *p;
    int32_t retval;

    p = s;
    while (!strchr(validchars, *p)) p++;

    sscanf(p, "%"SCNx32, &retval);

    return retval;
}
