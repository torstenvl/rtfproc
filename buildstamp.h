#ifndef __BUILDSTAMP_H
#define __BUILDSTAMP_H

char *buildstamp(void) {
    static char date[] = __DATE__;
    static char time[] = __TIME__;
    static char mo[3] = { 0 };
    static char bs[24] = { 0 };
    size_t bi = 0;
    size_t di = 0;
    size_t ti = 0;

    if (bs[0] == 0) {
        if (date[0]=='J' && date[3]=='u') { mo[0] = '0'; mo[1] = '1'; }
        if (date[0]=='F')                 { mo[0] = '0'; mo[1] = '2'; }
        if (date[0]=='M' && date[2]=='r') { mo[0] = '0'; mo[1] = '3'; }
        if (date[0]=='A' && date[1]=='p') { mo[0] = '0'; mo[1] = '4'; }
        if (date[0]=='M' && date[2]=='y') { mo[0] = '0'; mo[1] = '5'; }
        if (date[0]=='J' && date[3]=='e') { mo[0] = '0'; mo[1] = '6'; }
        if (date[0]=='J' && date[3]=='y') { mo[0] = '0'; mo[1] = '7'; }
        if (date[0]=='A' && date[1]=='u') { mo[0] = '0'; mo[1] = '8'; }
        if (date[0]=='S')                 { mo[0] = '0'; mo[1] = '9'; }
        if (date[0]=='O')                 { mo[0] = '1'; mo[1] = '0'; }
        if (date[0]=='N')                 { mo[0] = '1'; mo[1] = '1'; }
        if (date[0]=='D')                 { mo[0] = '1'; mo[1] = '2'; }

        for (di = 7; date[di] != 0; di++) bs[bi++] = date[di];
        bs[bi++] = mo[0];
        bs[bi++] = mo[1];
        bs[bi++] = date[4];
        bs[bi++] = date[5];
        // bs[bi++] = '.';
        bs[bi++] = time[0];
        bs[bi++] = time[1];
        bs[bi++] = time[3];
        bs[bi++] = time[4];
        bs[bi++] = time[6];
        bs[bi++] = time[7];
        bs[bi++] = '\0';
    }
    
    return bs;
}

#endif
