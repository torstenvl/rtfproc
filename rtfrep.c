#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "rtfrep.h"
#include "memzero.h"
#include "re.h"



#define DIE(s) (die(s, __func__, __LINE__))

void die(char *s, const char *f, size_t l) {
  fprintf(stderr, "Died on line %zu of function %s:\n    %s\n\n\n", l, f, s);
  exit(EXIT_FAILURE);
}



#define RE_DOT_MATCHES_NEWLINE 1
typedef struct regex_t* re_t;
re_t re_compile(const char* pattern);
int  re_matchp(re_t pattern, const char* text, int* matchlength);
int  re_match(const char* pattern, const char* text, int* matchlength);



void rtf_get_next_token(rtfobj *, rtftoken *);
void rtf_output(rtfobj *, char *);
void push_attr(rtfobj *);
void pop_attr(rtfobj *);
void mod_attr(rtfobj *, rtftoken *);



void rtf_replace(rtfobj *R) {
    rtftoken *T = malloc(sizeof(*T));

    while (T) {
        rtf_get_next_token(R, T);
        switch (T->type) {
            SCOPE_OPEN:
                rtf_output(R, T);
                push_attr(R);
                break;
            SCOPE_CLOSE:
                rtf_output(R, T);
                pop_attr(R);
                // If we closed scope of entire document, quit processing
                if (R->attr == NULL) { free(T); T = NULL; }
                break;
            IMM_CMD:
            COMPLEX_CMD:
                rtf_output(R, T);
                mod_attr(R, T);
                break;
            TEXT:
                rtf_output(R, T);
                break;
            FUCK:
                free(T);
                T = NULL;
        }
    }
}




rtfobj *new_rtfobj(FILE *in, FILE *out, char **dict) {
    rtfobj *R = malloc(sizeof *R);

    if (!R) DIE("Failed allocating new RTF Object.");

    R->is = in;
    R->os = out;
    R->attr = NULL;
    R->dict = dict;
    memzero(R->raw, RAW_BUFFER_SIZE);
    memzero(R->mtc, MTC_BUFFER_SIZE);

    return R;
}




void push_attr(rtfobj *R) {
    rtfattribs *new = malloc(sizeof *new);

    if (!new) DIE("Failed allocating new attribute scope.");

    if (R->attr) {
        // If we already have an RTF attribute set stack, then insert the new
        // RTF attribute set and line up our pointers.
        memmove(attr, R->attr, sizeof *attr);
        attr->parent = R->attr;
        R->attr = attr;
    } else {
        // Otherwise we are the lucky first RTF attribute set. We don't have
        // a parent (outer scope attribute set) and our attributes are
        // starting from nothing.
        memzero(attr, sizeof *attr);
        attr->parent = NULL;
        R->attr = attr;
    }
}



void pop_attr(rtfobj *R) {
    rtfattribs *oldattr;

    if (!R->attr) DIE("Attempt to pop non-existent attribute set off stack!");

    oldattr = R->attr;             // Point it at the current attribute set
    R->attr = oldattr->parent;     // Modify structure to point to outer scope
    free(oldattr);                 // Delete the old attribute set
}



void mod_attr(rtfobj *R, rtftoken *T) {
    // Will match the token against different commands and see if there's
    // anything relevant to us.

    int ml;       // Regex match length
    char *txt = T->contents;

    if (re_match(txt, "\\\\uc[0-9]*", &ml)) {
        R->attr->uc = (size_t)strtoul(&txt[3], NULL, 10);
    } else if (re_match(txt, "\\\\ansi", &ml)) {
        R->attr->cp = ANSI;
    } else if (re_match(txt, "\\\\mac", &ml)) {
        R->attr->cp = MAC;
    } else if (re_match(txt, "\\\\pc", &ml)) {
        R->attr->cp = PC;
    } else if (re_match(txt, "\\\\pca", &ml)) {
        R->attr->cp = PCA;
    } else if (re_match(txt, "\\\\CP_437", &ml)) {
        R->attr->cp = CP_437;
    } else if (re_match(txt, "\\\\CP_708", &ml)) {
        R->attr->cp = CP_708;
    } else if (re_match(txt, "\\\\CP_709", &ml)) {
        R->attr->cp = CP_709;
    } else if (re_match(txt, "\\\\CP_710", &ml)) {
        R->attr->cp = CP_710;
    } else if (re_match(txt, "\\\\CP_711", &ml)) {
        R->attr->cp = CP_711;
    } else if (re_match(txt, "\\\\CP_720", &ml)) {
        R->attr->cp = CP_720;
    } else if (re_match(txt, "\\\\CP_819", &ml)) {
        R->attr->cp = CP_819;
    } else if (re_match(txt, "\\\\CP_850", &ml)) {
        R->attr->cp = CP_850;
    } else if (re_match(txt, "\\\\CP_852", &ml)) {
        R->attr->cp = CP_852;
    } else if (re_match(txt, "\\\\CP_860", &ml)) {
        R->attr->cp = CP_860;
    } else if (re_match(txt, "\\\\CP_862", &ml)) {
        R->attr->cp = CP_862;
    } else if (re_match(txt, "\\\\CP_863", &ml)) {
        R->attr->cp = CP_863;
    } else if (re_match(txt, "\\\\CP_864", &ml)) {
        R->attr->cp = CP_864;
    } else if (re_match(txt, "\\\\CP_865", &ml)) {
        R->attr->cp = CP_865;
    } else if (re_match(txt, "\\\\CP_866", &ml)) {
        R->attr->cp = CP_866;
    } else if (re_match(txt, "\\\\CP_874", &ml)) {
        R->attr->cp = CP_874;
    } else if (re_match(txt, "\\\\CP_932", &ml)) {
        R->attr->cp = CP_932;
    } else if (re_match(txt, "\\\\CP_936", &ml)) {
        R->attr->cp = CP_936;
    } else if (re_match(txt, "\\\\CP_949", &ml)) {
        R->attr->cp = CP_949;
    } else if (re_match(txt, "\\\\CP_950", &ml)) {
        R->attr->cp = CP_950;
    } else if (re_match(txt, "\\\\CP_1250", &ml)) {
        R->attr->cp = CP_1250;
    } else if (re_match(txt, "\\\\CP_1251", &ml)) {
        R->attr->cp = CP_1251;
    } else if (re_match(txt, "\\\\CP_1252", &ml)) {
        R->attr->cp = CP_1252;
    } else if (re_match(txt, "\\\\CP_1253", &ml)) {
        R->attr->cp = CP_1253;
    } else if (re_match(txt, "\\\\CP_1254", &ml)) {
        R->attr->cp = CP_1254;
    } else if (re_match(txt, "\\\\CP_1255", &ml)) {
        R->attr->cp = CP_1255;
    } else if (re_match(txt, "\\\\CP_1256", &ml)) {
        R->attr->cp = CP_1256;
    } else if (re_match(txt, "\\\\CP_1257", &ml)) {
        R->attr->cp = CP_1257;
    } else if (re_match(txt, "\\\\CP_1258", &ml)) {
        R->attr->cp = CP_1258;
    } else if (re_match(txt, "\\\\CP_1361", &ml)) {
        R->attr->cp = CP_1361;
    } else if (re_match(txt, "\\\\CP_10000", &ml)) {
        R->attr->cp = CP_10000;
    } else if (re_match(txt, "\\\\CP_10001", &ml)) {
        R->attr->cp = CP_10001;
    } else if (re_match(txt, "\\\\CP_10004", &ml)) {
        R->attr->cp = CP_10004;
    } else if (re_match(txt, "\\\\CP_10005", &ml)) {
        R->attr->cp = CP_10005;
    } else if (re_match(txt, "\\\\CP_10006", &ml)) {
        R->attr->cp = CP_10006;
    } else if (re_match(txt, "\\\\CP_10007", &ml)) {
        R->attr->cp = CP_10007;
    } else if (re_match(txt, "\\\\CP_10029", &ml)) {
        R->attr->cp = CP_10029;
    } else if (re_match(txt, "\\\\CP_10081", &ml)) {
        R->attr->cp = CP_10081;
    } else if (re_match(txt, "\\\\CP_57002", &ml)) {
        R->attr->cp = CP_57002;
    } else if (re_match(txt, "\\\\CP_57003", &ml)) {
        R->attr->cp = CP_57003;
    } else if (re_match(txt, "\\\\CP_57004", &ml)) {
        R->attr->cp = CP_57004;
    } else if (re_match(txt, "\\\\CP_57005", &ml)) {
        R->attr->cp = CP_57005;
    } else if (re_match(txt, "\\\\CP_57006", &ml)) {
        R->attr->cp = CP_57006;
    } else if (re_match(txt, "\\\\CP_57007", &ml)) {
        R->attr->cp = CP_57007;
    } else if (re_match(txt, "\\\\CP_57008", &ml)) {
        R->attr->cp = CP_57008;
    } else if (re_match(txt, "\\\\CP_57009", &ml)) {
        R->attr->cp = CP_57009;
    } else if (re_match(txt, "\\\\CP_57010", &ml)) {
        R->attr->cp = CP_57010;
    } else if (re_match(txt, "\\\\CP_57011", &ml)) {
        R->attr->cp = CP_57011;
    }
}




void rtf_get_next_token(rtfobj *R, rtftoken *T) {
    int c;       // Character input from stream
    int ml;      // Regex match length
    char *txt = T->contents
    size_t i = 0;

    memzero(T->contents, TKN_BUFFER_SIZE);
    T->type = FUCK;

    while ((c = fgetc(R->is)) != EOF) {
        txt[i++] = (char)c;
    }

    if (re_match(txt, "^{", &ml) && ml == 1) {
        T->type = SCOPE_OPEN;
    } else if (re_match(txt, "^}", &ml) && ml == 1) {
        T->type = SCOPE_CLOSE;
    } else if (re_match(txt, "^\\\\\\w", &ml)) {
        T->type = IMM_CMD;
