#include <stdio.h>
#include <string.h>
#include "rtfsed.h"
#include "utillib.h"

int main(void) {
    const char *finname  = "TEST/test-latepartial-input.rtf";
    const char *foutname = "./test-latepartial-output.rtf";
    FILE *fin;
    FILE *fout;
    rtfobj *R;

    (fin =  fopen(finname,  "rb")) || DIE("Could not read file \'%s\'\n",     finname );
    (fout = fopen(foutname, "wb")) || DIE("Could not write to file \'%s\'\n", foutname);

    const char *replacements[] = {
        "JAMES",               "BOOBEAR",
        "MEXICAN",             "LATIN",
        "ATTORNEY",            "Maj J. L. Ockert",
        "TORTLOCATION",        "Colorado Springs, CO",
        NULL 
    };

    R = new_rtfobj(fin, fout, NULL, replacements);
    rtfreplace(R);
    delete_rtfobj(R);

    fclose(fin);
    fclose(fout);

    return 0;
}
