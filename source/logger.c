//
// Created by Andrey on 17.09.2025.
//
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <tchar.h>

static FILE* gLogFile = NULL;
char filename[260] = {0};

void log_init(const char fn[])
{
    strcpy(filename, fn);
}

void log_printf(const char* fmt, ...)
{
    if (strlen(filename) > 0)
    {
        gLogFile = fopen(filename, "a+");

        if (!gLogFile) return;
        va_list args;
        va_start(args, fmt);
        vfprintf(gLogFile, fmt, args);
        fflush(gLogFile);
        va_end(args);

        fclose(gLogFile);
    }
}

void show_last_error_box(const DWORD errorCode, const TCHAR* context)
{
    TCHAR* msgBuf = NULL;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &msgBuf, 0, NULL
        );

    TCHAR text[4096];
    if (context) {
        _stprintf(text, _T("%s\nКод: %lu\n%s"), context, errorCode, msgBuf ? msgBuf : _T("(нет описания)"));
    } else {
        _stprintf(text, _T("Код: %lu\n%s"), errorCode, msgBuf ? msgBuf : _T("(нет описания)"));
    }

    MessageBox(NULL, text, _T("Ошибка приложения"), MB_OK | MB_ICONERROR);

    if (msgBuf)
        LocalFree(msgBuf);
}
