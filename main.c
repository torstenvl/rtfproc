#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include "rtfsed.h"
#include "STATIC/cpgtou.h"

int main(void) {
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

    R = new_rtfobj(stdin, stdout, replacements);
    rtfreplace(R);
    delete_rtfobj(R);

    exit(EXIT_SUCCESS);
}
