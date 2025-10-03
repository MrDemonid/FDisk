/* Modified by Andrey Hlus in 2025:
 * Adapted for Windows platform.
 *
 * Original notice follows:
 *
 * This file is part of the svarlang project and is published under the terms
 * of the MIT license.
 *
 * Copyright (C) 2021-2024 Mateusz Viste
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* if WITHSTDIO is enabled, then remap file operations to use the standard
 * stdio amenities */

#include "svarlang.h"


#include <stdio.h>   /* FILE, fopen(), fseek(), etc */
#include <tchar.h>

#include "../printf.h"
typedef FILE *FHANDLE;
#define FOPEN(x) fopen(x, "rb")
#define FCLOSE(x) fclose(x)
#define FSEEK(f,b) fseek(f,b,SEEK_CUR)
#define FREAD(f,t,b) fread(t, 1, b, f)


/* supplied through deflang.c */
extern char svarlang_mem[];
extern unsigned short svarlang_dict[];
extern const unsigned short svarlang_memsz;
extern const unsigned short svarlang_string_count;


const char *svarlang_strid(unsigned short id) {
    unsigned short left = 0, right = svarlang_string_count - 1, x;
    unsigned short v;

    if (svarlang_string_count == 0) return ("");

    while (left <= right) {
        x = left + ((right - left) >> 2);
        v = svarlang_dict[x * 2];

        if (id == v)
            return (svarlang_mem + svarlang_dict[x * 2 + 1]);

        if (id > v) {
            if (x == 65535) goto not_found;
            left = x + 1;
        } else {
            if (x == 0) goto not_found;
            right = x - 1;
        }
    }

not_found:
    return ("");
}


void mvucomp(char *dst, const unsigned short *src, unsigned short complen) {
    unsigned char rawwords = 0; /* number of uncompressible words */
    complen /= 2; /* I'm interested in number of words, not bytes */

    while (complen != 0) {
        unsigned short token;
        /* get next mvcomp token */
        token = *src;
        src++;
        complen--;

        /* token format is LLLL OOOO OOOO OOOO, where:
         * OOOO OOOO OOOO is the back reference offset (number of bytes-1 to rewind)
         * LLLL is the number of bytes (-1) that have to be copied from the offset.
         *
         * However, if LLLL is zero then the token's format is different:
         * 0000 RRRR BBBB BBBB
         *
         * The above form occurs when uncompressible data is encountered:
         * BBBB BBBB is the literal value of a byte to be copied
         * RRRR is the number of RAW (uncompressible) WORDS that follow (possibly 0)
         */

        /* raw word? */
        if (rawwords != 0) {
            unsigned short *dst16 = (void *) dst;
            *dst16 = token;
            dst += 2;
            rawwords--;

            /* literal byte? */
        } else if ((token & 0xF000) == 0) {
            *dst = token;
            /* no need for an explicit "& 0xff", dst is a char ptr so token is naturally truncated to lowest 8 bits */
            dst++;
            rawwords = token >> 8; /* number of RAW words that are about to follow */

            /* else it's a backreference */
        } else {
            char *src = dst - (token & 0x0FFF) - 1;
            token >>= 12;
            for (;;) {
                *dst = *src;
                dst++;
                src++;
                if (token == 0) break;
                token--;
            }
        }
    }
}

void tchar_to_ansi(const TCHAR* src, char* dst, int dstSize) {
#ifdef UNICODE
    WideCharToMultiByte(CP_ACP, 0, src, -1, dst, dstSize, NULL, NULL);
#else
    strncpy(dst, src, dstSize);
    dst[dstSize-1] = '\0';
#endif
}


int svarlang_load(const TCHAR *in_fname, const TCHAR *in_lang) {
    char fname[MAX_PATH];
    char lang[MAX_PATH];
    unsigned short langid;
    unsigned short buff16[2];
    FHANDLE fd;
    signed char exitcode = 0;
    struct {
        unsigned long sig;
        unsigned short string_count;
    } hdr;

    if (in_lang == NULL || in_fname == NULL || _tcslen(in_lang) == 0 || _tcslen(in_fname) == 0)
        return -1;

    // Поскольку LNG расчитан на ansi, то дальше просто работаем в этой кодировке
    tchar_to_ansi(in_fname, fname, sizeof(fname));
    tchar_to_ansi(in_lang, lang, sizeof(lang));

    langid = *((unsigned short *) lang);
    langid &= 0xDFDF; /* make sure lang is upcase */

    fd = FOPEN(fname);
    if (!fd) return (-1);

    /* read hdr, sig should be "SvL\x1a" (0x1a4c7653) */
    if ((FREAD(fd, &hdr, 6) != 6) || (hdr.sig != 0x1a4c7653L) || (hdr.string_count != svarlang_string_count)) {
        exitcode = -2;
        goto FCLOSE_AND_EXIT;
    }

    for (;;) {
        /* read next lang id and string table size in file */
        if (FREAD(fd, buff16, 4) != 4) {
            exitcode = -3;
            goto FCLOSE_AND_EXIT;
        }

        /* is it the lang I am looking for? */
        if ((buff16[0] & 0x7FFF) == langid) break; /* compare without highest bit - it is a flag for compression */

        /* skip to next lang (in two steps to avoid a potential uint16 overflow) */
        FSEEK(fd, svarlang_string_count * 4);
        FSEEK(fd, buff16[1]);
    }

    /* load the index (dict) */
    if (FREAD(fd, svarlang_dict, svarlang_string_count * 4) != svarlang_string_count * 4) {
        exitcode = -4;
        goto FCLOSE_AND_EXIT;
    }

    /* is the lang block compressed? then uncompress it */
    if (buff16[0] & 0x8000) {
        unsigned short *mvcompptr;

        /* start by loading the entire block at the end of the svarlang mem */
        mvcompptr = (void *) (svarlang_mem + svarlang_memsz - buff16[1]);
        if (FREAD(fd, mvcompptr, buff16[1]) != buff16[1]) {
            exitcode = -5;
            goto FCLOSE_AND_EXIT;
        }

        /* uncompress now */
        mvucomp(svarlang_mem, mvcompptr, buff16[1]);

        goto FCLOSE_AND_EXIT;
    }

    /* lang block not compressed - load as is */
    if (FREAD(fd, svarlang_mem, buff16[1]) != buff16[1]) {
        exitcode = -7;
    }

FCLOSE_AND_EXIT:

    FCLOSE(fd);
    return (exitcode);
}
