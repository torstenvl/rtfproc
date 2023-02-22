/*═════════════════════════════════════════════════════════════════════════*\
║                                                                           ║
║  RTFPROC - RTF Processing Library                                         ║
║  Copyright (c) 2019-2023, Joshua Lee Ockert                               ║
║                                                                           ║
║  THIS WORK IS PROVIDED 'AS IS' WITH NO WARRANTY OF ANY KIND. THE IMPLIED  ║
║  WARRANTIES OF MERCHANTABILITY, FITNESS, NON-INFRINGEMENT, AND TITLE ARE  ║
║  EXPRESSLY DISCLAIMED. NO AUTHOR SHALL BE LIABLE UNDER ANY THEORY OF LAW  ║
║  FOR ANY DAMAGES OF ANY KIND RESULTING FROM THE USE OF THIS WORK.         ║
║                                                                           ║
║  Permission to use, copy, modify, and/or distribute this work for any     ║
║  purpose is hereby granted, provided this notice appears in all copies.   ║
║                                                                           ║
\*═════════════════════════════════════════════════════════════════════════*/

#include <stdio.h>
#include <string.h>
#include "rtfproc.h"
#include "utillib.h"

int main(void) {
    const char *finname  = "TEST/latepartial-input.rtf";
    const char *foutname = "temp.rtf";
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

    R = new_rtfobj(fin, fout, NULL);
    add_rtfobj_replacements(R, replacements);
    rtfreplace(R);
    delete_rtfobj(R);

    fclose(fin);
    fclose(fout);

    return 0;
}
