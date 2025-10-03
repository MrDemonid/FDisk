/*
// Program:  Free FDISK for Windows
// Module:   CODEPAGE.H
// Module Description: Header for CODEPAGE.C
// Written By: Andrey Hlus
// Version:  1.0.0
// Copyright: 2025 under the terms of the GNU GPL, Version 2
*/


#ifndef FDISK_CODEPAGE_H
#define FDISK_CODEPAGE_H

#include <tchar.h>
#include <windows.h>


enum Language
{
    LANG_DEFAULT = 0,
    LANG_EN,
    LANG_RU,
    LANG_DE,
    LANG_ES,
    LANG_FR,
    LANG_PL,
    LANG_TR,
    LANG_IT
};

/**
 * Возвращает кодовую страницу для выбранного языка (OEM).
 */
UINT GetCodePage(enum Language lang);


/**
 * Возвращает кодовую страницу для заданного языка (OEM).
 * @param str Строковое представление строки ("EN", "RU", "IT", ....)
 */
enum Language GetLanguageType(const TCHAR *str);


/**
 * Возвращает название кодовой страницы.
 * @return Короткое имя, или NULL.
 */
TCHAR *GetLanguageName(enum Language lang);


/**
 * Конвертация ansi-строки в unicode-строку.
 * @param str Исходная ansi-строка.
 * @param lang Кодировка строки.
 */
WCHAR *Ansi2Unicode(const char *str, enum Language lang);

#endif //FDISK_CODEPAGE_H