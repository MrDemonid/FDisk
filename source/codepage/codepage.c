/*
// Program:  Free FDISK for Windows
// Module:   CODEPAGE.H
// Module Description: Functions to determine language type, map languages to OEM code pages,
//                     retrieve language codes, and convert ANSI strings to Unicode
// Written By: Andrey Hlus
// Version:  1.0.0
// Copyright: 2025 under the terms of the GNU GPL, Version 2
*/


#include <tchar.h>
#include <windows.h>

#include "codepage.h"


UINT GetCodePage(enum Language lang) {
    UINT oemCP = 437;
    switch (lang) {
        case LANG_RU: oemCP = 866; break;
        case LANG_PL: oemCP = 852; break;
        case LANG_TR: oemCP = 857; break;
        case LANG_DE:
        case LANG_FR:
        case LANG_ES:
        case LANG_IT: oemCP = 858; break;
        case LANG_EN: oemCP = 437; break;
    }
    return oemCP;
}


enum Language GetLanguageType(const TCHAR *str)
{
    if (str)
    {
        if (_tcsicmp(str, "en") == 0) return LANG_EN;
        if (_tcsicmp(str, "ru") == 0) return LANG_RU;
        if (_tcsicmp(str, "de") == 0) return LANG_DE;
        if (_tcsicmp(str, "es") == 0) return LANG_ES;
        if (_tcsicmp(str, "fr") == 0) return LANG_FR;
        if (_tcsicmp(str, "pl") == 0) return LANG_PL;
        if (_tcsicmp(str, "tr") == 0) return LANG_TR;
        if (_tcsicmp(str, "it") == 0) return LANG_IT;
    }
    return LANG_DEFAULT;
}


TCHAR *GetLanguageName(enum Language lang) {
    switch (lang) {
        case LANG_RU: return "ru";
        case LANG_PL: return "pl";
        case LANG_TR: return "tr";
        case LANG_DE: return "de";
        case LANG_FR: return "fr";
        case LANG_ES: return "es";
        case LANG_IT: return "it";
        case LANG_EN: return "en";
    }
    return NULL;
}


WCHAR *Ansi2Unicode(const char *str, enum Language lang)
{
    static WCHAR buf[16*1024];
    if (!str)
        return L"";
    UINT cp = GetCodePage(lang);
    MultiByteToWideChar(cp, 0, str, -1, buf, _countof(buf));
    return buf;
}
