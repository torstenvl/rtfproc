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
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>


static int32_t cdpt_from_utf16(uint16_t hi, uint16_t lo) {
    int32_t cdpt;
    bool hisurrogate = (0xD800 <= hi && hi <= 0xDBFF);
    bool losurrogate = (0xDC00 <= lo && lo <= 0xDFFF);

    if (hisurrogate && losurrogate) {
        // We have a valid surrogate pair, so convert it.
        // Unicode 15.0, Tbl 3-5 says UTF-16 surrogate pairs in form:
        //         110110wwwwyyyyyy
        //                   110111xxxxxxxxxx
        // map to scalar code points of value:
        //              uuuuuyyyyyyxxxxxxxxxx     (uuuuu=wwww+1)
        cdpt = ((lo & 0b000000000001111111111)) |
               ((hi & 0b00000111111)    << 10 ) |
               ((hi & 0b01111000000)    << 10 ) +
               ((     0b00001000000)    << 10 ) ;
    } else if (!hisurrogate && !losurrogate) {
        // Neither of this pair is a surrogate; lo should be a valid code
        // point in the Basic Multilingual Plane, so return that. 
        cdpt = lo;
    } else {
        // One of them is a surrogate but not both; we have an encoding
        // error. Return a question mark as a placeholder. 
        cdpt = '?';
    }
    return cdpt;
}


static void utf8_from_cdpt(int32_t c, char utf8[5]) {
    char *u = utf8;
    if (c < 0) {
        u[0]= '\0';
    }
    else if (c < 0b000000000000010000000) { // Up to 7 bits
        u[0]= (char)(c>>0  & 0b01111111 | 0b00000000);  // 7 bits –> 0xxxxxxx
        u[1]= '\0';
    } 
    else if (c < 0b000000000100000000000) { // Up to 11 bits
        u[0]= (char)(c>>6  & 0b00011111 | 0b11000000);  // 5 bits –> 110xxxxx
        u[1]= (char)(c>>0  & 0b00111111 | 0b10000000);  // 6 bits –> 10xxxxxx
        u[2]= '\0';
    } 
    else if (c < 0b000010000000000000000) { // Up to 16 bits
        u[0]= (char)(c>>12 & 0b00001111 | 0b11100000);  // 4 bits –> 1110xxxx
        u[1]= (char)(c>>6  & 0b00111111 | 0b10000000);  // 6 bits –> 10xxxxxx
        u[2]= (char)(c>>0  & 0b00111111 | 0b10000000);  // 6 bits –> 10xxxxxx
        u[3]= '\0';
    } 
    else if (c < 0b100010000000000000000) { // Up to 21 bits
        u[0]= (char)(c>>18 & 0b00000111 | 0b11110000);  // 3 bits –> 11110xxx
        u[1]= (char)(c>>12 & 0b00111111 | 0b10000000);  // 6 bits –> 10xxxxxx
        u[2]= (char)(c>>6  & 0b00111111 | 0b10000000);  // 6 bits –> 10xxxxxx
        u[3]= (char)(c>>0  & 0b00111111 | 0b10000000);  // 6 bits –> 10xxxxxx
        u[4]= '\0';
    } 
    else {
        u[0]= '\0';
    }
}


int main(void) {
    uint16_t input;
    uint16_t temp_hi_srgt;

    int32_t cdpt;
    char utf8buffer[5];

    while (1) {
        input = 0;

        printf("Enter a UTF-16 hex sequence:  0x");
        scanf(" %hx", &input);
        scanf("%*[^\n]");
        scanf("%*c");
        
        if (feof(stdin)) break;
        if (input == 0) break;

        if (0xD800 <= input && input <= 0xDBFF) {
            // High surrogate
            temp_hi_srgt = input;
            continue;
        } else if (0xDC00 <= input && input <= 0xDFFF) {
            // Low surrogate
            if (0xD800 <= temp_hi_srgt && temp_hi_srgt <= 0xDBFF) {
                cdpt = cdpt_from_srgtpair(temp_hi_srgt, input);
                if (cdpt < 0) cdpt = '?';
            } else {
                cdpt = '?';
            }
            temp_hi_srgt = 0;
        } else {
            // Basic Multilingual Plane
            cdpt = input;
            temp_hi_srgt = 0;
        }

        utf8_from_cdpt(cdpt, utf8buffer);

        printf("Equal to Unicode code point U+%04" PRIX32" (%s).\n", cdpt, utf8buffer);
        printf("Check against https://www.compart.com/en/unicode/U+%04" PRIX32"\n", cdpt);

    }

    return 0;
}