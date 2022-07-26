#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include "rtfsed.h"
#include "STATIC/cpgtou.h"

#define FILENAME       "TEST/rtfprocess-input.rtf"
#define OUTPUTFILENAME "TEST/rtfprocess-output.rtf"




int main(void) {
    FILE *fin  = NULL;
    FILE *fout = NULL;
    rtfobj *R;


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

    fin  = fopen(FILENAME, "rb");         assert(fin);
    fout = fopen(OUTPUTFILENAME, "wb");   assert(fout);

    R = new_rtfobj(fin, fout, replacements);
    rtfreplace(R);
    delete_rtfobj(R);

    fclose(fin);
    fclose(fout);

    // system("open TEST/rtfprocess-output.rtf");

    exit(EXIT_SUCCESS);
}
