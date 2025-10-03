/*
// Program:  Free FDISK
// Module:   FDISKIO.C
// Based on code by Brian E. Reifsnyder
// Copyright: 1998-2008, under the terms of the GNU GPL, Version 2
//
// Modified and adapted for Windows by: Andrey Hlus, 2025
*/

#define FDISKIO
#define _CRT_SECURE_NO_WARNINGS

#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ansicon.h"
#include "compat.h"
#include "fdiskio.h"
#include "main.h"
#include "pcompute.h"
#include "pdiskio.h"
#include "printf.h"
#include "svarlang/svarlang.h"
#include "codepage/codepage.h"


/* bootloader pointers */
/*extern char booteasy_code[];*/
extern char bootnormal_code[];


/* Automatically partition the selected hard drive */
int Automatically_Partition_Hard_Drive(void) {
    int index = 0;

    /*  unsigned long maximum_partition_size_in_MB; */
    Partition_Table *pDrive = &part_table[(flags.drive_number - 128)];

    Determine_Drive_Letters();

    /* First, make sure no primary or extended partitions exist. */
    /* Compaq diagnostic partitions are ok, though.              */
    do {
        if ((brief_partition_table[(flags.drive_number - 128)][index] > 0) &&
            (brief_partition_table[(flags.drive_number - 128)][index] != 18)) {
            /* NLS:The hard drive has already been partitioned */
            con_print(svarlang_str(7, 0));
            return 99;
        }
        index++;
    } while (index < 4);

    /* Create a primary partition...if the size or type is incorrect,     */
    /* int Create_Primary_Partition(...) will adjust it automatically.    */
    Determine_Free_Space();
    int part_no = Create_Primary_Partition(6, 2048);
    if (part_no == 99) {
        return part_no;
    }
    int error_code = Set_Active_Partition(part_no);

    /* Create an extended partition, if space allows. */
    Determine_Free_Space();
    if (pDrive->pri_free_space > 0) {
        part_no = Create_Primary_Partition(5, 999999ul);

        if (part_no == 99) {
            return part_no;
        }

        /* Fill the extended partition with logical drives. */
        Determine_Free_Space();
        do {
            error_code = Create_Logical_Drive(6, 2048);
            if (error_code != 0) {
                return error_code;
            }
            Determine_Free_Space();
        } while ((pDrive->ext_free_space > 0) && (Determine_Drive_Letters() < _T('Z')));
    }

    return 0;
}

/* Clear the first sector on the hard disk...removes the partitions and MBR. */
int Clear_Entire_Sector_Zero(void) {
    // Qprintf("Clearing boot sector of drive %x\n", flags.drive_number);
    Clear_Sector_Buffer();
    return Write_Physical_Sectors(flags.drive_number, 0, 0, 1, 1);
}

/* Clear the Flag */
int Clear_Flag(int flag_number) {
    if (flags.flag_sector == 0) {
        return 9;
    } else if (flags.flag_sector >
               part_table[(flags.drive_number - 128)].total_sect) {
        return 3;
    }

    int error_code = Read_Physical_Sectors((flags.drive_number), 0, 0, (flags.flag_sector), 1);
    sector_buffer[(446 + (flag_number - 1))] = 0;
    error_code |= Write_Physical_Sectors((flags.drive_number), 0, 0, (flags.flag_sector), 1);
    return error_code;
}

/* Clear Partition Table */
int Clear_Partition_Table(void) {
    int error_code = Read_Physical_Sectors(flags.drive_number, 0, 0, 1, 1);
    if (error_code != 0) {
        return error_code;
    }

    memset(sector_buffer + 0x1be, 0, 4 * 16);

    return Write_Physical_Sectors(flags.drive_number, 0, 0, 1, 1);
}

/* Create Alternate Master Boot Code */
int Load_MBR(int ipl_only) {
    FILE *file_pointer;
    TCHAR home_path[_MAX_PATH];
    int index = 0;
    int error_code;
    int c;

    //Qprintf("Load_MBR()\n");
    error_code = Read_Physical_Sectors(flags.drive_number, 0, 0, 1, 1);
    if (error_code != 0) {
        return error_code;
    }

    if (sector_buffer[0x1fe] == 0x55 && sector_buffer[0x1ff] == 0xaa) {
        /* Clear old IPL, if any */
        memset(sector_buffer, 0x00, SIZE_OF_IPL);
    } else {
        /* no MBR currently installed, clear whole sector */
        memset(sector_buffer, 0, sizeof(sector_buffer));

        sector_buffer[0x1fe] = 0x55;
        sector_buffer[0x1ff] = 0xaa;
    }

    _tcscpy(home_path, path);
    _tcscat(home_path, _T("boot.mbr"));

    /* Search the directory Free FDISK resides in before searching the PATH */
    /* in the environment for the boot.mbr file.                            */
    file_pointer = _tfopen(home_path, _T("rb"));

    if (!file_pointer) {
        file_pointer = _tfopen(searchpath(_T("boot.mbr")), _T("rb"));
    }

    if (!file_pointer) {
        /* NLS:The "boot.mbr" file has not been found */
        con_print(svarlang_str(7, 1));
        return 8;
    }

    index = 0;
    if (ipl_only) {
        do {
            c = fgetc(file_pointer);
            sector_buffer[index] = (c != EOF) ? c : 0;
            index++;
        } while (index < SIZE_OF_IPL);
    } else {
        do {
            c = fgetc(file_pointer);
            sector_buffer[index] = c;

            if (c == EOF) {
                fclose(file_pointer);
                return 9;
            }
            index++;
        } while (index < SECT_SIZE);
    }

    fclose(file_pointer);

    error_code = Write_Physical_Sectors(flags.drive_number, 0, 0, 1, 1);

    return error_code;
}

/* Create Booteasy MBR */
/* DISABLED
void Create_BootEasy_MBR( void )
{
   Read_Physical_Sectors( flags.drive_number, 0, 0, 1, 1 );

   memcpy( sector_buffer, booteasy_code, SIZE_OF_IPL );

   sector_buffer[0x1fe] = 0x55;
   sector_buffer[0x1ff] = 0xaa;

   Write_Physical_Sectors( flags.drive_number, 0, 0, 1, 1 );
}*/

/* Create Normal MBR */
static int Create_BootNormal_MBR(void)
{
    int error_code = Read_Physical_Sectors(flags.drive_number, 0, 0, 1, 1);
    if (error_code != 0) {
        return error_code;
    }

    if (sector_buffer[0x1fe] != 0x55 || sector_buffer[0x1ff] != 0xaa) {
        /* no MBR currently installed, clear whole sector */
        memset(sector_buffer, 0, sizeof(sector_buffer));

        sector_buffer[0x1fe] = 0x55;
        sector_buffer[0x1ff] = 0xaa;
    }

    memcpy(sector_buffer, bootnormal_code, SIZE_OF_IPL);

    return Write_Physical_Sectors(flags.drive_number, 0, 0, 1, 1);
}

#ifdef SMART_MBR
/* Create Normal MBR */
extern void __cdecl __far BootSmart_code();

int Create_BootSmart_IPL(void) {
    int error_code;

    /* NLS:Creating Drive Smart MBR for disk %d */
    con_printf(svarlang_str(7, 6), flags.drive_number - 0x7F);

    error_code = Read_Physical_Sectors(flags.drive_number, 0, 0, 1, 1);
    if (error_code != 0) {
        return error_code;
    }

    if (sector_buffer[0x1fe] != 0x55 || sector_buffer[0x1ff] != 0xaa) {
        /* no MBR currently installed, clear whole sector */
        memset(sector_buffer, 0, sizeof(sector_buffer));

        sector_buffer[0x1fe] = 0x55;
        sector_buffer[0x1ff] = 0xaa;
    }

    _fmemcpy(sector_buffer, BootSmart_code, SIZE_OF_IPL);

    return Write_Physical_Sectors(flags.drive_number, 0, 0, 1, 1);
}
#endif

/* Create Master Boot Code */
int Create_MBR(void) {
    if (flags.use_ambr == TRUE) {
        return Load_MBR(1);
    } else {
        return Create_BootNormal_MBR(); /* BootEasy disabled */
    }
}

/* parse bool_text for "ON" or "OFF" and set integer var accordingly */
int bool_string_to_int(int *var, const TCHAR *bool_text) {
    if (0 == _tcsicmp(bool_text, _T("ON"))) {
        *var = TRUE;
        return 1;
    }
    if (0 == _tcsicmp(bool_text, _T("OFF"))) {
        *var = FALSE;
        return 1;
    }
    return 0;
}

/* Read and process the fdisk.ini file */
void Process_Fdiskini_File(void) {
    /* NLS:Error encountered on line %d of the "fdisk.ini" file. */
    const TCHAR *error_str = svarlang_str(7, 3);
    //  char char_number[2];
    char command_buffer[20];
    TCHAR home_path[_MAX_PATH];
    char line_buffer[256];
    char setting_buffer[20];

    long line_counter = 1;
    //  long setting;

    _tcscpy(home_path, path);
    _tcscat(home_path, _T("fdisk.ini"));

    /* Search the directory Free FDISK resides in before searching the PATH */
    /* in the environment for the fdisk.ini file.                           */
    FILE *file = _tfopen(home_path, _T("rt"));

    if (!file) {
        TCHAR *sp = searchpath(_T("fdisk.ini"));
        if (sp)
            file = _tfopen(sp, _T("rt"));
    }

    if (file) {
        int done_looking = FALSE;
        int end_of_file_marker_encountered = FALSE;
        int sub_buffer_index = 0;
        int object_found = FALSE;
        int number;
        int command_ok = FALSE;
        int index = 0;
        while (fgets(line_buffer, 255, file) != NULL) {
            if ((0 != strncmp(line_buffer, ";", 1)) &&
                (0 != strncmp(line_buffer, "end", 3)) &&
                (0 != strncmp(line_buffer, "END", 3)) &&
                (0 != strncmp(line_buffer, "999", 3)) &&
                (line_buffer[0] != 0x0a) &&
                (end_of_file_marker_encountered == FALSE)) {
                /* Clear the command_buffer and setting_buffer */
                for (index = 0; index < 20; index++) {
                    command_buffer[index] = 0x00;
                    setting_buffer[index] = 0x00;
                }

                /* Extract the command and setting from the line_buffer */

                /* Find the command */
                index = 0;
                sub_buffer_index = 0;
                done_looking = FALSE;
                object_found = FALSE;
                do {
                    if ((line_buffer[index] != '=') &&
                        ((line_buffer[index] >= 0x30) &&
                         (line_buffer[index] <= 0x7a))) {
                        object_found = TRUE;
                        command_buffer[sub_buffer_index] = line_buffer[index];
                        sub_buffer_index++;
                    }

                    if ((object_found == TRUE) &&
                        ((line_buffer[index] == '=') ||
                         (line_buffer[index] == ' '))) {
                        //command_buffer[sub_buffer_index]=0x0a;
                        done_looking = TRUE;
                    }

                    if ((index == 254) || (line_buffer[index] == 0x0a)) {
                        con_printf(error_str, line_counter);
                        halt(3);
                    }

                    index++;
                } while (done_looking == FALSE);

                /* Find the setting */
                sub_buffer_index = 0;
                object_found = FALSE;
                done_looking = FALSE;

                do {
                    if ((line_buffer[index] != '=') &&
                        (((line_buffer[index] >= 0x30) &&
                          (line_buffer[index] <= 0x7a)) ||
                         (line_buffer[index] == '-'))) {
                        object_found = TRUE;
                        setting_buffer[sub_buffer_index] = line_buffer[index];
                        sub_buffer_index++;
                    }

                    if ((object_found == TRUE) &&
                        ((line_buffer[index] == 0x0a) ||
                         (line_buffer[index] == ' ') ||
                         (line_buffer[index] == 0))) {
                        done_looking = TRUE;
                        //setting_buffer[sub_buffer_index]=0x0a;
                    }

                    if (index == 254) {
                        con_printf(error_str, line_counter);
                        halt(3);
                    }

                    index++;
                } while (done_looking == FALSE);

                /* Adjust for the possibility of TRUE or FALSE in the fdisk.ini file. */
                if (0 == _stricmp(setting_buffer, "TRUE")) {
                    _tcscpy(setting_buffer, "ON");
                }
                if (0 == _stricmp(setting_buffer, "FALSE")) {
                    _tcscpy(setting_buffer, "OFF");
                }

                /* Process the command found in the line buffer */

                command_ok = FALSE;

                /* Align partitions to 4k */
                if (0 == _stricmp(command_buffer, "ALIGN_4K")) {
                    if (!bool_string_to_int(&flags.align_4k, setting_buffer)) {
                        goto parse_error;
                    }
                    command_ok = TRUE;
                }

                /* Check for the ALLOW_4GB_FAT16 statement */
                if (0 == _stricmp(command_buffer, "ALLOW_4GB_FAT16")) {
                    if (!bool_string_to_int(&flags.allow_4gb_fat16,
                                            setting_buffer)) {
                        goto parse_error;
                    }
                    command_ok = TRUE;
                }

                /* Check for the ALLOW_ABORT statement */
                if (0 == _stricmp(command_buffer, "ALLOW_ABORT")) {
                    if (!bool_string_to_int(&flags.allow_abort,
                                            setting_buffer)) {
                        goto parse_error;
                    }
                    command_ok = TRUE;
                }

                /* Check for the AMBR statement */
                if (0 == _stricmp(command_buffer, "AMBR")) {
                    if (!bool_string_to_int(&flags.use_ambr, setting_buffer)) {
                        goto parse_error;
                    }
                    command_ok = TRUE;
                }

                /* Check for the CHECKEXTRA statement */
                if (0 == _stricmp(command_buffer, "CHECKEXTRA")) {
                    if (!bool_string_to_int(&flags.check_for_extra_cylinder,
                                            setting_buffer)) {
                        goto parse_error;
                    }
                    command_ok = TRUE;
                }

                /* Check for the COLORS statement */
                if (0 == _stricmp(command_buffer, "COLORS")) {
                    number = _tstoi(setting_buffer);

                    if ((number >= 0) && (number <= 127)) {
                        flags.screen_color = number;
                    } else {
                        goto parse_error;
                    }
                    command_ok = TRUE;
                }

                /* Check for the COLORS statement */
                if (0 == _stricmp(command_buffer, "DLA")) {
                    number = _tstoi(setting_buffer);

                    if ((number >= 0) && (number <= 2)) {
                        flags.dla = number;
                    } else {
                        goto parse_error;
                    }
                    command_ok = TRUE;
                }

                /* Check for the DEL_ND_LOG statement */
                if (0 == _stricmp(command_buffer, "DEL_ND_LOG")) {
                    if (!bool_string_to_int(&flags.del_non_dos_log_drives,
                                            setting_buffer)) {
                        goto parse_error;
                    }
                    command_ok = TRUE;
                }

                /* Check for the DRIVE statement */
                if (0 == _stricmp(command_buffer, "DRIVE")) {
                    if (13 != strlen(setting_buffer)) {
                        command_ok = FALSE;
                    } else {
                        char conversion_buffer[5];

                        int drive = setting_buffer[0] - '1';

                        conversion_buffer[4] = 0;
                        conversion_buffer[3] = 0;
                        conversion_buffer[2] = 0;

                        conversion_buffer[0] = setting_buffer[11];  // последние 2 цифры
                        conversion_buffer[1] = setting_buffer[12];

                        user_defined_chs_settings[drive].total_sectors = _tstol(conversion_buffer);

                        conversion_buffer[0] = setting_buffer[7];
                        conversion_buffer[1] = setting_buffer[8];
                        conversion_buffer[2] = setting_buffer[9];

                        user_defined_chs_settings[drive].total_heads = _tstol(conversion_buffer) - 1;

                        conversion_buffer[0] = setting_buffer[2];
                        conversion_buffer[1] = setting_buffer[3];
                        conversion_buffer[2] = setting_buffer[4];
                        conversion_buffer[3] = setting_buffer[5];

                        user_defined_chs_settings[drive].total_cylinders = _tstol(conversion_buffer) - 1;

                        user_defined_chs_settings[drive].defined = TRUE;

                        command_ok = TRUE;
                    }
                }

                /* Check for the FLAG_SECTOR statement */
                if (0 == _stricmp(command_buffer, "FLAG_SECTOR")) {
                    number = _tstoi(setting_buffer);
                    if (number == 0) {
                        flags.flag_sector = 0;
                    } else if ((number >= 2) && (number <= 64)) {
                        flags.flag_sector = number;
                    } else if (number == 256) {
                        flags.flag_sector = part_table[0].total_sect;
                    } else {
                        goto parse_error;
                    }
                    command_ok = TRUE;
                }

                /* Check for the LABEL statement: removed 2023/07/15*/
                if (0 == _stricmp(command_buffer, "LABEL")) {
                    command_ok = TRUE;
                }

                /* Check for the LBA_MARKER statement */
                if (0 == _stricmp(command_buffer, "LBA_MARKER")) {
                    if (!bool_string_to_int(&flags.lba_marker,
                                            setting_buffer)) {
                        goto parse_error;
                    }
                    command_ok = TRUE;
                }

                /* Check for the SET_ANY_ACT statement */
                if (0 == _stricmp(command_buffer, "SET_ANY_ACT")) {
                    if (!bool_string_to_int(&flags.set_any_pri_part_active,
                                            setting_buffer)) {
                        goto parse_error;
                    }
                    command_ok = TRUE;
                }

                /* Check for the VERSION statement */
                if (0 == _stricmp(command_buffer, "VERSION")) {
                    if (0 == _stricmp(setting_buffer, "4")) {
                        flags.version = COMP_FOUR;
                    } else if (0 == _stricmp(setting_buffer, "5")) {
                        flags.version = COMP_FIVE;
                    } else if (0 == _stricmp(setting_buffer, "6")) {
                        flags.version = COMP_SIX;
                    } else if (0 == _stricmp(setting_buffer, "W95")) {
                        flags.version = COMP_W95;
                    } else if (0 == _stricmp(setting_buffer, "W95B")) {
                        flags.version = COMP_W95B;
                    } else if (0 == _stricmp(setting_buffer, "W98")) {
                        flags.version = COMP_W98;
                    } else if (0 == _stricmp(setting_buffer, "FD")) {
                        flags.version = COMP_FD;
                    } else {
                        goto parse_error;
                    }
                    command_ok = TRUE;
                }

                /* Check for the XO statement */
                if (0 == _stricmp(command_buffer, "XO")) {
                    if (!bool_string_to_int(&flags.extended_options_flag,
                                            setting_buffer)) {
                        goto parse_error;
                    }
                    command_ok = TRUE;
                }

                /* Check for the LANG statement */
                if (0 == _stricmp(command_buffer, "LANG")) {
                    setting_buffer[2] = _T('\0');
                    flags.lang_type = GetLanguageType( setting_buffer );
                    command_ok = TRUE;
                }

                if (command_ok == FALSE) {
                    goto parse_error;
                }
            }

            if ((0 == strncmp(line_buffer, "999", 3)) &&
                (0 == strncmp(line_buffer, "end", 3)) &&
                (0 == strncmp(line_buffer, "END", 3))) {
                end_of_file_marker_encountered = TRUE;
            }

            line_counter++;
        }
        fclose(file);
    }

    return;

parse_error:
    con_printf(error_str, line_counter);
    halt(3);
}

/* Remove MBR */
int Remove_IPL(void) {
    int error_code = Read_Physical_Sectors(flags.drive_number, 0, 0, 1, 1);
    if (error_code != 0) {
        return error_code;
    }

    memset(sector_buffer, 0, SIZE_OF_IPL);

    return Write_Physical_Sectors((flags.drive_number), 0, 0, 1, 1);
}

/* Save MBR */
int Save_MBR(void) {
    int index = 0;

    int error_code = Read_Physical_Sectors(flags.drive_number, 0, 0, 1, 1);
    if (error_code != 0) {
        return error_code;
    }

    FILE *file_pointer = _tfopen(_T("boot.mbr"), _T("wb"));

    if (!file_pointer) {
        return 8;
    }

    do {
        if (fputc(sector_buffer[index], file_pointer) == EOF) {
            fclose(file_pointer);
            return 8;
        }
        index++;
    } while (index < SECT_SIZE);

    fclose(file_pointer);
    return 0;
}

/* Set the flag */
int Set_Flag(int flag_number, int flag_value) {
    if (flags.flag_sector == 0) {
        return 9;
    }
    if (flags.flag_sector > part_table[(flags.drive_number - 128)].total_sect) {
        return 3;
    }

    int error_code = Read_Physical_Sectors(flags.drive_number, 0, 0, flags.flag_sector, 1);
    if (error_code != 0) {
        return error_code;
    }
    sector_buffer[(446 + (flag_number - 1))] = flag_value;
    return Write_Physical_Sectors(flags.drive_number, 0, 0, flags.flag_sector, 1);
}

/* Test the flag */
int Test_Flag(int flag_number) {
    if (flags.flag_sector != 0) {
        if (Read_Physical_Sectors(flags.drive_number, 0, 0, flags.flag_sector, 1) != 0) {
            /* NLS:Error reading sector. */
            con_print(svarlang_str(7, 4));
            halt(8);
        }
    } else {
        /* NLS:Sector flagging functions have been disabled. */
        con_print(svarlang_str(7, 5));
        halt(9);
    }
    return (sector_buffer[(446 + flag_number - 1)]);
}
