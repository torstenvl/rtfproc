#include <stdio.h>
#include <stdlib.h>
#include "rtfrep.h"

#define FILENAME          "test.rtf"

int main(void) {
    repobj *r = new_repobj_from_file(FILENAME);
    rtf_process(r);
    destroy_repobj(r);

    return 0;
}
