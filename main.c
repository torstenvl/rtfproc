#include <stdio.h>
#include "rtfsed.h"
#include "cpgtou.h"
#include "utillib.h"

#if defined(RTFAUTOOPEN) && defined(__APPLE__)
#define RTFINFILE  "TEST/rtfprocess-input.rtf"
#define RTFOUTFILE "TEST/rtfprocess-output.rtf"
#define OPENCMD    "open TEST/rtfprocess-output.rtf"
#elif defined(RTFAUTOOPEN) && defined(__unix__)
#define RTFINFILE  "TEST/rtfprocess-input.rtf"
#define RTFOUTFILE "TEST/rtfprocess-output.rtf"
#define OPENCMD    "xdg-open TEST/rtfprocess-output.rtf"
#else 
#define RTFINFILE  "TEST/bigfile-input.rtf"
#define RTFOUTFILE "TEST/bigfile-output.rtf"
#endif

int main(int argc, char **argv) {
    rtfobj *R;
    FILE *fin;
    FILE *fout;
    const char *finname  = ((argc>=2) ? argv[1] : RTFINFILE);
    const char *foutname = ((argc>=3) ? argv[2] : RTFOUTFILE);

    (fin = fopen(finname, "rb"))   || DIE("Could not read file \'%s\'\n", finname);
    (fout = fopen(foutname, "wb")) || DIE("Could not write to file \'%s\'\n", foutname);

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
        "こんにちは！",                "Bonjour.",
        NULL 
    };

    R = new_rtfobj(fin, fout, replacements);
    rtfreplace(R);
    delete_rtfobj(R);

    if (fin != stdin)   fclose(fin);
    if (fout != stdout) fclose(fout);

    #if defined(RTFAUTOOPEN)
    system(OPENCMD);
    #endif

    return 0;
}
