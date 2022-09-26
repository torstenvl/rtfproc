#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "rtfsed.h"
#include "STATIC/cpgtou/cpgtou.h"
#include "STATIC/utillib/utillib.h"

#define VALIDSTRING(x) (strlen(x) > 0)

#if defined( RTFAUTOOPEN )
  #define RTFINFILE   "TEST/rtfprocess-input.rtf"
  #define RTFOUTFILE  "TEST/rtfprocess-output.rtf"
  #if defined( OS_MAC )
    #define OPENCMD     "open TEST/rtfprocess-output.rtf"
  #elif defined( OS_LINUX ) ||  defined( OS_BSD )
    #define OPENCMD     "xdg-open TEST/rtfprocess-output.rtf"
  #elif defined( OS_WIN )
    #define OPENCMD     "start TEST/rtfprocess-output.rtf"
  #endif
#else
#define RTFINFILE   ""
#define RTFOUTFILE  ""
#define OPENCMD     ""
#define system(...) ((void)0)
#endif

int main(int argc, char **argv) {
    rtfobj *R;
    FILE *fin = stdin;
    FILE *fout = stdout;

    const char *finname  = (argc>=2) ? argv[1] : RTFINFILE;
    const char *foutname = (argc>=3) ? argv[2] : RTFOUTFILE;

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
        system(OPENCMD);
    }

    return 0;
}
