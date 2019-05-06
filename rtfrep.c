#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "rtfrep.h"

// DESIGN DECISIONS
// With a sufficiently-sized file buffer, then so long as our RTF file and
// key tokens are not insane, we shouldn't have too many matches that span
// buffer reads.  Since invalidating our buffers will lead to needless disk
// thrashing, we will allocate a relatively large buffer on the stack and
// assume all key token matches will be found within the buffer; those that
// do not can be handled in a special case.  Allocating a large buffer on the
// stack shouldn't have negative performance implications here, since it seems
// inconceivable (INCONCEIVABLE!) that this function will ever be called in
// any sort of recursive fashion.


// Macros
#define ISERROR(x) ((x)!=0)
#define partialmatch(x, y) ((x) == (y[0]))


// Encapsulated function prototypes
int tokencheck(char *, size_t, char *, size_t, char *);





int rtf_replace(FILE *in_stream, FILE *out_stream, char *key, char *replace) {

  // Variable declarations

  int    errflags = 0;      // return 0 for success, anything else for error
  int    matchlength;
  int    filestatus;
  size_t readsize;
  size_t buffersize;
  size_t remnantsize;
  size_t remnantmax;
  size_t i;


  // Buffer size constants are in rtf_replace.h

  char filebuffer[FILE_BUFFER_SIZE];
  char tokenbuffer[TOKEN_BUFFER_SIZE];


  // Main loop - continue until the input stream reaches end-of-file

  while (!feof(in_stream)) {

    // Try to read as much as we can into the filebuffer
    // fread(char *buffer, size_t size, size_t maxnumber, FILE *stream)

    readsize = fread(filebuffer, 1, FILE_BUFFER_SIZE, in_stream);

    // If we can't fill the buffer, we either have an error or have reached
    // the end of the file.  Log an error, if any, and process what we have.

    if (readsize < FILE_BUFFER_SIZE) {
      filestatus = ferror(in_stream);
      if (ISERROR(filestatus)) {
        fprintf(stderr, "File read error %d!\n", filestatus);
        errflags++;
      }
    }

    buffersize = readsize;

    // Loop through the file buffer looking for partial matches.

    for (i = 0; i < buffersize; i++) {

      if (partialmatch(filebuffer[i], key)) {

        // We have a partial match, but we don't know how long the matching
        // text is or what the replacement text should be.  We also don't know
        // if the full amount of the matching text is in our current file
        // buffer. So here's what we're going to do:
        //   - If we're already more than halfway through processing our
        //     file buffer, we're going to output it and get more from the
        //     file.
        //   - Once we know we have a good-sized file buffer to check the
        //     match against, we will see if we have a full match; if so,
        //     we will output the replacement text rather than the file text;
        //     and if not, then continue processing the buffer (we will
        //     eventually output the false positive later, when we have a full
        //     match, or when we have a partial match in the second half of
        //     a file buffer, or when we have an entire file buffer with no
        //     matches).

        // If we've looped through most of the file buffer, there might not
        // be enough file text to tell if we have a full match or not.  Output
        // what we have, move the unprocessed part of the buffer to the
        // beginning, and fill up the now-empty part of the buffer as much as
        // we can.
        if ((i * 2) > buffersize) {
          remnantsize = buffersize - i;
          remnantmax  = FILE_BUFFER_SIZE - i;
          fwrite(filebuffer, 1, i, out_stream);
          memmove(filebuffer, filebuffer + i, remnantsize);
          readsize = fread(filebuffer + remnantsize, 1, remnantmax, in_stream);
          if (readsize < remnantmax) {
            filestatus = ferror(in_stream);
            if (ISERROR(filestatus)) {
              fprintf(stderr, "File read error %d!\n", filestatus);
              errflags++;
            }
          }
          buffersize += readsize;
          i = 0;
        }

        // See if we have a full match, and if so, get the length of the
        // matching file text.  This will have to be retooled to return
        // a character buffer, in all likelihood.
        matchlength = tokencheck(filebuffer + i, buffersize - i,
                tokenbuffer,    TOKEN_BUFFER_SIZE,
                key);

        // If we have a match, then output the appropriate replacement token.
        // TODO:  If we have multiple tokens, we will have to find out WHICH
        // token was matched ... the lines between this iterative function
        // and the token-checking function need to be shifted.
        if (matchlength > 0) {
          fwrite(replace, 1, strlen(replace), out_stream);
          memmove(filebuffer, filebuffer + i + matchlength,
            buffersize - (i + matchlength));
          buffersize = buffersize - (i + matchlength);
          i = 0;
        }
      }
    }

    // At this point we have iterated through and handled any potential matches
    // in the file buffer, which means whatever is left in our buffer is good
    // to output.
    fwrite(filebuffer, 1, buffersize, out_stream);

  } // end of buffer

  return errflags;
}





int tokencheck(char *fbuf, size_t fbufsz,
	       char *tbuf, size_t tbufsz,
	       char *key) {

  // Iterators for the file buffer and token buffer
  int i = 0;
  int j = 0;

  // Return value is the number of matching bytes, or -1
  int retval;

  // For easy reference
  size_t keylen = strlen(key);

  // State variables
  // sp         - the next whitespace character ends a command, ergo is 0x20;
  //              particularly important for \n which is otherwise ignored.
  // uc         -
  bool sp = false;


  while (1) {

    // Check if our next newline should count as a space
    if (fbuf[i] == '\\') {
      sp = true;
    }

    // Check for poorly translated MS-DOS CRLFs, just in case; then check
    // for proper newlines
    if ((i < (fbufsz-1)) && (fbuf[i] == 13) && (fbuf[i+1] == 10)) {
      tbuf[j++] = (sp) ? ' ' : fbuf[i];
      if (sp) i++;
      sp = false;
    } else if (isspace(fbuf[i])) {
      tbuf[j++] = (sp) ? ' ' : fbuf[i];
      sp = false;
    } else {
      tbuf[j++] = fbuf[i];
    }

    i++;

    if (i >= fbufsz) break;
    if (j >= (tbufsz-1)) break;
    if (j >= keylen) break;
  }

  tbuf[j] = '\0';

  if (!strcmp(tbuf, key)) {
    retval = i;
  } else if (strlen(tbuf) < keylen) {
    retval = INDETERMINATE;
  } else {
    retval = TOKENMISMATCH;
  }

  return retval;
}
