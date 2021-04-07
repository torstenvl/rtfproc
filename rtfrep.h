#ifndef RTFPROC_H
#define RTFPROC_H

#include <stdio.h>
#include <stdint.h>

// Longest RTF command is 25 bytes plus argument. 64 bytes should be plenty.
// The longest text we will support replacing is 512 code points long for now.
// Allow any of those code points to be in the form \uNNNNNN\'xx\'xx (16bytes)
// or have some amount of character formatting in it.
#define RAW_BUFFER_SIZE 8192
#define TXT_BUFFER_SIZE 1024
#define CMD_BUFFER_SIZE 256

typedef enum rtf_read_state_t {txt, cmd, cmdproc, ignore, flush} rtf_read_state_t;
typedef enum rtf_match_state_t {match, nomatch, indeterminate} rtf_match_state_t;
typedef char byte;

typedef struct rtfobj {
    FILE *ipstrm;
    FILE *opstrm;

    int c;

    rtf_read_state_t rdstt;
    rtf_match_state_t mtchstt;
    byte rawbfr[RAW_BUFFER_SIZE];
    byte txtbfr[TXT_BUFFER_SIZE];
    byte cmdbfr[CMD_BUFFER_SIZE];
    size_t ri;
    size_t ti;
    size_t ci;

    char **dict;
    size_t di;
} rtfobj;




rtfobj *new_rtfobj_stream_stream_dict(FILE *, FILE *, char **);
int rtf_replace(rtfobj *);

#endif
