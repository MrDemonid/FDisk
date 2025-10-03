//
// Created by Andrey on 30.09.2025.
//

#ifndef FDISK_LOGGER_H
#define FDISK_LOGGER_H

void log_init(const char* fn);
void log_printf(const char* fmt, ...);

/**
 *
 * @param errorCode Вывод ошибки (от GetLastError()) в MessageBox(), на случай проблем с консолью.
 * @param context Дополнительное сообщение.
 */
void show_last_error_box(DWORD errorCode, const TCHAR* context);

#endif //FDISK_LOGGER_H