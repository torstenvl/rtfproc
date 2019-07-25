#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ISERROR(x)        ((x) != 0)

#define FILENAME          "test.rtf"
#define FILE_BUFFER_SIZE  512
#define ORACLE_BUFFER_SIZE 128

int main(void) {

  // Variable declarations
  char filebuffer[FILE_BUFFER_SIZE];
  char oraclebuffer[ORACLE_BUFFER_SIZE];
  FILE  *in_stream;
  FILE  *out_stream = stdout;
  size_t readsize;
  int filestatus = 0;
  int errflags = 0;

  size_t i;
  size_t oracle;
  size_t stackdepth = 0;
  size_t maxstackdepth = 0;

  // Open the file
  in_stream = fopen(FILENAME, "rb");

  if (NULL == in_stream) {
    fprintf(stderr, "Unable to open file '%s' for reading.\n", FILENAME);
    exit(EXIT_FAILURE);
  }

  while (!feof(in_stream)) {

    readsize = fread(filebuffer, 1, FILE_BUFFER_SIZE, in_stream);

    // If we can't fill the buffer, we either have an error or have reached
    // the end of the file.
    if (readsize < FILE_BUFFER_SIZE) {
      filestatus = ferror(in_stream);
      if (ISERROR(filestatus)) {
        // If it's an error, log it and move on.
        fprintf(stderr, "File read error %d!\n", filestatus);
        errflags++;
      } else {
        // End of the file was reached.  We might still have some good data
        // though, so we will keep processing/writing.
      }
    }

    for (i = 0; i < readsize; i++) {
      if (filebuffer[i] == '\\') {
        // The next thing we have is an RTF code of some sort
        // The oracle cursor looks into the near future.  Start just past i
        // and read until we reach the end of the RTF code token.
        // RTF code tokens are terminated by a backslash, whitespace, or
        // the start or end of an RTF block (delimited by { and }).
        // FRAGILE:  WHAT IF WE EXHAUST A BUFFER?
        memset(oraclebuffer, 0, ORACLE_BUFFER_SIZE);
        for (oracle=1;
             oracle < (ORACLE_BUFFER_SIZE-1) && oracle+i < readsize;
             oracle++) {
          if (filebuffer[i+oracle] == '\\') break;
          if (isspace(filebuffer[i+oracle])) break;
          if (filebuffer[i+oracle] == '}') break;
          if (filebuffer[i+oracle] == '{') break;
          if (oraclebuffer[0] == '\'' && oracle > 3) break;
          oraclebuffer[oracle-1] = filebuffer[i+oracle];
        }
        fprintf(stderr, "Found RTF command token: %s\n", oraclebuffer);
      }
      if (filebuffer[i] == '{') {
        stackdepth++;
        if (stackdepth > maxstackdepth) maxstackdepth = stackdepth;
      }
      if (filebuffer[i] == '}') {
        if (stackdepth == 0) fprintf(stderr, "TRYING TO DECREASE STATE STRUCT STACK DEPTH BELOW ZERO\n\n");
        stackdepth--;
      }
    }

    fwrite(filebuffer, 1, readsize, out_stream);

  } // end of buffer-read loop

  fprintf(stderr, "\n\n");

  return errflags;
}
