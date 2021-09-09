#ifndef __RTFSED_H
#define __RTFSED_H

#include <stdio.h>
#include <stdbool.h>
#include "rtftypes.h"


rtfobj *new_rtfobj(FILE *fin, FILE *fout, const char **replacements);

void    rtfreplace(rtfobj *R);


#endif
