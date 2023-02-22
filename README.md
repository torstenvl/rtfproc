# RTFPROC

## Description

A library for RTF processing, including replacement of text in RTF files.

## Usage

Create RTF processing objects with `new_rtfobj(FILE *fin, FILE *fout, FILE *ftxt)`, passing the RTF input file, RTF output file, and text output file as arguments.  The last two arguments can be NULL, in which case the library will not output RTF or plain text, respectively.

In all other functions in this library, your RTF object pointer is the first argument.

You can replacing text in an RTF file and output the new RTF.  After creating the RTF object, simply call `add_one_rtfobj_replacement()` to add a replacement key and the value to replace matches with.  Alternatively, you can call `add_rtfobj_replacements()`, where the second argument is an array of alternating keys and values, terminated by `NULL`.  After setting up your replacements, call `rtfreplace()`. 

If you want to do some other kind of processing, you can use `rtfprocess()`.  The second argument is the name of the function you want the RTF processing engine to call at the beginning, at each step of processing, and at the end.  The third argument is a void pointer to data you want available to your callback function.

Your callback function must take three arguments: the RTF object, a void pointer (the same one you provided the processing engine), and an integer, which will be equal to `RTF_PROC_START`, `RTF_PROC_STEP`, or `RTF_PROC_END` as appropriate.  It can manipulate the RTF object, which is defined in `rtfproc.h`.  Most people will be interested in the `raw`, `cmd`, and `txt` buffers, with current sizes/indexes in `ri`, `ci`, and `ti`, respectively.  You can also use the `reset_raw_buffer_by()`, `reset_cmd_buffer_by()`, and `reset_txt_buffer_by()` functions. 

Delete RTF processing objects with `delete_rtfobj()`.  This will free memory used by the RTF object and the objects it contains and uses. 

## Example

The below program simply converts an RTF file to plain text. This could be done more simply by passing `stdout` as the third parameter to `new_rtfobj()`, but this demonstrates the `rtfprocess()` callback functionality. 

```C
#include <stdio.h>
#include <stdlib.h>
#include "rtfproc.h"

void outputrtftxt(rtfobj *R, void *p, int procstage) {
    fwrite(R->txt, R->ti, 1, stdout);
    reset_txt_buffer_by(R, R->ti);
    reset_raw_buffer_by(R, R->ri);
    if (procstage == RTF_PROC_END) fwrite("\n", 1, 1, stdout);
}

int main(int argc, char **argv) {
    FILE *fin;
    rtfobj *R;

    if (argc != 2) {
        fprintf(stderr, "Usage: rtf2txt <RTFFILE>\n");
        fprintf(stderr, "Converts <RTFFILE> to UTF-8 text on standard output.\n\n");
        exit(1);
    }
    
    fin = fopen(argv[1], "rb");
    if (!fin) { perror("Can't open file"); exit(1); }
 
    R = new_rtfobj(fin, NULL, NULL);
    if (!R)   { perror("Can't allocate RTF object"); exit(1); }

    rtfprocess(R, outputrtftxt, NULL);    
    delete_rtfobj(R);
    fclose(fin);

    return 0;
}
```

Due to nested dependencies, it is recommended to compile the above example as follows:

```sh
cc rtf2txt.c `find . -regex ".*/src$" -exec echo -I{} \;` `find . -regex ".*/src/.*\.c"` -o rtf2txt
```

## License

RTFPROC – RTF Processing Library
Copyright &copy; 2019-2023, Joshua Lee Ockert

THIS WORK IS PROVIDED "AS IS" WITH NO WARRANTY OF ANY KIND. THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS, NON-INFRINGEMENT, AND TITLE ARE
EXPRESSLY DISCLAIMED. NO AUTHOR SHALL BE LIABLE UNDER ANY THEORY OF LAW
FOR ANY DAMAGES OF ANY KIND RESULTING FROM THE USE OF THIS WORK.

Permission to use, copy, modify, and/or distribute this work for any
purpose is hereby granted, provided this notice appears in all copies.

