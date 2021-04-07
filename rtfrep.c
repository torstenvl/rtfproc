#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "rtfrep.h"
#include "memzero.h"


void resetstate(rtfobj *R) {
    R->rdstt = txt;
    R->mtchstt = indeterminate;
    memzero(R->rawbfr, RAW_BUFFER_SIZE);
    memzero(R->txtbfr, TXT_BUFFER_SIZE);
    memzero(R->cmdbfr, CMD_BUFFER_SIZE);
    R->ri = 0L;
    R->ti = 0L;
    R->ci = 0L;
    R->c = '\0';
}


int rtf_replace(rtfobj *R) {
    resetstate(R);

    while ((R->c = fgetc(R->ipstrm)) != EOF) {
        R->rawbfr[R->ri++] = R->c;
        switch (R->c) {
        case '{':
            if (R->rdstt == cmd) { R->rdstt = cmdproc; }
            // New scope. Copy and push rtf attribute set.
            break;
        case '}':
            if (R->rdstt == cmd) { R->rdstt = cmdproc; }
            // End of scope. Restore previous rtf attribute set.
            break;
        case '\\':
            if ((R->ri >= 2) && (R->rawbfr[R->ri - 2] == '*')) {
                // Allow novel RTF commands to continue
                R->cmdbfr[R->ci++] = R->c;
            } else if (R->rdstt == cmd) {
                R->rawbfr[--R->ri] = '\0';
                ungetc(R->c, R->ipstrm);
                R->rdstt = cmdproc;
            } else {
                R->rdstt = cmd;
                R->cmdbfr[R->ci++] = R->c;
            }
            break;
        case '\t':
        case '\n':
        case '\v':
        case '\f':
        case '\r':
        case ' ':
            if (R->rdstt == cmd) { R->rdstt = cmdproc; }
            else { R->txtbfr[R->ti++] = R->c; }
            break;
        default:
            if (R->rdstt == cmd) { R->cmdbfr[R->ci++] = R->c; }
            else if (R->rdstt == txt) { R->txtbfr[R->ti++] = R->c; }
        }


        if (R->rdstt == cmd) {
            // Don't do anything until we read the whole command in
            continue;
        } else if (R->rdstt == cmdproc) {
            // Let us know what command we encountered, do something if
            // appropriate, and otherwise move on.
            fprintf(stderr, "Encountered command '%s'\n", R->cmdbfr);
            //***************************************************************//
            // TODO                                                          //
            // With the current setup, we are resetting buffers after read-  //
            // ing a command.  That ensures that we are always outputting    //
            // the commands we receive outside of a match, but it ALSO means //
            // we are losing our 'place' with text chunks if there's an RTF  //
            // command in the middle of a word.  This could lead to missed   //
            // replacements -- indeed, it almost definitely will.            //
            // What we need to do instead is to reset ONLY the command       //
            // reading buffer HERE... and also, if we're starting to read    //
            // a potential match, we need to output the raw buffers before   //
            // then as well.  What we really want to have happen is this:    //
            //     - complete command with no match text                     //
            //          - output the raw buffer and reset                    //
            //     - complete command with partial match text                //
            //          - hold off until we know whether to output rep       //
            // The best way to do this might be to distinguish between a     //
            // readstate of txt versus a readstate of unknown.  Use txt as   //
            // a readstate only when we're actually matching text that may   //
            // need to be replaced, and unknown in all other circumstances.  //
            // However, to do that, we will also need to be able to restore  //
            // from a cmd readstate to the previous readstate of either txt  //
            // or unknown.  A full 'stack' of readstates seems overkill.  I  //
            // will have to think about it some more.  In the meantime, here //
            // is what I'm thinking for code in this particular place:       //
            //***************************************************************//
            // Do something with it
            // dosomething(rtf_cmd_buffer);
            // And set us up to keep chugging
            // memzero(rtf_cmd_buffer, CMD_BUFFER_SIZE);
            // ci = 0L;
            // readstate = txt;
            // NOTE NOTE NOTE: Another option would be to output the command
            // buffer when we're done reading it, but I'm not sure I like that
            // because we could mess up non-command rtf tokens like { or }
            fwrite(R->rawbfr, sizeof(byte), R->ri, R->opstrm);
            resetstate(R);
        } else if (R->rdstt == txt) {
            // If we are reading in text, let's see if we have a match or
            // we aren't sure yet.
            //     Sure we have nomatch --> flush buffers
            //     Sure we have a match --> write out the replacement
            //     Not sure we have one --> just keep reading
            R->mtchstt = nomatch;
            for (R->di = 0; R->dict[R->di] != NULL && R->mtchstt != match; R->di += 2) {
                // Check for ANY partial match at length ti
                if (!strncmp(R->dict[R->di], R->txtbfr, R->ti)) {
                    // Check for a full match.  If they match so far and this
                    // iterator position is the full length of the match
                    // token in the dictionary, then it's a full match.
                    if (strlen(R->dict[R->di]) == R->ti) {
                        R->mtchstt = match;
                        break;
                    } else {
                        R->mtchstt = indeterminate;
                    }
                }
            }
            if (R->mtchstt == indeterminate) {
                // If we've hit our testing limits, there's no supported match
                if (R->ci + 1 == CMD_BUFFER_SIZE) R->mtchstt = nomatch;
                if (R->ri + 1 == RAW_BUFFER_SIZE) R->mtchstt = nomatch;
                if (R->ti + 1 == TXT_BUFFER_SIZE) R->mtchstt = nomatch;
                // Otherwise we will just keep chugging along
            }
            if (R->mtchstt == nomatch) {
                fwrite(R->rawbfr, sizeof(byte), R->ri, R->opstrm);
                resetstate(R);
            }
            if (R->mtchstt == match) {
                fwrite(R->dict[R->di+1], sizeof(byte), strlen(R->dict[R->di+1]), R->opstrm);
                resetstate(R);
            }
        }
    }
    fwrite(R->rawbfr, sizeof(byte), R->ri, R->opstrm);

    return 0;
}




rtfobj *new_rtfobj_stream_stream_dict(FILE *in, FILE *out, char **dict) {
    rtfobj *R = malloc(sizeof *R);

    if (R) {
        R->rdstt = txt;
        R->mtchstt = indeterminate;
        memzero(R->rawbfr, RAW_BUFFER_SIZE);
        memzero(R->txtbfr, TXT_BUFFER_SIZE);
        memzero(R->cmdbfr, CMD_BUFFER_SIZE);
        R->ri = 0L;
        R->ti = 0L;
        R->ci = 0L;
        R->c = '\0';
        R->ipstrm = in;
        R->opstrm = out;
        R->dict = dict;
    }

    return R;
}
