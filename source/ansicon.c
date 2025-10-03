/* Modified by Andrey Hlus in 2025:
 * Adapted for Windows platform.
 *
 * Original notice follows:
 *
 * This file is part of the ANSICON project and is published under the terms
 * of the MIT license.
 *
 * Copyright (C) 2023 Bernd Boeckmann
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

#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>

#include "logger.h"
#include "codepage/codepage.h"
#include "ansicon.h"


#define CON_MAX_ARG 4

/* escape sequence arguments */
static int arg_count;
static int arg[CON_MAX_ARG];

int con_error;

static unsigned con_width;
static unsigned con_height;
static unsigned con_size;
static char con_attributes_enabled = 1;

static int con_curx;
static int con_cury;

static WORD con_textattr;

static char flag_interpret_esc;

static char con_is_device;
static char cursor_sync_disabled;

static HANDLE hConsole = NULL;
static HANDLE hPrevConsole = NULL;


static void con_get_hw_cursor(int* x, int* y);

static void con_set_hw_cursor(int x, int y);

static void con_advance_cursor(void);


static COLORREF vgaPalette[16] = {
    RGB(0, 0, 0), RGB(0, 0, 128), RGB(0, 128, 0), RGB(0, 128, 128), RGB(128, 0, 0), RGB(128, 0, 128), RGB(128, 128, 0), RGB(192, 192, 192),
    RGB(128, 128, 128), RGB(0, 0, 255), RGB(0, 255, 0), RGB(0, 255, 255), RGB(255, 0, 0), RGB(255, 0, 255), RGB(255, 255, 0), RGB(255, 255, 255)
};


void con_set_dos_palette(void)
{

    // Загружаем kernel32.dll и ищем SetConsoleScreenBufferInfoEx
    HMODULE hKernel = GetModuleHandle(_T("kernel32.dll"));
    if (!hKernel)
        return;

    typedef BOOL (WINAPI *pSetConsoleScreenBufferInfoEx)(HANDLE, PCONSOLE_SCREEN_BUFFER_INFOEX);
    typedef BOOL (WINAPI *pGetConsoleScreenBufferInfoEx)(HANDLE, PCONSOLE_SCREEN_BUFFER_INFOEX);

    pSetConsoleScreenBufferInfoEx fSet = (pSetConsoleScreenBufferInfoEx)GetProcAddress(hKernel, "SetConsoleScreenBufferInfoEx");
    pGetConsoleScreenBufferInfoEx fGet = (pGetConsoleScreenBufferInfoEx)GetProcAddress(hKernel, "GetConsoleScreenBufferInfoEx");

    if (!fSet || !fGet)
    {
        // Функция недоступна. Похоже работаем на Windows XP и ниже.
        return;
    }

    // Получаем текущие настройки буфера
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex;
    ZeroMemory(&csbiex, sizeof(csbiex));
    csbiex.cbSize = sizeof(csbiex);

    if (!fGet(hConsole, &csbiex))
        return;

    // Устанавливаем DOS-палитру (цвета 0–15)
    memcpy(csbiex.ColorTable, vgaPalette, sizeof(csbiex.ColorTable));
    fSet(hConsole, &csbiex);

    // Обновляем текущие атрибуты цвета, чтобы соответствовать палитре
    con_textattr &= 0xFF; // оставляем младшие 8 бит без изменений
    SetConsoleTextAttribute(hConsole, con_textattr);
}

/*
 * Запрещает изменять размеры окна.
 */
void con_fix_window_size(HWND hwnd)
{
    LONG style = GetWindowLong(hwnd, GWL_STYLE);

    // Убираем рамку изменения размеров и кнопку "Развернуть"
    style &= ~(WS_SIZEBOX | WS_MAXIMIZEBOX);

    SetWindowLong(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    // Убираем из меню окна пункты с аналогичным действием
    HMENU hMenu = GetSystemMenu(hwnd, FALSE);
    if (hMenu)
    {
        DeleteMenu(hMenu, SC_SIZE, MF_BYCOMMAND);
        DeleteMenu(hMenu, SC_MAXIMIZE, MF_BYCOMMAND);
    }
}

DWORD con_set_size(HANDLE hCon, int width, int height)
{
    // Обрезаем размеры по максимально допустимым, иначе нам не дадут их установить
    COORD largest = GetLargestConsoleWindowSize(hCon);
    width = width < largest.X ? width : largest.X;
    height = height < largest.Y ? height : largest.Y;

    // Устанавливаем размер окна на минималк
    SMALL_RECT zeroWindow = {0, 0, 0, 0};
    if (!SetConsoleWindowInfo(hCon, TRUE, &zeroWindow))
        return GetLastError();

    // Меняем размер буфера консоли
    COORD bufferSize = {(SHORT)width, (SHORT)height};
    if (!SetConsoleScreenBufferSize(hCon, bufferSize))
        return GetLastError();

    // Теперь можно менять и размер окна консоли
    SMALL_RECT windowSize = {0, 0, (SHORT)(width - 1), (SHORT)(height - 1)};
    if (!SetConsoleWindowInfo(hCon, TRUE, &windowSize))
        return GetLastError();

    // Контрольный "выстрел", чтобы на Win 10+ не появились скролы
    if (!SetConsoleScreenBufferSize(hCon, bufferSize))
        return GetLastError();

    return 0;
}

void con_set_lang(const enum Language lang)
{
    UINT oemCP = GetCodePage(lang);

#ifdef UNICODE
    if (IsWindowsVistaOrGreater()) {
        // Vista+
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    } else {
        SetConsoleOutputCP(oemCP);
        SetConsoleCP(oemCP);
    }
#else
    SetConsoleOutputCP(oemCP);
    SetConsoleCP(oemCP);
#endif
}

/*
 * Создание и настройка окна консоли
 */
DWORD con_open(int width, int height)
{
    // Получаем стандартный вывод
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

    if (!hStdout || hStdout == INVALID_HANDLE_VALUE)
    {
        // Если консоли нет (GUI-приложение), создаем её
        if (!AllocConsole())
            return GetLastError();
        hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!hStdout || hStdout == INVALID_HANDLE_VALUE)
            return GetLastError();
    }

    hPrevConsole = hStdout;

    // Создаем свой буфер, на случай если нас запустили из под консоли.
    hConsole = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CONSOLE_TEXTMODE_BUFFER,
        NULL
    );
    if (hConsole == INVALID_HANDLE_VALUE)
    {
        hConsole = NULL;
        return GetLastError();
    }
    if (!SetConsoleActiveScreenBuffer(hConsole))
    {
        return GetLastError();
    }

    // Настраиваем кодировку (пока по дефолту)
    con_set_lang(LANG_EN);

    // Устанавливаем нормальную палитру
    con_set_dos_palette();

    return con_set_size(hConsole, width, height);
}

void con_close(void)
{
    if (hPrevConsole)
    {
        SetConsoleActiveScreenBuffer(hPrevConsole);
        hPrevConsole = NULL;
    }

    if (hConsole)
    {
        CloseHandle(hConsole);
        hConsole = NULL;
    }
}

DWORD con_init(int interpret_esc)
{
    flag_interpret_esc = interpret_esc;
    /* screen size ? */
    con_width = 80;
    con_height = 50;
    con_size = con_width * con_height;

    DWORD res = con_open(con_width, con_height);
    if (res)
        return res;
    con_fix_window_size(GetConsoleWindow());

    con_reset_attr();
    con_get_hw_cursor(&con_curx, &con_cury);
    cursor_sync_disabled = 0;
    return 0;
}

unsigned con_get_width(void)
{
    return con_width;
}

unsigned con_get_height(void)
{
    return con_height;
}

/* function to disable hardware cursor sync  */
void con_disable_cursor_sync(void)
{
    cursor_sync_disabled++;
}

/* function to enable cursor sync */
void con_enable_cursor_sync(void)
{
    if (cursor_sync_disabled)
    {
        cursor_sync_disabled--;
    }

    if (!cursor_sync_disabled)
    {
        con_set_hw_cursor(con_curx, con_cury);
    }
}

void con_set_cursor_xy(int x, int y)
{
    con_curx = min(max(1, x), con_width);
    con_cury = min(max(1, y), con_height);

    if (!cursor_sync_disabled)
    {
        con_set_hw_cursor(con_curx, con_cury);
    }
}

void con_set_cursor_rel(int dx, int dy)
{
    int x, y;

    x = con_curx + dx;
    y = con_cury + dy;

    con_set_cursor_xy(x, y);
}

void con_get_cursor_xy(int* x, int* y)
{
    *x = con_curx;
    *y = con_cury;
}

int con_get_cursor_x(void)
{
    return con_curx;
}

int con_get_cursor_y(void)
{
    return con_cury;
}

static int curx_save = 1, cury_save = 1;

void con_save_cursor_xy(void)
{
    curx_save = con_curx;
    cury_save = con_cury;
}

void con_restore_cursor_xy(void)
{
    con_curx = curx_save;
    con_cury = cury_save;
}

static void con_get_hw_cursor(int* x, int* y)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!hConsole)
    {
        *x = *y = 1;
        return;
    }
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
    {
        *x = *y = 1;
        return;
    }
    *x = csbi.dwCursorPosition.X + 1;
    *y = csbi.dwCursorPosition.Y + 1;
}

static void con_set_hw_cursor(int x, int y)
{
    if (!hConsole)
        return;
    COORD pos = {x - 1, y - 1};
    SetConsoleCursorPosition(hConsole, pos);
}

void con_sync_from_hw_cursor(void)
{
    con_get_hw_cursor(&con_curx, &con_cury);
}

static void con_advance_cursor(void)
{
    int x, y;

    x = con_curx;
    y = con_cury;

    if (x < con_width)
    {
        x += 1;
    }
    else
    {
        x = 1;
        y += 1;
    }

    if (y > con_height)
    {
        con_scroll(1);
        y = con_height;
    }

    con_set_cursor_xy(x, y);
}

/* waits for a key press and returns it.
 * extended keys are returned ORed with 0x100 */
int con_readkey(void)
{
#ifdef UNICODE
    wint_t ch = _getwch(); // wide getch
    if (ch == 0 || ch == 0xE0) // расширенный код
    {
        wint_t ext = _getwch();
        return 0x100 | ext;
    }
    return ch;
#else
    int ch = _getch();
    if (ch == 0 || ch == 0xE0) // расширенный код
    {
        int ext = _getch();
        return 0x100 | ext;
    }
    return ch;
#endif
}

static void con_nl(void)
{
    int x, y;

    x = 1;
    y = con_cury + 1;

    if (y > con_height)
    {
        con_scroll(1);
        y = con_height;
    }
    con_set_cursor_xy(x, y);
}

static void con_cr(void)
{
    con_set_cursor_xy(1, con_cury);
}

static void _con_putc_plain(TCHAR c)
{
    COORD pos;
    DWORD written;

    // unsigned short c = (unsigned short)ch;
    if (c >= _T(' ') || c < 0) // fix: russian characters in windows-1251 start at 0xC0.
    {
        pos.X = con_curx - 1;
        pos.Y = con_cury - 1;
        // пишем символ и атрибут
        WriteConsoleOutputCharacter(hConsole, &c, 1, pos, &written);
        WriteConsoleOutputAttribute(hConsole, &con_textattr, 1, pos, &written);
        con_advance_cursor();
    }
    else
    {
        /* handle control characters */
        switch (c)
        {
        case _T('\n'): /* new line */
            con_nl();
            break;
        case _T('\r'): /* carrige return */
            con_cr();
            break;
        case _T('\t'):
            int n = 8 - (con_curx & 7);
            for (int i = 0; i <= n; i++)
            {
                _con_putc_plain(_T(' '));
            }
            break;
        }
    }
}

void con_scroll(int n)
{
    // область, которую двигаем
    SMALL_RECT sr;
    sr.Left = 0;
    sr.Top = n; // откуда начинаем (пропускаем n строк)
    sr.Right = con_width - 1;
    sr.Bottom = con_height - 1;
    // куда двигаем
    COORD dest;
    dest.X = 0;
    dest.Y = 0;
    // заполняем освободившиеся строки пробелами с текущим цветом
    CHAR_INFO fill;
#ifdef UNICODE
    fill.Char.UnicodeChar = L' ';
#else
    fill.Char.AsciiChar = ' ';
#endif
    fill.Attributes = con_textattr;
    // прокрутка вверх на n строк
    ScrollConsoleScreenBuffer(hConsole, &sr, NULL, dest, &fill);
}

void con_clrscr(void)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD written;

    GetConsoleScreenBufferInfo(hConsole, &csbi);
    DWORD cells = csbi.dwSize.X * csbi.dwSize.Y;
    COORD home = {0, 0};

    FillConsoleOutputCharacter(hConsole, _T(' '), cells, home, &written);
    FillConsoleOutputAttribute(hConsole, con_textattr, cells, home, &written);
    SetConsoleCursorPosition(hConsole, home);
    con_set_cursor_xy(1, 1);
}

void con_clreol(void)
{
    DWORD written;
    TCHAR ch = _T(' ');

    COORD pos = {con_curx - 1, con_cury - 1};
    for (int x = con_curx; x <= con_width; x++)
    {
        WriteConsoleOutputCharacter(hConsole, &ch, 1, pos, &written);
        WriteConsoleOutputAttribute(hConsole, &con_textattr, 1, pos, &written);
        pos.X++;
    }
}

void con_enable_attr(int flag)
{
    con_attributes_enabled = (char)flag;
}

void con_reset_attr(void)
{
    con_textattr = 7;
    // SetConsoleTextAttribute(hConsole, con_textattr);
}

void con_set_bold(int flag)
{
    if (!con_attributes_enabled)
    {
        return;
    }
    if (flag)
    {
        con_textattr |= FOREGROUND_INTENSITY; // включаем яркость текста
    }
    else
    {
        con_textattr &= ~FOREGROUND_INTENSITY; // выключаем яркость
    }
    // SetConsoleTextAttribute(hConsole, con_textattr);
}

int con_get_bold(void)
{
    return (con_textattr & FOREGROUND_INTENSITY) != 0;
}

void con_set_textcolor(WORD color)
{
    color &= 7;
    if (con_attributes_enabled)
    {
        con_textattr = (con_textattr & 0xfff8) | color;
        // SetConsoleTextAttribute(hConsole, con_textattr);
    }
}

void con_set_backcolor(WORD color)
{
    color &= 7;
    if (con_attributes_enabled)
    {
        con_textattr = (con_textattr & 0xff8f) | (color << 4);
        // SetConsoleTextAttribute(hConsole, con_textattr);
    }
}

void con_set_blinking(int flag)
{
    if (con_attributes_enabled)
    {
        con_textattr = (con_textattr & 0x7f) | ((flag & 1) << 7);
        // SetConsoleTextAttribute(hConsole, con_textattr);
    }
}

static int handle_ansi_function(TCHAR function)
{
    int i, argi;

    switch (function)
    {
    case 'H': /* position cursor */
    case 'f':
        if (con_is_device)
        {
            con_set_cursor_xy(
                arg[0] < 1 ? 1 : arg[0],
                arg[1] < 1 ? 1 : arg[1]
            );
        }
        return 1;

    case 'A': /* cursor up */
        if (con_is_device)
        {
            con_set_cursor_rel(0, arg[0]);
        }
        return 1;

    case 'B': /* cursor down */
        if (con_is_device)
        {
            con_set_cursor_rel(0, arg[0] == -1 ? 1 : arg[0]);
        }
        return 1;

    case 'C': /* cursor right */
        if (con_is_device)
        {
            con_set_cursor_rel(arg[0] == -1 ? 1 : arg[0], 0);
        }
        return 1;

    case 'D': /* cursor left */
        if (con_is_device)
        {
            con_set_cursor_rel(arg[0], 0);
        }
        return 1;

    case 'J': /* erase display */
        if (arg_count != 1 || arg[0] != 2)
        {
            return 0;
        }
        if (con_is_device)
        {
            con_clrscr();
        }
        return 1;

    case 'K': /* erase line */
        if (con_is_device)
        {
            con_clreol();
        }
        return 1;

    case 'm': /* set text attributes */
        for (i = 0; i < arg_count; i++)
        {
            argi = arg[i];
            if (argi == 0)
            {
                con_reset_attr();
            }
            else if (argi == 1)
            {
                con_set_bold(1);
            }
            else if (argi == 5)
            {
                con_set_blinking(1);
            }
            else if (argi == 22)
            {
                con_set_bold(0);
            }
            else if (argi == 25)
            {
                con_set_blinking(0);
            }
            else if (argi >= 30 && argi <= 37)
            {
                con_set_textcolor(argi - 30);
            }
            else if (argi >= 40 && argi <= 47)
            {
                con_set_backcolor(argi - 40);
            }
        }
        return 1;
    }

    return 0;
}

static void _con_putc_ansi(TCHAR c)
{
    static enum { S_NONE, S_START, S_START2, S_ARG, S_FINI, S_ERR } state;

    int i;

    switch (state)
    {
    case S_NONE:
        if (c == _T('\33'))
        {
            /* start of escape sequence, reset internal state */
            arg_count = 0;
            for (i = 0; i < CON_MAX_ARG; i++)
            {
                arg[i] = -1;
            }
            state++;
        }
        else
        {
            _con_putc_plain(c);
            return;
        }
        break;
    case S_START:
        /* skip [ following escape character */
        if (c == _T('['))
        {
            state++;
        }
        else
        {
            state = S_ERR;
        }
        break;

    case S_START2:
        /* if first character after [ is no letter (command), it must be a
             (possibly empty) numeric argument */
        if (!isalpha(c))
        {
            arg_count++;
        }
        state++;
    /* fallthrough !! */
    case S_ARG: /* process numeric arguments and command */
        if (isdigit(c))
        {
            /* numeric argument */
            if (arg[arg_count - 1] == -1)
            {
                arg[arg_count - 1] = 0;
            }
            arg[arg_count - 1] = 10 * arg[arg_count - 1] + (c - '0');
        }
        else if (c == _T(';'))
        {
            /* semicolon starts a new (possibly empty) numeric argument */
            if (arg_count < CON_MAX_ARG - 1)
            {
                arg_count++;
            }
            else
            {
                state = S_ERR;
            }
        }
        else if (isalpha(c))
        {
            if (handle_ansi_function(c))
            {
                state = S_FINI;
            }
            else
            {
                state = S_ERR;
            }
        }
        else
        {
            state = S_ERR;
        }
        break;

    default:
        break;
    }

    switch (state)
    {
    case S_ERR:
        con_error = 1;
        state = S_NONE;
        break;

    case S_FINI:
        state = S_NONE;

    default:
        break;
    }
}

void con_print(const TCHAR* s)
{
    con_disable_cursor_sync();
    if (flag_interpret_esc)
    {
        while (*s)
        {
            _con_putc_ansi(*s++);
        }
    }
    else
    {
        while (*s)
        {
            _con_putc_plain(*s++);
        }
    }
    con_enable_cursor_sync();
}

void con_puts(const TCHAR* s)
{
    con_disable_cursor_sync();
    con_print(s);
    con_putc('\n');
    con_enable_cursor_sync();
}

void con_print_at(int x, int y, const TCHAR* s)
{
    con_disable_cursor_sync();
    con_set_cursor_xy(x, y);
    con_print(s);
    con_enable_cursor_sync();
}

void con_putc(TCHAR c)
{
    if (flag_interpret_esc)
    {
        _con_putc_ansi(c);
    }
    else
    {
        _con_putc_plain(c);
    }
}
