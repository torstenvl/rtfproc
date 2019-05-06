#ifndef RTFPROC_H
#define RTFPROC_H

// let's go ahead and read 16 512-byte blocks at a time
#define FILE_BUFFER_SIZE 8192
#define TOKEN_BUFFER_SIZE 256

#include <stdio.h>

int rtf_replace(FILE *, FILE *, char *, char *);

#define INDETERMINATE 1
#define TOKENMISMATCH 2

#endif
