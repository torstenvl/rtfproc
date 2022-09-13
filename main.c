#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include "rtfsed.h"
#include "STATIC/cpgtou/cpgtou.h"

int main(int argc, char **argv) {
    rtfobj *R;
    FILE *fin = stdin;
    FILE *fout = stdout;

    if (argc >= 2) {
        fin = fopen(argv[1], "rb");
        if (!fin) {
            fprintf(stderr, "Could not read file \'%s\'\n", argv[1]);
            perror("Exiting");
            exit(EXIT_FAILURE);
        }
    }

    if (argc >= 3) {
        fout = fopen(argv[2], "wb");
        if (!fout) {
            fprintf(stderr, "Could not write to file \'%s\'\n", argv[2]);
            perror("Exiting");
            exit(EXIT_FAILURE);
        }
    }

    const char *replacements[] = {
        "«SSIC»",                    "1000",
        "«Office Code»",             "B 0524",
        "«Date»",                    "13 Sep 21",
        "«Property Mgr Name»",       "Shady Management",
        "«Property Mgr Addr»",       "1234 Main Street",
        "«Property Mgr City»",       "Woodbridge",
        "«Property Mgr State»",      "VA",
        "«Property Mgr ZIP»",        "22192",
        "«Client Rank»",             "Colonel",
        "«Client Full Name»",        "Chesty A. Puller",
        "«Client Last Name»",        "Puller",
        NULL 
    };

    R = new_rtfobj(fin, fout, replacements);
    rtfreplace(R);
    delete_rtfobj(R);

    exit(EXIT_SUCCESS);
}
