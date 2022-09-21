// --------------------------------------------------------------------------
// TODO
//   - Implement a font table via parallel arrays of int16_ts for the font
//     number and int16_ts for the charset. 
//
//   - Add attributes for \ansicpgN, font tables with \fcharsetN and/or
//     \cpgN, and \cchsN. Interpret \'xx according to current character set
//     or code page. 
//         NOTE: Different processors recognize these differently, and end-
//         users will be surprised if we don't match based on what they see
//         in the word processor.  That said... word processors don't
//         typically write RTF that they don't honor...
//
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
#include "rtfsed.h"
#include "STATIC/regex/re.h"
#include "STATIC/cpgtou/cpgtou.h"
#include "STATIC/utillib/utillib.h"

// Internal function declarations
static void dispatch_scope(int c, rtfobj *R);
static void dispatch_text(int c, rtfobj *R);
static void dispatch_command(rtfobj *R);
static void read_command(rtfobj *R);
static void proc_command(rtfobj *R);
static void proc_cmd_escapedliteral(rtfobj *R);
// static void proc_cmd_asterisk(rtfobj *R);
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
static void add_string_to_raw(const char *s, rtfobj *R);
static void reset_raw_buffer(rtfobj *R);
static void reset_txt_buffer(rtfobj *R);
static void reset_cmd_buffer(rtfobj *R);

static void encode_utf8(int32_t c, char utf8[5]);
static int32_t cdpt_from_utf16(uint16_t hi, uint16_t lo);
static int32_t get_num_arg(const char *s);
static int32_t get_hex_arg(const char *s);

#define REGEX_MATCH(x, y)    (re_match(y, x, NULL) > -1)








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////            CONSTRUCTOR/DESTRUCTOR FOR NEW RTF OBJECTS               ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

rtfobj *new_rtfobj(FILE *fin, FILE *fout, const char **srch) {
    size_t i;
    rtfobj *R = malloc(sizeof *R);

    if (!R) { LOG("Failed allocating new RTF Object."); return NULL; }

    // Initialize the whole thing to zero
    memzero(R, sizeof *R);

    // Set up file streams    
    R->fin  = fin;
    R->fout = fout;

    R->rawz = RAW_BUFFER_SIZE;
    R->txtz = TXT_BUFFER_SIZE;
    R->cmdz = CMD_BUFFER_SIZE;

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

        while (R->attr->outer) {
            pop_attr(R);
        }

        // We have to free the last one manually since the pop_attr function
        // will not remove the last attribute set, for fear of dereferencing
        // a NULL pointer during processing if there's text past the closing
        // brace of the RTF file (even spaces). ONLY the delete_rtfobj() 
        // function should remove the last attribute set. 
        free(R->attr);
    }
    free(R);
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////               MAIN PROCESSING LOOP & LIBRARY ENTRY                  ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

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

        if (R->ti>0 && R->attr && (R->attr->notxt == false)) {
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
        
        if (R->fatalerr)  {  LOG("Encountered a fatal error");  return;  }
    }

    output_raw(R);
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                         DISPATCH FUNCTIONS                          ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static void dispatch_scope(int c, rtfobj *R) {
    add_to_raw(c, R);
    if      (c == '{')  push_attr(R);
    else if (c == '}')  pop_attr(R);
}



static void dispatch_command(rtfobj *R) {
    read_command(R);
    proc_command(R);
    // RAW/TXT BUFFER COORDINATION
    //
    // Add the command to the raw buffer, but only AFTER proc_command()
    // IOT ensure R->txt is updated before R->raw even with \' and \u
    // IOT allow the R->raw buffer to flush on first R->txt update
    // IOT defer flushing R->raw on every iteration
    //
    // Why not flush R->raw on first R->txt update even if R->raw is updated
    // first? 
    // Because then R->raw will ALREADY CONTAIN the code corresponding to the
    // beginning of R->txt, so it will get flushed on first R->txt update.
    // This is bad because, if R->txt contains a match, we don't want that
    // text OR THE RTF CODE THAT GENERATES IT to wind up in the end-product,
    // only the replacement text. 
    add_string_to_raw(R->cmd, R);
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








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                       PATTERN MATCH FUNCTION                        ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static int pattern_match(rtfobj *R) {
    size_t i, j;

    // Check for complete matches
    for (i = 0; i < R->srchz; i++) {
        for (j = 0; R->txt[j] != '\0' && R->txt[j] == R->srch_key[i][j]; j++); 
        if (R->txt[j] == R->srch_key[i][j]) { R->srch_match = i; return MATCH; }
    }

    // RAW/TXT BUFFER COORDINATION
    //
    // Check for partial matches at current offset. This includes an empty
    // text buffer with data in the raw buffer. To prevent flushing raw 
    // RTF that's important when there's a match, we output the raw buffer
    // when we FIRST add to the text buffer (see below) rather than on every
    // input byte that doesn't contain text. 
    for (i = 0; i < R->srchz; i++) {
        for (j = 0; j < R->ti && R->txt[j] == R->srch_key[i][j]; j++); 
        if (j == R->ti) { return PARTIAL; }
    }

    return NOMATCH;
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                  COMMAND DISPATCH HELPER FUNCTIONS                  ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

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
        case '\n': // Line Feed --> newline
            add_to_cmd(c, R);
            break;
        case '\r': // Carriage Return --> newline
            add_to_cmd(c, R);

            // Check if next character is a newline, to avoid double newlines
            // for platforms with CRLF line terminators. 
            if ((c=fgetc(R->fin)) == EOF) { LOG("EOF after carriage return"); R->fatalerr = EIO; break; }
            if (c != '\n') ungetc(c, R->fin);

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
            else                  ungetc(c, R->fin);
            break;
    }
}



static void proc_command(rtfobj *R) {
    // Set pointer for internal use so that we don't have to deal with 
    // backslashes at the beginning of our regular expression matching
    char *cmd = &R->cmd[1];

    if (0); 
    else if (REGEX_MATCH(cmd, "^\\$"))            proc_cmd_escapedliteral(R);
    else if (REGEX_MATCH(cmd, "^{$"))             proc_cmd_escapedliteral(R);
    else if (REGEX_MATCH(cmd, "^}$"))             proc_cmd_escapedliteral(R);
    else if (REGEX_MATCH(cmd, "^uc[0-9]+ ?$"))    proc_cmd_uc(R);
    else if (REGEX_MATCH(cmd, "^u-?[0-9]+ ?$"))   proc_cmd_u(R);
    else if (REGEX_MATCH(cmd, 
             "^\'[0-9A-Fa-f][0-9A-Fa-f] ?$"))     proc_cmd_apostrophe(R);
    else if (REGEX_MATCH(cmd, "^fonttbl ?$"))     proc_cmd_fonttbl(R);
    else if (REGEX_MATCH(cmd, "^f[0-9]+ ?$"))     proc_cmd_f(R);
    else if (REGEX_MATCH(cmd, "^pict ?$"))        proc_cmd_shuntblock(R);
    else if (REGEX_MATCH(cmd, "^colortbl ?$"))    proc_cmd_shuntblock(R);
    else if (REGEX_MATCH(cmd, "^stylesheet ?$"))  proc_cmd_shuntblock(R);
    else if (REGEX_MATCH(cmd, "^operator ?$"))    proc_cmd_shuntblock(R);
    else if (REGEX_MATCH(cmd, "^bin ?$"))         proc_cmd_shuntblock(R);
    else if (REGEX_MATCH(cmd, "^par ?$"))         proc_cmd_newpar(R);
    else if (REGEX_MATCH(cmd, "^line ?$"))        proc_cmd_newline(R);
    else if (REGEX_MATCH(cmd, 
             "^[\r\n][\r\n]? ?$"))                proc_cmd_newline(R);
    else if (1)                                   proc_cmd_unknown(R);
    
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
    // This is going to be way way way more complicated...
    char utf8[5];
    int32_t arg = get_num_arg(&R->cmd[1]);
    
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
        // Argument is in the high surrogate range
        R->highsurrogate = arg;
    } else if (0xDC00 <= arg && arg <= 0xDFFF) {
        // Argument is in the low surrogate range
        int32_t cdpt = cdpt_from_utf16((uint16_t)R->highsurrogate, (uint16_t)arg);
        encode_utf8(cdpt, utf8);
        add_string_to_txt(utf8, R);
    } else {
        // Argument is likely in the Basic Multilingual Plane
        encode_utf8(arg, utf8);
        add_string_to_txt(utf8, R);
    }
    
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








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                     PROCESSING BUFFER FUNCTIONS                     ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

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
    // RAW/TXT BUFFER COORDINATION
    //
    // Output the raw buffer when we FIRST add text to the text buffer. This
    // ONLY works if we always add to the text buffer before we add to the
    // text buffer... Meeting this requirement necessitated reworking the
    // command_dispatch() code so that read_command() doesn't add to the
    // command text to the raw buffer -- only after calling proc_command()
    // (including potentially adding text to R->txt) do we add to the raw
    // buffer.  This ensures that R->raw doesn't contain anything we need for
    // R->txt when R->txt is first modified... but that seems a little bit
    // brittle.  There is probably a better way to refactor this, probably
    // by tracking the beginning and ending index into raw for the RTF code
    // that results in each byte of text in R->txt. 
    if (R->ri > 0 && R->ti == 0) {
        output_raw(R);
        reset_raw_buffer(R);
    }
    if (R->ti+1 >= R->txtz) {
        DBUG("Exhausted txt buffer.");
        DBUG("R->ti = %zu. Last txt data: \'%s\'", R->ti, &R->txt[R->ti-80]);
        LOG("No match within limits. Flushing buffers, trying to recover.");
        output_raw(R);
        reset_raw_buffer(R);
        reset_txt_buffer(R);
    }
    if (R->attr->uc0i) {
        R->attr->uc0i--;
    } else {
        R->txtrawmap[R->ti] = R->ri;
        R->txt[R->ti++]     = (char)c;
    }
}



static void add_to_cmd(int c, rtfobj *R) {
    if (R->ci+1 >= R->cmdz) {
        DBUG("Exhausted cmd buffer.");
        DBUG("R->ci = %zu. Last cmd data: \'%s\'", R->ci, &R->cmd[R->ci-80]);
        DBUG("THIS IS A MAJOR LOGIC ERROR!");
        reset_cmd_buffer(R);
    }
    R->cmd[R->ci++] = (char)c;
}



static void add_string_to_txt(const char *s, rtfobj *R) {
    if (R->ti+strlen(s) >= R->txtz) {
        LOG("Exhausted txt buffer.");
        LOG("R->ti = %zu. Last txt data: \'%s\'", R->ti, &R->txt[R->ti-80]);
        LOG("No match within limits. Flushing buffers, trying to recover.");
        output_raw(R);
        reset_raw_buffer(R);
        reset_txt_buffer(R);
    }

    while (*s) add_to_txt((int)(*s++), R);
}



static void add_string_to_raw(const char *s, rtfobj *R) {
    if (R->ri+strlen(s) >= R->rawz) {
        LOG("Exhausted raw buffer.");
        LOG("R->ri = %zu. Last raw data: \'%s\'", R->ri, &R->raw[R->ri-80]);
        LOG("No match within limits. Flushing buffers, trying to recover.");
        output_raw(R);
        reset_raw_buffer(R);
        reset_txt_buffer(R);
        reset_cmd_buffer(R);
    }

    while (*s) add_to_raw((int)(*s++), R);
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








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                       BUFFER OUTPUT FUNCTIONS                       ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static void output_match(rtfobj *R) {
    size_t len = strlen(R->srch_val[R->srch_match]);

    // fprintf(stderr, "R->raw = \"%s\"\n", R->raw);
    // fprintf(stderr, "R->txt = \"%s\"\n", R->txt);
    // fprintf(stderr, "R->txtrawmap = { %zu", R->txtrawmap[0]);
    // for (size_t i = 1U; i < R->ti; i++) {
    //     fprintf(stderr, ", %zu", R->txtrawmap[i]);
    // }
    // fprintf(stderr, " }\n");

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








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                      ATTRIBUTE STACK FUNCTIONS                      ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

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
        // "If an RTF scope delimiter character (that is, an opening or
        // closing brace) is encountered while scanning skippable data,
        // the skippable data is considered to end before the delimiter."
        R->attr->uc0i = 0; 
        memmove(newattr, R->attr, sizeof *newattr);
        newattr->outer = R->attr;
        R->attr = newattr;
    }
}



static void pop_attr(rtfobj *R) {
    rtfattr *oldattr = NULL;

    if (R->attr && R->attr->outer) {
        oldattr = R->attr;        // Point it at the current attribute set
        R->attr = oldattr->outer; // Modify structure to point to outer scope
        free(oldattr);            // Delete the old attribute set
    } else {
        // DBUG("Attempt to pop non-existent attribute set off stack!");
        // DBUG("Ignoring likely off-by-one error.");
    }
}








/////////////////////////////////////////////////////////////////////////////
////                                                                     ////
////                          UNICODE FUNCTIONS                          ////
////                                                                     ////
/////////////////////////////////////////////////////////////////////////////

static void encode_utf8(int32_t c, char utf8[5]) {
    char *u = utf8;
    if (c < 0) {
        u[0]= '\0';
    } else 
    if (c < 0b000000000000010000000) {            // Up to 7 bits
        u[0]= (char)((c>>0  & 0b01111111) | 0b00000000);  // 7 bits –> 0xxxxxxx
        u[1]= '\0';
    } 
    else if (c < 0b000000000100000000000) {      // Up to 11 bits
        u[0]= (char)((c>>6  & 0b00011111) | 0b11000000);  // 5 bits –> 110xxxxx
        u[1]= (char)((c>>0  & 0b00111111) | 0b10000000);  // 6 bits –> 10xxxxxx
        u[2]= '\0';
    } 
    else if (c < 0b000010000000000000000) {      // Up to 16 bits
        u[0]= (char)((c>>12 & 0b00001111) | 0b11100000);  // 4 bits –> 1110xxxx
        u[1]= (char)((c>>6  & 0b00111111) | 0b10000000);  // 6 bits –> 10xxxxxx
        u[2]= (char)((c>>0  & 0b00111111) | 0b10000000);  // 6 bits –> 10xxxxxx
        u[3]= '\0';
    } 
    else if (c < 0b100010000000000000000) {      // Up to 21 bits
        u[0]= (char)((c>>18 & 0b00000111) | 0b11110000);  // 3 bits –> 11110xxx
        u[1]= (char)((c>>12 & 0b00111111) | 0b10000000);  // 6 bits –> 10xxxxxx
        u[2]= (char)((c>>6  & 0b00111111) | 0b10000000);  // 6 bits –> 10xxxxxx
        u[3]= (char)((c>>0  & 0b00111111) | 0b10000000);  // 6 bits –> 10xxxxxx
        u[4]= '\0';
    } 
    else {
        u[0]= '\0';
    }
}



static int32_t cdpt_from_utf16(uint16_t hi, uint16_t lo) {
    int32_t cdpt;
    bool hisurrogate = (0xD800 <= hi && hi <= 0xDBFF);
    bool losurrogate = (0xDC00 <= lo && lo <= 0xDFFF);

    if (hisurrogate && losurrogate) {
        // We have a valid surrogate pair, so convert it.
        // Unicode 15.0, Tbl 3-5 says UTF-16 surrogate pairs in form:
        //         110110wwwwyyyyyy
        //                   110111xxxxxxxxxx
        // map to scalar code points of value:
        //              uuuuuyyyyyyxxxxxxxxxx     (uuuuu=wwww+1)
        cdpt = ((lo & 0b000000000001111111111)) |
               ((hi & 0b00000111111)    << 10 ) |
               ((hi & 0b01111000000)    << 10 ) +
               ((     0b00001000000)    << 10 ) ;
    } else if (!hisurrogate && !losurrogate) {
        // Neither of this pair is a surrogate; lo should be a valid code
        // point in the Basic Multilingual Plane, so return that. 
        cdpt = lo;
    } else {
        // One of them is a surrogate but not both; we have an encoding
        // error. Return a question mark as a placeholder. 
        cdpt = '?';
    }
    return cdpt;
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
