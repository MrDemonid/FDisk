cmake_minimum_required(VERSION 4.0)
project(utf8tocp C)

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
        utf8tocp.c
)

# -------------------------
# Создаем exe
# -------------------------
add_executable(utf8tocp ${SOURCES})


# -------------------------
# Макросы компилятору
# -------------------------
if(BUILD_UNICODE)
    target_compile_definitions(utf8tocp PRIVATE UNICODE _UNICODE)
    target_compile_options(utf8tocp PRIVATE /source-charset:utf-8)
else ()
    target_compile_options(utf8tocp PRIVATE /source-charset:utf-8 /execution-charset:windows-1251)
endif()

if (MSVC)

endif()

