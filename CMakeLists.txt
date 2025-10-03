cmake_minimum_required(VERSION 4.0)
project(FDisk C)

# -------------------------
# C стандарт
# -------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# -------------------------
# Опция: Unicode/ANSI (ON - Unicode, OFF - ANSI)
# -------------------------
# "FORCE" гарантирует, что значение не будет перезаписано старым кэшем
option(BUILD_UNICODE "Compile with Unicode (_UNICODE/UNICODE)" OFF)
set(BUILD_UNICODE ${BUILD_UNICODE} CACHE BOOL "Compile with Unicode (_UNICODE/UNICODE)" FORCE)


# -------------------------
# Исходники
# -------------------------
set(SOURCES
        main.c
        source/ansicon.c
        source/printf.c
        source/pdiskio.c
        source/svarlang/auto_exe.c
        source/svarlang/auto_nls.c
        source/svarlang/svarlang.c
        source/svarlang/version.c
        source/deflang.c
        source/compat.c
        source/fdiskio.c
        source/pcompute.c
        source/display.c
        source/cmd.c
        source/helpscr.c
        source/kbdinput.c
        source/ui.c
        source/environ.c
        source/environ.h
        source/logger.c
        source/codepage/codepage.c
)

# -------------------------
# Создаем exe
# -------------------------
add_executable(FDisk ${SOURCES})


# -------------------------
# Макросы компилятору
# -------------------------

# Задаем флаг -DRELEASE, если это релизная сборка
target_compile_definitions(FDisk PRIVATE
        $<$<CONFIG:Release>:RELEASE>
)

# Опции для юникода и анси.
if(BUILD_UNICODE)
    target_compile_definitions(FDisk PRIVATE UNICODE _UNICODE)
    target_compile_options(FDisk PRIVATE /source-charset:utf-8)
else ()
    target_compile_options(FDisk PRIVATE /source-charset:utf-8 /execution-charset:windows-1251)
endif()

if (MSVC)
    set_target_properties(FDisk PROPERTIES LINK_FLAGS "/MANIFEST:NO")
    target_sources(FDisk PRIVATE FDisk.rc)

    if (CMAKE_SIZEOF_VOID_P EQUAL 4) # 32-битная сборка
        message(STATUS "Configuring for 32-bit (Windows XP compatibility)")
        target_link_options(FDisk PRIVATE
                "/SUBSYSTEM:CONSOLE,5.01"
                "/DYNAMICBASE:NO"
                "/NXCOMPAT:NO"
        )
        add_definitions(-D_WIN32_WINNT=0x0501 -DNTDDI_VERSION=0x05010300)

    else() # 64-битная сборка
        message(STATUS "Configuring for 64-bit (modern Windows)")
        target_link_options(FDisk PRIVATE "/SUBSYSTEM:CONSOLE,6.00")
        add_definitions(-D_WIN32_WINNT=0x0600 -DNTDDI_VERSION=0x06000000)
    endif()
endif()
