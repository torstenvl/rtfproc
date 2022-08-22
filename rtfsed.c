// ---------------------------------------------------------------------------- 
// TODO
//   - Implement a font table. Because document fonttbl entries are not 
//     guaranteed to be in consecutive ordinal order, it may be highly
//     space-inefficient to store font table entries in a sparse array,
//     though it would give O(1) access. The next best way might be to store
//     them in a dense sorted array of fonttbl entries, with insertion/
//     insertion sort and binary search retrieval. This should give O(log N)
//     access. 
//
//   - Add attributes for \ansicpgN, font tables with \fcharsetN and/or
//     \cpgN, and \cchsN. Interpret \'xx according to current character set
//     or code page. 
//
//   - Normalize replacement tokens as well as any Unicode in the text before
//     comparison.  Use NFC (or maybe NFD?) normalization.
//   
//   - Add additional areas where we skip blocks for large data types.  This
//     should, at a minimum, include \binN
// ---------------------------------------------------------------------------- 



// NOTES
// {\fonttbl\f0\fmodern\fcharset0 Courier;}
// {\fonttbl
// {\f0\fbidi \froman\fcharset0\fprq2{\*\panose 02020603050405020304}Times New Roman;}
// {\f37\fbidi \fswiss\fcharset0\fprq2{\*\panose 020f0502020204030204}Calibri;}
// {\flomajor\f31500\fbidi \froman\fcharset0\fprq2{\*\panose 02020603050405020304}Times New Roman;}
// {\fdbmajor\f31501\fbidi \froman\fcharset0\fprq2{\*\panose 02020603050405020304}Times New Roman;}
// {\fhimajor\f31502\fbidi \fswiss\fcharset0\fprq2{\*\panose 020f0302020204030204}Calibri Light;}
// {\fbimajor\f31503\fbidi \froman\fcharset0\fprq2{\*\panose 02020603050405020304}Times New Roman;}
// {\flominor\f31504\fbidi \froman\fcharset0\fprq2{\*\panose 02020603050405020304}Times New Roman;}
// {\fdbminor\f31505\fbidi \froman\fcharset0\fprq2{\*\panose 02020603050405020304}Times New Roman;}
// {\fhiminor\f31506\fbidi \fswiss\fcharset0\fprq2{\*\panose 020f0502020204030204}Calibri;}
// {\fbiminor\f31507\fbidi \froman\fcharset0\fprq2{\*\panose 02020603050405020304}Times New Roman;}
// {\f42\fbidi \froman\fcharset238\fprq2 Times New Roman CE;}

// {\flomajor\f31514\fbidi \froman\fcharset178\fprq2 Times New Roman (Arabic);}
// {\fdbmajor\f31524\fbidi \froman\fcharset178\fprq2 Times New Roman (Arabic);}
// {\fhimajor\f31534\fbidi \fswiss\fcharset178\fprq2 Calibri Light (Arabic);}
// {\fbimajor\f31544\fbidi \froman\fcharset178\fprq2 Times New Roman (Arabic);}
// {\flominor\f31554\fbidi \froman\fcharset178\fprq2 Times New Roman (Arabic);}
// {\fdbminor\f31564\fbidi \froman\fcharset178\fprq2 Times New Roman (Arabic);}
// {\fhiminor\f31574\fbidi \fswiss\fcharset178\fprq2 Calibri (Arabic);}
// {\fbiminor\f31584\fbidi \froman\fcharset178\fprq2 Times New Roman (Arabic);}
// }





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
static void proc_cmd_ignoreblock(rtfobj *R);
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
static void reset_buffers(rtfobj *R);
static void reset_raw_buffer(rtfobj *R);
static void reset_txt_buffer(rtfobj *R);
static void reset_cmd_buffer(rtfobj *R);
static void skip_thru_block(rtfobj *R);

static void encode_utf8(int32_t c, char utf8[5]);
static void memzero(void *const p, const size_t n);
static int32_t get_num_arg(const char *s);
static int32_t get_hex_arg(const char *s);

// Macros

// Macros specific to proc_command()
#define REGEX_MATCH(x, y)    (re_match(y, x, NULL) > -1)
#define STRING_MATCH(x, y)   (!strcmp(x, y))

// Macros for logging and debugging
#define RTFDEBUG
#ifdef RTFDEBUG
    #define DBUG(...) (LOG(__VA_ARGS__))
#else
    #define DBUG(...) ((void)0)
#endif
#define LOG(...) (fprintf(stderr, "In rtfsed.c:%s() on line %d: ", \
                  __func__, __LINE__) \
                  && fprintf(stderr, __VA_ARGS__ ) \
                  && fprintf(stderr, "\n"))




//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////             CONSTRUCTOR/DESTRUCTOR FOR NEW RTF OBJECTS               ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////

rtfobj *new_rtfobj(FILE *fin, FILE *fout, const char **srch) {
    size_t i;
    rtfobj *R = malloc(sizeof *R);

    if (!R) {
        LOG("Failed allocating new RTF Object.");
        return NULL;
    }

    // Set up file streams    
    R->fin = fin;
    R->fout = fout;

    // 
    R->fatalerr = 0;

    // Set up replacement dictionary
    for (i=0; srch[i] != NULL; i++); 
    R->srchz = i/2;
    R->srch_max_keylen = 0UL;
    R->srch_match = 0UL; 

    // Allocate two lists of pointers
    R->srch_key = malloc(R->srchz * sizeof *R->srch_key);
    R->srch_val = malloc(R->srchz * sizeof *R->srch_val);
    if ((!R->srch_key) || (!R->srch_val)) { 
        delete_rtfobj(R);
        LOG("Out of memory allocating search key/value pointers!");
        return NULL; 
    }

    // Assign those pointers to the strings we were passed. 
    for (i=0; i < R->srchz; i++) {
        R->srch_key[i] = srch[2*i];
        R->srch_val[i] = srch[2*i+1];
        if (R->srch_max_keylen < strlen(R->srch_key[i])) {
            R->srch_max_keylen = strlen(R->srch_key[i]);
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
            case '\\':          dispatch_command(R);       break;
            default:            dispatch_text(c, R);       break;
        }

        if (R->fatalerr)  {  LOG("Encountered a fatal error");  return;  }

        switch (pattern_match(R)) {
            case PARTIAL:        /* Keep reading */        break;
            case MATCH:          output_match(R);          break;
            case NOMATCH:        output_raw(R);            break;
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
    add_to_raw('\\', R);
    read_command(R);
    proc_command(R);
}


static void dispatch_text(int c, rtfobj *R) {
    // Ignore newlines and carriage returns in RTF code. Consider tabs and
    // vertical tabs to be interchangeable with spaces. Treat everything else
    // literally. 
    if      (c == '\n') {     add_to_raw(c, R);                              }
    else if (c == '\r') {     add_to_raw(c, R);                              } 
    else if (c == '\t') {     add_to_raw(c, R);      add_to_txt(' ', R);     }
    else if (c == '\v') {     add_to_raw(c, R);      add_to_txt(' ', R);     }
    else {                    add_to_raw(c, R);      add_to_txt(c, R);       }
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
    for (i = 0; i < R->srchz; i++) {
        if (!strcmp(R->txt, R->srch_key[i])) {
            R->srch_match = i;
            return MATCH;
        }
    }

    // Check for partial matches at current offset
    for (i = 0; i < R->srchz; i++) {
        if (!strncmp(R->txt, R->srch_key[i], R->ti)) {
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
    //        Instead, we could iterate through and discover that 
    //             strncmp(&R->txt[2], R->srch_key[?], R->ti-2)
    //
    //        However, unclear what to do then since we don't track what
    //        part of the text buffer corresponds to what part of the raw
    //        buffer. 

    //        IDEA: COULD HAVE AN ARRAY OF SIZE_Ts THE SAME SIZE AS txt[]
    //              THAT MAPS TO THE CORRESPONDING STARTING INDEX IN raw[]

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

    if ((c=fgetc(R->fin)) == EOF) { LOG("Unexpected EOF"); R->fatalerr = EIO; return; }

    switch (c) {
        case '{':  // Escaped literal
        case '}':  // Escaped literal
        case '\\': // Escaped literal
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

            if ((c=fgetc(R->fin)) == EOF) { LOG("Unexpected EOF"); R->fatalerr = EIO; break; }
            add_to_cmd(c, R);
            add_to_raw(c, R);

            if ((c=fgetc(R->fin)) == EOF) { LOG("Unexpected EOF"); R->fatalerr = EIO; break; }
            add_to_cmd(c, R);
            add_to_raw(c, R);

            break;
        default:
            if (!isalnum(c)) { LOG("Invalid command format..."); R->fatalerr = EINVAL; break; }
            add_to_cmd(c, R);
            add_to_raw(c, R);

            // Greedily add input bytes to command buffer, so long as they're valid
            while(isalnum(c = fgetc(R->fin))) {
                add_to_cmd(c, R);
                add_to_raw(c, R);
            }

            // Stopped getting valid command bytes. What now? Depends on if it's an EOF 
            // or a space or something else. "Something else" is probably a backslash
            // for the next command, so we need to put it back on the input stream.
            if (isspace(c))     add_to_raw(c, R);
            else if (c == EOF)  LOG("Unexpected EOF") && (R->fatalerr = EIO);
            else                ungetc(c, R->fin);
            break;
    }
}


static void proc_command(rtfobj *R) {
    if (0); 
    else if (STRING_MATCH(R->cmd, "\\"))                        proc_cmd_escapedliteral(R);
    else if (STRING_MATCH(R->cmd, "{"))                         proc_cmd_escapedliteral(R);
    else if (STRING_MATCH(R->cmd, "}"))                         proc_cmd_escapedliteral(R);
    else if (STRING_MATCH(R->cmd, "*"))                         proc_cmd_asterisk(R);
    else if (REGEX_MATCH(R->cmd, "^uc[0-9]+$"))                 proc_cmd_uc(R);
    else if (REGEX_MATCH(R->cmd, "^u-?[0-9]+$"))                proc_cmd_u(R);
    else if (REGEX_MATCH(R->cmd, "^\'[0-9A-Fa-f][0-9A-Fa-f]$")) proc_cmd_apostrophe(R);
    else if (REGEX_MATCH(R->cmd, "^fonttbl$"))                  proc_cmd_fonttbl(R);
    else if (REGEX_MATCH(R->cmd, "^pict$"))                     proc_cmd_ignoreblock(R);
    else if (REGEX_MATCH(R->cmd, "^colortbl$"))                 proc_cmd_ignoreblock(R);
    else if (REGEX_MATCH(R->cmd, "^stylesheet$"))               proc_cmd_ignoreblock(R);
    else if (REGEX_MATCH(R->cmd, "^operator$"))                 proc_cmd_ignoreblock(R);
    else if (REGEX_MATCH(R->cmd, "^bin$"))                      proc_cmd_ignoreblock(R);
    else if (REGEX_MATCH(R->cmd, "^par$"))                      proc_cmd_newpar(R);
    else if (REGEX_MATCH(R->cmd, "^line$"))                     proc_cmd_newline(R);
    else if (1)                                                 proc_cmd_unknown(R);
}

static void proc_cmd_escapedliteral(rtfobj *R) {
    add_to_txt(R->cmd[0], R);
    R->attr->ignorable = false;
}

static void proc_cmd_asterisk(rtfobj *R) {
    R->attr->ignorable = true;
}

static void proc_cmd_uc(rtfobj *R) {
    R->attr->uc = (size_t)get_num_arg(R->cmd);
    R->attr->ignorable = false;
}

static void proc_cmd_u(rtfobj *R) {
    char utf8[5];
    encode_utf8(get_num_arg(R->cmd), utf8);
    add_string_to_txt(utf8, R);
    R->attr->uc0i = R->attr->uc;
    R->attr->ignorable = false;
}

static void proc_cmd_apostrophe(rtfobj *R) {
    char utf8[5];
    if (R->attr->uc0i) {
        R->attr->uc0i--;
    } else {
        // TODO: CONVERT FROM CODE PAGE TO UNICODE
        encode_utf8(get_hex_arg(R->cmd), utf8);
        add_string_to_txt(utf8, R);
    }
    R->attr->ignorable = false;
}

static void proc_cmd_fonttbl(rtfobj *R) {
    output_raw(R);
    reset_buffers(R);    
    for (int braces = 1, clast = 0, c = 0; braces > 0; ) {
        c = fgetc(R->fin);
        if (c == EOF) { R->fatalerr = EIO; return; }
        if (c == '{' && clast != '\\') braces++;
        if (c == '}' && clast != '\\') braces--;
        fputc(c, R->fout);
        clast = c;
    }
    R->attr->ignorable = false;
}

static void proc_cmd_ignoreblock(rtfobj *R) {
    skip_thru_block(R);
    R->attr->ignorable = false;
}

static void proc_cmd_newpar(rtfobj *R) {
    add_to_txt('\n', R);
    add_to_txt('\n', R);
    R->attr->ignorable = false;
}

static void proc_cmd_newline(rtfobj *R) {
    add_to_txt('\n', R);
    R->attr->ignorable = false;
}

static void proc_cmd_unknown(rtfobj *R) {
    if (R->attr->ignorable) {
        skip_thru_block(R);
        R->attr->ignorable = false;
    }
}




//////////////////////////////////////////////////////////////////////////////
////                                                                      ////
////                     PROCESSING BUFFER FUNCTIONS                      ////
////                                                                      ////
//////////////////////////////////////////////////////////////////////////////

static void add_to_txt(int c, rtfobj *R) {
    if (R->ti+1 >= R->txtz) {
        LOG("Exhausted txt buffer.");
        LOG("R->ti = %zu. Last txt data: \'%s\'", R->ti, &R->txt[R->ti-80]);
        LOG("No match within limits. Flushing buffers, attempting recovery");
        output_raw(R);
        reset_buffers(R);
    }
    if (R->attr->uc0i) {
        R->attr->uc0i--;
    } else {
        R->txt[R->ti++] = (char)c;
    }
}


static void add_string_to_txt(const char *s, rtfobj *R) {
    if (R->ti+strlen(s) >= R->txtz) {
        LOG("Exhausted txt buffer.");
        LOG("R->ti = %zu. Last txt data: \'%s\'", R->ti, &R->txt[R->ti-80]);
        LOG("No match within limits. Flushing buffers, attempting recovery");
        output_raw(R);
        reset_buffers(R);
    }

    while (*s) add_to_txt((int)(*s++), R);
}


static void add_to_cmd(int c, rtfobj *R) {
    if (R->ci+1 >= R->cmdz) {
        LOG("Exhausted cmd buffer.");
        LOG("R->ci = %zu. Last cmd data: \'%s\'", R->ci, &R->cmd[R->ci-80]);
        LOG("No match within limits. Flushing buffers, attempting recovery");
        output_raw(R);
        reset_buffers(R);
    }
    R->cmd[R->ci++] = (char)c;
}


static void add_to_raw(int c, rtfobj *R) {
    if (R->ri+1 >= R->rawz) {
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

    reset_buffers(R);
}


static void output_raw(rtfobj *R) {
    fwrite(R->raw, 1, R->ri, R->fout);
    reset_buffers(R);
}


static void skip_thru_block(rtfobj *R) {
    output_raw(R);
    reset_buffers(R);
    
    for (int braces = 1, clast = 0, c = 0; braces > 0; ) {
        c = fgetc(R->fin);
        if (c == EOF) { R->fatalerr = EIO; return; }
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
        newattr->ignorable = false;
        newattr->cpg = CPG_1252;
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
    if (c < 0x80) {
        // (__ & 01111111)|00000000  ==> 0xxx xxxx
        utf8[0] = (char)(c>>0  &  0x7F) | (char)0x00;  
        utf8[1] = '\0';
    }
    else if (c < 0x0800) {
        // (__ & 00011111)|11000000  ==> 110x xxxx
        // (__ & 00111111)|10000000  ==> 10xx xxxx
        utf8[0] = (char)(c>>6 & 0x1F) | (char)0xC0;
        utf8[1] = (char)(c>>0 & 0x3F) | (char)0x80;
        utf8[2] = '\0';
    }
    else if (c < 0x010000) {
        // (__ & 00001111)|11100000  ==> 1110 xxxx
        // (__ & 00111111)|10000000  ==> 10xx xxxx
        // (__ & 00111111)|10000000  ==> 10xx xxxx
        utf8[0] = (char)(c>>12 &  0x0F) | (char)0xE0; 
        utf8[1] = (char)(c>>6  &  0x3F) | (char)0x80;
        utf8[2] = (char)(c>>0  &  0x3F) | (char)0x80;
        utf8[3] = '\0';
    }
    else if (c < 0x110000) {
        // (__ & 00000111)|11110000  ==> 1111 0xxx
        // (__ & 00111111)|10000000  ==> 10xx xxxx
        // (__ & 00111111)|10000000  ==> 10xx xxxx
        // (__ & 00111111)|10000000  ==> 10xx xxxx
        utf8[0] = (char)(c>>18 &  0x07) | (char)0xF0;
        utf8[1] = (char)(c>>12 &  0x3F) | (char)0x80;
        utf8[2] = (char)(c>>6  &  0x3F) | (char)0x80;
        utf8[3] = (char)(c>>0  &  0x3F) | (char)0x80;
        utf8[4] = '\0';
    }
}


static void memzero(void *const p, const size_t n) {
    volatile unsigned char *volatile p_ = (volatile unsigned char *volatile) p;
    for (; p_  <  (volatile unsigned char *volatile) p + n; p_++) {
        *p_ = 0U;
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
