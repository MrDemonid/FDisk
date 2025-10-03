cmake_minimum_required(VERSION 4.0)
project(tlumacz C)

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
        tlumacz.c
)

# -------------------------
# Создаем exe
# -------------------------
add_executable(tlumacz ${SOURCES})


# -------------------------
# Макросы компилятору
# -------------------------
if(BUILD_UNICODE)
    target_compile_definitions(tlumacz PRIVATE UNICODE _UNICODE)
    target_compile_options(tlumacz PRIVATE /source-charset:utf-8)
else ()
    target_compile_options(tlumacz PRIVATE /source-charset:utf-8 /execution-charset:windows-1251)
endif()

if (MSVC)

endif()

