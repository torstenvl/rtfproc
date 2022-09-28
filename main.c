#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "rtfsed.h"
#include "STATIC/cpgtou/cpgtou.h"
#include "STATIC/utillib/utillib.h"

#define VALIDSTRING(x) (strlen(x) > 0)

#if   defined(RTFAUTOOPEN)

#if   defined(__APPLE__)
#define RTFINFILE  "TEST/rtfprocess-input.rtf"
#define RTFOUTFILE "TEST/rtfprocess-output.rtf"
#define OPENCMD    "open TEST/rtfprocess-output.rtf"
#elif defined(__unix__)
#define RTFINFILE  "TEST/rtfprocess-input.rtf"
#define RTFOUTFILE "TEST/rtfprocess-output.rtf"
#define OPENCMD    "xdg-open TEST/rtfprocess-output.rtf"
#endif

#else 

#define RTFINFILE  ((argc>=2) ? argv[1] : "")
#define RTFOUTFILE ((argc>=3) ? argv[2] : "")
#define OPENCMD    "true"

#endif

int main(int argc, char **argv) {
    rtfobj *R;
    FILE *fin = stdin;
    FILE *fout = stdout;
    const char *finname  = RTFINFILE;
    const char *foutname = RTFOUTFILE;

    if (VALIDSTRING(finname)) {
        (fin = fopen(finname, "rb")) || DIE("Could not read file \'%s\'\n", finname);
    } 

    if (VALIDSTRING(foutname)) {
        (fout = fopen(foutname, "wb")) || DIE("Could not write to file \'%s\'\n", foutname);
    } 

    setvbuf(fin, NULL, _IOFBF,  (1<<21));
    setvbuf(fout, NULL, _IOFBF, (1<<21));

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

    if (fin != stdin) fclose(fin);
    if (fout != stdout) fclose(fout);

    if (VALIDSTRING(OPENCMD)) {
        int ret = system(OPENCMD);
        ret=ret^ret;
    }

    return 0;
}
