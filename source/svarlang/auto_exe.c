/* Modified by Andrey Hlus in 2025:
 * Adapted for Windows platform.
 *
 * Original notice follows:
 *
 * This file is part of the svarlang project and is published under the terms
 * of the MIT license.
 *
 * Copyright (C) 2021-2023 Mateusz Viste
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

#include "../codepage/codepage.h"
#include "svarlang.h"


/**
 * Извлекает имя программы без расширения.
 * @param fullName Полное имя программы, включая путь до неё.
 * @param fName То же самое, но без расширения.
 */
static void split_path(const TCHAR *fullName, TCHAR *fName)
{
    TCHAR _drive[_MAX_DRIVE];
    TCHAR _dir[_MAX_DIR];
    TCHAR _fname[_MAX_FNAME];
    TCHAR _ext[_MAX_EXT];

    _tsplitpath(fullName, _drive, _dir, _fname, _ext);
    _tcscpy(fName, _drive);
    _tcscat(fName, _dir);
    _tcscat(fName, _fname);
}

int svarlang_autoload_exepath(const TCHAR *selfexe, const enum Language lang) {
    TCHAR fname[MAX_PATH];

    if (!selfexe)
        return (-200);

    split_path(selfexe, fname);
    _tcscat(fname, _T(".lng"));

    return  svarlang_load(fname, GetLanguageName(lang));
}
