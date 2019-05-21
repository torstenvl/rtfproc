#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define ISERROR(x)        ((x) != 0)

#define FILENAME          "test.rtf"
#define FILE_BUFFER_SIZE  512

int main(void) {

  // Variable declarations
  char filebuffer[FILE_BUFFER_SIZE];
  FILE  *in_stream;
  FILE  *out_stream = stdout;
  size_t readsize;
  int filestatus = 0;
  int errflags = 0;

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

    for (size_t i = 0; i < readsize; i++) {
      if (filebuffer[i] == '\\') {
        // The next thing we have is an RTF code of some sort
        fprintf(stderr, "Found \\\t");
      }
      if (filebuffer[i] == '{') {
        // Opening a new RTF context
        fprintf(stderr, "Found \\\t");
      }
      if (filebuffer[i] == '}') {
        // Closing an RTF context
        fprintf(stderr, "Found \\\t");
      }
    }

    fwrite(filebuffer, 1, readsize, out_stream);

  } // end of buffer-read loop

  fprintf(stderr, "\n\n");

  return errflags;
}
