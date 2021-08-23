#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "rtfrep.h"
#include "memzero.h"
#include "re.h"

#define DIE(s) (die(s, __func__, __LINE__))

void push_attr(rtfobj *R);
void pop_attr(rtfobj *R);
void mod_attr(rtfobj *R, char *cmd);
void die(char *s, const char *f, size_t l);




















///////////////////////////////
////    MAIN LOGIC LOOP    ////
///////////////////////////////



void rtfreplace(rtfobj *R) {
    int c;

    while ((c = fgetc(R->fin)) != EOF) {
        


        fputc(c, R->fout);
    }
}




















/////////////////////////////////////////
////    ATTRIBUTE STACK FUNCTIONS    ////
/////////////////////////////////////////



void push_attr(rtfobj *R) {
    rtfattr *newattr = malloc(sizeof *newattr);

    if (!newattr) DIE("Failed allocating new attribute scope.");

    if (R->attr == NULL) {
        memzero(newattr, sizeof *newattr);
        newattr->parent = NULL;
        newattr->txtable = false;
        R->attr = newattr;
    } else {
        memmove(newattr, R->attr, sizeof *newattr);
        newattr->parent = R->attr;
        R->attr = newattr;
    }
}



void pop_attr(rtfobj *R) {
    rtfattr *oldattr;

    if (!R->attr) DIE("Attempt to pop non-existent attribute set off stack!");

    oldattr = R->attr;             // Point it at the current attribute set
    R->attr = oldattr->parent;     // Modify structure to point to outer scope
    free(oldattr);                 // Delete the old attribute set
}



void mod_attr(rtfobj *R, char *cmd) {
    int reml; // RegEx Match Length

    if (re_match(cmd, "\\\\uc[0-9]*", &reml)) {
        R->attr->uc = (size_t)strtoul(&cmd[3], NULL, 10);
    } 
}




















//////////////////////////////////////////
////    CONSTRUCTORS & DESTRUCTORS    ////
//////////////////////////////////////////



rtfobj *new_rtfobj(FILE *fin, FILE *fout, char **dict) {
    rtfobj *R = malloc(sizeof *R);

    if (!R) DIE("Failed allocating new RTF Object.");

    R->fin = fin;
    R->fout = fout;
    R->attr = NULL;
    R->dict = dict;

    R->ri = 0UL;
    R->ti = 0UL;
    R->ci = 0UL;

    R->rawz = RAW_BUFFER_SIZE;
    R->txtz = TXT_BUFFER_SIZE;
    R->cmdz = CMD_BUFFER_SIZE;

    memzero(R->raw, R->rawz);
    memzero(R->txt, R->txtz);
    memzero(R->cmd, R->cmdz);

    return R;
}




















//////////////////////////////////
////    UTILITY FUNCTIONS    /////
//////////////////////////////////



void die(char *s, const char *f, size_t l) {
  fprintf(stderr, "Died on line %zu of function %s:\n    %s\n\n\n", l, f, s);
  exit(EXIT_FAILURE);
}

