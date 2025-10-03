#include <windows.h>
#include <stdio.h>
#include <tchar.h>

/*
/IMAGE:"F:\emu\PK8000\CF.CPM",243,16,63 /IMAGE:"F:\Emu80qt_40501\pk8000\cf.img",64,8,8
/IMAGE:"G:\cf-card.img",243,16,63
/XO - расширенные функции редактирования

*/

#define MAIN

#include "source/main.h"
#include "source/environ.h"
#include "source/cmd.h"

#include "source/codepage/codepage.h"
#include "source/ansicon.h"
#include "source/printf.h"
#include "source/pdiskio.h"
#include "source/display.h"
#include "source/svarlang/svarlang.h"
#include "source/fdiskio.h"
#include "source/helpscr.h"
#include "source/pcompute.h"
#include "source/ui.h"

#include "source/logger.h"


#if defined(UNICODE)
#pragma message("=> UNICODE is defined")
#else
#pragma message("=> UNICODE is NOT defined")
#endif

void halt(int code) {
    if (code != 0) {
        Pause();
    }
    con_close();
    if (code == -1)
        code = 0;
    exit(code);
}

/**
 * Определяет, доступна ли в текущей ОС установка UTF-8 в консоли.
 * @return TRUE - если версия ОС Vista+
 */
static BOOL IsWindowsVistaOrGreater() {
    OSVERSIONINFO osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    if (!GetVersionEx(&osvi)) {
        return TRUE;    // Если функция не сработала, считаем Vista+ (не должно происходить)
    }
    /*
        5.1 - XP
        5.2 - XP x64/Server 2003
        6.0 — Vista.
        6.1 - Windows 7.
        6.2 - Windows 8
        6.3 - Windows 8.1
        10.x - Windows 10/11
    */
    // Windows Vista имеет версию 6.0
    if (osvi.dwMajorVersion > 6) return TRUE;
    if (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion >= 0) return TRUE;
    return FALSE; // всё что меньше 6.0 — XP и старше
}


BOOL IsRunAsAdmin(void) {
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdministratorsGroup = NULL;

    // Allocate and initialize a SID for the Administrators group.
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2,SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                 &pAdministratorsGroup)) {
        CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin);
        FreeSid(pAdministratorsGroup);
    }
    return fIsRunAsAdmin == TRUE;
}


/* Initialize flags, variables, load fdisk.ini, load part.ini, etc. */
static void InitOptions(TCHAR *environment[]) {
    /* initialize flags, the ones not set here default to FALSE */
    flags.display_name_description_copyright = TRUE;
    flags.dla = DLA_AUTO; /* drive letter assignment based on OS */
    flags.drive_number = 128;
    flags.flag_sector = 2;
    flags.lba_marker = TRUE;
    flags.screen_color = 0x07; /* light grey on black */
    flags.set_any_pri_part_active = TRUE;
    flags.total_number_hard_disks = 255;
    flags.use_iui = TRUE;
    flags.using_default_drive_number = TRUE;
    flags.version = COMP_W95B;

    /* Clear the user_defined_chs_settings structure */
    int index = 0;
    do {
        user_defined_chs_settings[index].defined = FALSE;
        user_defined_chs_settings[index].total_cylinders = 0;
        user_defined_chs_settings[index].total_heads = 0;
        user_defined_chs_settings[index].total_sectors = 0;

        index++;
    } while (index < MAX_DISKS);

    Process_Fdiskini_File();

    Get_Environment_Settings(environment);

    /* Adjust flags if extended options mode is selected */
    if (flags.extended_options_flag == TRUE) {
        flags.allow_abort = TRUE;
        flags.del_non_dos_log_drives = TRUE;
        flags.set_any_pri_part_active = TRUE;
    }

    /* If the version is W95B or later then default to FAT32 support.  */
    if (flags.version >= COMP_W95B) {
        flags.fat32 = TRUE;
    }

    if (flags.dla == DLA_AUTO) {
        if (os_oem == OEM_DRDOS) {
            flags.dla = DLA_DRDOS;
        } else {
            flags.dla = DLA_MSDOS;
        }
    }
}

static void InitDisks(void) {
    /* Check for interrupt 0x13 extensions (If the proper version is set.) */
    Initialize_LBA_Structures();

    if (Read_Partition_Tables() != 0) {
        con_puts(svarlang_str(255, 0));
        halt(1);
    }

    if (flags.maximum_drive_number == 0) {
        con_puts(svarlang_str(255, 1));
        halt(6);
    }
}

// Добавляем его к списку дисков.
static void mountImageFile(Arg_Structure *arg) {
    if (Mount_Image_File(arg->filename, arg->value, arg->extra_value, arg->additional_value) != 0) {
        con_puts(svarlang_str(255, 0));
        halt(1);
    }
}

static void Ensure_Drive_Number(void) {
    if (flags.using_default_drive_number == TRUE) {
        con_puts(svarlang_str(255, 2));
        halt(9);
    }
}

/**
 * Разбивает путь к программе на путь и имя программы.
 * На выходе:
 *      filename[] - имя программы, без расширения.
 *      path[] - путь до программы.
 */
static void split_path(const TCHAR *thisPath) {
    path[0] = _T('\0');
    filename[0] = _T('\0');
    if (thisPath == NULL)
        return;

    /* Ищем начало имени программы. */
    int index = _tcslen(thisPath);
    int location = 0;
    do {
        if (thisPath[index] == _T('\\')) {
            location = index + 1;
            index = -1;                 // break
        }
        index--;
    } while (index >= 0);

    // Копируем имя программы в filename.
    index = location;
    do {
        filename[index - location] = thisPath[index];
        index++;
    } while (index <= (_tcslen(thisPath)));

    index = 0;
    do {
        if (filename[index] == _T('.')) {
            filename[index] = _T('\0');
        }
        index++;
    } while (index < 12);

    /* Сохраняем путь к программе в path[] */
    if (location > 0) {
        index = 0;
        do {
            path[index] = thisPath[index];
            index++;
        } while (index < location);
        path[index] = 0;
    } else {
        path[0] = _T('\0');
    }
}

/*
/////////////////////////////////////////////////////////////////////////////
//  MAIN ROUTINE
/////////////////////////////////////////////////////////////////////////////
*/
int _tmain(int argc, TCHAR *argv[]) {
    int fat32_temp;

    log_init("error.txt");

    /* initialize console io with interpretation of esc seq enabled */
    DWORD code;
    if ((code = con_init(1)))
    {
        log_printf("con_init() failed with code: 0x%X\n", code);
        show_last_error_box(code, _T("Error creating console"));
        con_close();
        exit((int) code);
    }

    /* Получаем путь до программы и имя программы, без расширения */
    split_path(argv[0]);


    /* initialize the SvarLANG library (loads translation strings)
     * first try to load strings from the same directory where FDISK.EXE
     * resides, and if this fails then try to load them from within the
     * FreeDOS-style %NLSPATH% directory. */
    if (svarlang_autoload_exepath( argv[0], GetLanguageType(_tgetenv(_T("LANG"))) ) != 0) {
        /* if this fails as well it is no big deal, svarlang is still
         * operational and will fall back to its default strings */
        svarlang_autoload_pathlist( filename, _tgetenv(_T("NLSPATH")), GetLanguageType(_tgetenv(_T("LANG"))) );
    }

    Determine_DOS_Version();
    if (os_version > OS_WIN_ME) {
        con_print(svarlang_str(255, 19));
        halt(1);
    }

    InitOptions(environ);
    if (flags.lang_type != LANG_DEFAULT) {
        svarlang_autoload_exepath(argv[0], flags.lang_type);
        con_set_lang(flags.lang_type);
    }

    /* First check to see if the "/?" command-line switch was entered.  If it
      was, then don't bother doing anything else.  Just display the help and
      exit.  This ensures that the hard disks are not accessed.              */
    if (((argc == 2) || (argc == 3)) && (argv[1][1] == _T('?'))) {
        flags.do_not_pause_help_information = FALSE;
        flags.screen_color = 7;

        if (argv[2] != NULL && ((argv[2][1] == _T('N')) || (argv[2][1] == _T('n')))) {
            flags.do_not_pause_help_information = TRUE;
            Shift_Command_Line_Options(1);
        }

        Display_Help_Screen();
        halt(-1);
    }

    InitDisks();

    /* New Parsing Routine */
    /* The command line format is:                                            */
    /* /aaaaaaaaaa:999999,999 9 /aaaaaaaaaa:999999,999 /aaaaaaaaaa:999999,999 /aaaaaaaaaa:999999,999   */
    /* Where:   "a" is an ascii character and "9" is a number                 */
    /* Note:  The second "9" in the above command line format is the drive    */
    /*        number.  This drive number can now be anywhere on the line.     */

    /* If "FDISK" is typed without any options */
    number_of_command_line_options = Get_Options(argv, argc);

    while (number_of_command_line_options > 0) {
        int command_ok = FALSE;

        if (0 == _tcscmp(arg[0].choice, _T("ACTIVATE")) || 0 == _tcscmp(arg[0].choice, _T("ACT"))) {
            flags.use_iui = FALSE;
            if ((arg[0].value < 1) || (arg[0].value > 4)) {
                con_puts(svarlang_str(255, 3));
                halt(9);
            }

            if (!Set_Active_Partition((int) (arg[0].value - 1))) {
                con_puts(svarlang_str(255, 4));
                halt(9);
            }
            command_ok = TRUE;
            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("ACTOK"))) {
            /* ignored */
            command_ok = TRUE;
            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("AUTO"))) {
            flags.use_iui = FALSE;
            if (Automatically_Partition_Hard_Drive()) {
                con_puts(svarlang_str(255, 5));
                halt(9);
            }
            command_ok = TRUE;
            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("CLEARMBR")) || 0 == _tcscmp(arg[0].choice, _T("CLEARALL"))) {
            flags.use_iui = FALSE;
            Ensure_Drive_Number();

            if (Clear_Entire_Sector_Zero() != 0) {
                con_puts(svarlang_str(255, 6));
                halt(8);
            }
            command_ok = TRUE;
            Read_Partition_Tables();
            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("CLEARFLAG"))) {
            flags.use_iui = FALSE;
            Command_Line_Clear_Flag();
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("CLEARIPL"))) {
            flags.use_iui = FALSE;
            Ensure_Drive_Number();

            if (Remove_IPL() != 0) {
                con_puts(svarlang_str(255, 7));
                halt(8);
            }
            command_ok = TRUE;

            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("CMBR"))) {
            flags.use_iui = FALSE;
            if (Create_MBR() != 0) {
                con_puts(svarlang_str(255, 11));
                halt(8);
            }
            command_ok = TRUE;
            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("DEACTIVATE")) || 0 == _tcscmp(arg[0].choice, _T("DEACT"))) {
            flags.use_iui = FALSE;
            if (Deactivate_Active_Partition() != 0 ||
                Write_Partition_Tables() != 0) {
                con_puts(svarlang_str(255, 9));
                halt(9);
            }
            command_ok = TRUE;
            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("DELETE")) || 0 == _tcscmp(arg[0].choice, _T("DEL"))) {
            flags.use_iui = FALSE;
            Ensure_Drive_Number();

            Command_Line_Delete();
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("DELETEALL")) ||
            0 == _tcscmp(arg[0].choice, _T("DELALL")) ||
            0 == _tcscmp(arg[0].choice, _T("CLEAR"))) {
            flags.use_iui = FALSE;
            Ensure_Drive_Number();

            if (Clear_Partition_Table() != 0) {
                con_puts(svarlang_str(255, 10));
                halt(9);
            }
            command_ok = TRUE;
            Read_Partition_Tables();
            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("DUMP"))) {
            flags.use_iui = FALSE;
            Dump_Partition_Information();
            command_ok = TRUE;

            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("EXT"))) {
            flags.use_iui = FALSE;
            Command_Line_Create_Extended_Partition();
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("FPRMT"))) {
            if (flags.version >= COMP_W95B) {
                flags.fprmt = TRUE;
            }
        }

        if (0 == _tcscmp(arg[0].choice, _T("IFEMPTY"))) {
            /* only execute the following commands if part tbl is empty */
            if (!Is_Pri_Tbl_Empty()) {
                halt(-1);
            }
            Shift_Command_Line_Options(1);
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("INFO"))) {
            flags.use_iui = FALSE;
            Command_Line_Info();
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("IPL"))) {
            flags.use_iui = FALSE;
            Ensure_Drive_Number();

            if (Create_MBR() != 0) {
                con_puts(svarlang_str(255, 11));
                halt(8);
            }
            command_ok = TRUE;
            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("LOADIPL")) || 0 == _tcscmp(arg[0].choice, _T("AMBR"))) {
            flags.use_iui = FALSE;
            Ensure_Drive_Number();

            if (Load_MBR(1) != 0) {
                con_puts(svarlang_str(255, 12));
                halt(8);
            }
            command_ok = TRUE;
            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("LOADMBR"))) {
            flags.use_iui = FALSE;
            Ensure_Drive_Number();

            if (Load_MBR(0) != 0) {
                con_puts(svarlang_str(255, 13));
                halt(8);
            }
            command_ok = TRUE;
            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("LOG"))) {
            flags.use_iui = FALSE;
            Command_Line_Create_Logical_DOS_Drive();
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("LOGO"))) {
            flags.use_iui = FALSE;
            fat32_temp = flags.fat32;
            flags.fat32 = FALSE;
            Command_Line_Create_Logical_DOS_Drive();
            flags.fat32 = fat32_temp;
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("MBR"))) {
            flags.use_iui = FALSE;
            if (Create_MBR() != 0) {
                con_puts(svarlang_str(255, 14));
                halt(8);
            }
            command_ok = TRUE;

            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("MODIFY"))) {
            flags.use_iui = FALSE;
            Command_Line_Modify();
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("MOVE"))) {
            flags.use_iui = FALSE;
            Command_Line_Move();
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("NOIPL"))) {
            /* prevent writing IPL upon MBR initialisation */
            flags.no_ipl = TRUE;
            Shift_Command_Line_Options(1);
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("PRI"))) {
            flags.use_iui = FALSE;
            Command_Line_Create_Primary_Partition();
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("PRIO"))) {
            flags.use_iui = FALSE;
            fat32_temp = flags.fat32;
            flags.fat32 = FALSE;
            Command_Line_Create_Primary_Partition();
            flags.fat32 = fat32_temp;
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("Q"))) {
            if (flags.version >= COMP_W95B) {
                Ask_User_About_FAT32_Support();
            }
        }

        if (0 == _tcscmp(arg[0].choice, _T("SETFLAG"))) {
            flags.use_iui = FALSE;
            Command_Line_Set_Flag();
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("SAVEMBR")) || 0 == _tcscmp(arg[0].choice, _T("SMBR"))) {
            flags.use_iui = FALSE;
            Ensure_Drive_Number();

            if (Save_MBR() != 0) {
                con_puts(svarlang_str(255, 16));
                halt(8);
            }
            command_ok = TRUE;

            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("STATUS"))) {
            flags.use_iui = FALSE;
            Command_Line_Status();
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("SWAP"))) {
            flags.use_iui = FALSE;
            Command_Line_Swap();
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("TESTFLAG"))) {
            flags.use_iui = FALSE;
            Command_Line_Test_Flag();
            command_ok = TRUE;
        }

        if (0 == _tcscmp(arg[0].choice, _T("UI"))) {
            flags.use_iui = TRUE;
            command_ok = TRUE;

            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("XO"))) {
            flags.extended_options_flag = TRUE;
            flags.allow_abort = TRUE;
            flags.del_non_dos_log_drives = TRUE;
            flags.set_any_pri_part_active = TRUE;
            command_ok = TRUE;
            Shift_Command_Line_Options(1);
        }

        if (0 == _tcscmp(arg[0].choice, _T("IMAGE"))) {
            // Подключаем образ файла вместо физического диска
            mountImageFile(&arg[0]);
            command_ok = TRUE;
            Shift_Command_Line_Options(1);
        }

        if (arg[0].choice[0] == '?') {
            flags.use_iui = FALSE;
            if (_tcscmp(arg[1].choice, _T("NOPAUSE"))) {
                flags.do_not_pause_help_information = TRUE;
                Shift_Command_Line_Options(1);
            }
            Display_Help_Screen();
            command_ok = TRUE;

            halt(-1);
        }

        if (command_ok == FALSE) {
            con_puts(svarlang_str(255, 18));
            halt(1);
        }
    }

    if (flags.use_iui == TRUE) {
        Interactive_User_Interface();       // запуск меню
    }

    if (Write_Partition_Tables() != 0) {
        con_print(svarlang_str(255, 15));
        con_print(_T("\n"));
        con_readkey();
        halt(8);
    }

    if (flags.use_iui == TRUE) {
        Exit_Screen();
    }
    halt(-1);
}
