#include <stdio.h>
#include <string.h>
#include "rtfsed.h"
#include "utillib.h"

int main(int argc, char **argv) {
    FILE *fin = stdin;
    FILE *fout = stdout;
    rtfobj *R;

    char *finname = (argc > 1) ? argv[1] : NULL;
    char *foutname = (argc > 2) ? argv[2] : NULL;
    if (finname)  (fin  = fopen(finname,  "rb")) || DIE("Could not read file \'%s\'\n",     finname );
    if (foutname) (fout = fopen(foutname, "wb")) || DIE("Could not write to file \'%s\'\n", foutname);

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

    R = new_rtfobj(fin, fout, NULL, replacements);
    rtfreplace(R);
    delete_rtfobj(R);

    if (fin  != stdin)  fclose(fin);
    if (fout != stdout) fclose(fout);

    return 0;
}
