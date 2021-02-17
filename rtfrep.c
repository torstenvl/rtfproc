#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "rtfrep.h"

// REMINDER OF STRUCTURE DECLARATIONS AND CONSTANTS
// typedef struct repobj {
//     FILE*  is;            // Input stream
//
//     byte   ib[FB_Z];      // Input file buffer
//     size_t ibz;           // Input buffer actual size
//     size_t ibi;           // Input buffer iterator
//
//     FILE*  os;            // Output stream
//
//     byte   ob[FB_Z];      // Output file buffer
//     size_t obz;           // Output buffer actual size
//     size_t obi;           // Output buffer iterator
//
//     byte   ibto[FB_Z];    // Input file buffer token oracle (readahead)
//     size_t ibtoi;         // Input buffer token oracle iterator
//
//     uccp   pb[PB_Z];      // Pattern buffer for comparing codepoints
//     size_t pbz;           // Pattern buffer actual size
//     size_t pbi;           // Pattern buffer iterator
//
//     size_t pbmaps[PB_Z];  // Maps to start of rtf sequence for pb[i] in ib[]
//     size_t pbmape[PB_Z];  // Maps to end of rtf sequence for pb[i] in ib[]
//
// } repobj;
// FB_Z file buffer (ib and ob) size
// PB_Z pattern buffer size


int rtf_process(repobj *r) {
    int nerrs;  // Number of errors
    int err;    // Current error

    nerrs = 0;

    while (!feof(r->is)) {

        ////////////////////////
        // SET UP OUR BUFFERS //
        ////////////////////////
        memzero(r->ob, FB_Z);
        r->ibz = fread(r->ib, 1, FB_Z, r->is);
        r->ibi = 0;
        r->obi = 0;

        /////////////////////////////////////////////////////
        // CHECK FOR INCOMPLETE BUFFER FILL → ERROR OR EOF //
        /////////////////////////////////////////////////////
        if (r->ibz < FB_Z) {
            if (ISERROR(err = ferror(r->is))) {
                fprintf(stderr, "File read error %d!\n", err);
                nerrs++;
            } else {
                // Incomplete buffer fill and no error
                // → must be end of file. Let it fall through
                // on the while() loop and end gracefully.
            }
        }

        ///////////////////////////////////////////////////////////////
        // LOOP INPUT BUFFER TO OUTPUT BUFFER, PROCESSING ON THE WAY //
        ///////////////////////////////////////////////////////////////
        while (r->ibi < r->ibz) {
            // if (r->ib[r->ibi] == '\\') { }
            // if (r->ib[r->ibi] == '{') { }
            // if (r->ib[r->ibi] == '}') { }
            r->ob[r->obi++] = r->ib[r->ibi++];
        }

        /////////////////////////////
        // FLUSH THE OUTPUT BUFFER //
        /////////////////////////////
        fwrite(r->ob, 1, r->obi, r->os);

    } // end of buffer-read loop

    return nerrs;
}



void memzero(void *const p, const size_t n)
{
  volatile unsigned char *volatile p_ = (volatile unsigned char *volatile) p;
  size_t i = (size_t) 0U;

  while (i < n) p_[i++] = 0U;
}




































/***************************************************************************/
/*                                                                         */
/*  THE FOLLOWING SECTION IMPLEMENTS BASIC OBJECT CREATION / DESTRUCTION   */
/*                                                                         */
/*  repobj *new_repobj(void);                                              */
/*  repobj *new_repobj_from_file(const char *);                            */
/*  repobj *new_repobj_to_file(const char *);                              */
/*  repobj *new_repobj_from_file_to_file(const char *, const char *);      */
/*  void destroy_repobj(repobj *);                                         */
/*                                                                         */
/***************************************************************************/





repobj *new_repobj(void) {
    return new_repobj_from_stream_to_stream(stdin, stdout);
}





repobj *new_repobj_from_file(const char *ifname) {
    FILE *ifptr;
    repobj *r;

    ifptr = fopen(ifname, "r");
    if (!ifptr) return NULL;

    r = new_repobj_from_stream_to_stream(ifptr, stdout);
    if (!r) {
        fclose (ifptr);
        return NULL;
    }

    return r;
}





repobj *new_repobj_to_file(const char *ofname) {
    FILE *ofptr;
    repobj *r;

    ofptr = fopen(ofname, "w");
    if (!ofptr) return NULL;

    r = new_repobj_from_stream_to_stream(stdin, ofptr);
    if (!r) {
        fclose(ofptr);
        return NULL;
    }

    return r;
}





repobj *new_repobj_from_file_to_file(const char *ifname, const char *ofname) {
    FILE *ifptr;
    FILE *ofptr;
    repobj *r;

    ifptr = fopen(ifname, "r");
    if (!ifptr) return NULL;

    ofptr = fopen(ofname, "w");
    if (!ofptr) {
        fclose(ifptr);
        return NULL;
    }

    r = new_repobj_from_stream_to_stream(ifptr, ofptr);
    if (!r) {
        fclose(ifptr);
        fclose(ofptr);
        return NULL;
    }

    return r;
}





repobj *new_repobj_from_stream_to_stream(FILE *ifstream, FILE *ofstream) {
    repobj *r;

    r = malloc(sizeof(*r));
    if (!r) return NULL;

    memzero(r, sizeof *r);
    r->is = ifstream;
    r->os = ofstream;

    return r;
}





void destroy_repobj(repobj *r) {
    if (r->is != stdin) fclose(r->is);
    if (r->os != stdout) fclose(r->os);

    memzero(r, sizeof *r);

    free(r);
}
