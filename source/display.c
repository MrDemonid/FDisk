/*
// Program:  Free FDISK
// Module:   DISPLAY.C
// Based on code by Brian E. Reifsnyder
// Copyright: 1998-2008, under the terms of the GNU GPL, Version 2
//
// Modified and adapted for Windows by: Andrey Hlus, 2025
*/

/* contains UI calls that required by command-line operations */

#include <tchar.h>
#include <stdio.h>
#include <string.h>

#include "ansicon.h"
#include "display.h"
#include "main.h"
#include "pcompute.h"
#include "printf.h"
#include "svarlang/svarlang.h"

/* Pause Routine */
void Pause(void) {
    con_putc(_T('\n'));
    con_print(svarlang_str(250, 3));

    /* wait for keypress */
    con_readkey();

    con_putc(_T('\r'));
    con_clreol();
}

/* Display Information */
void Display_Information(void) {
    if (flags.extended_options_flag == TRUE) {
        con_set_cursor_xy(1, 1);
        if (flags.version == COMP_FOUR) {
            con_print(_T("4"));
        }
        if (flags.version == COMP_FIVE) {
            con_print(_T("5"));
        }
        if (flags.version == COMP_SIX) {
            con_print(_T("6"));
        }
        if (flags.version == COMP_W95) {
            con_print(_T("W95"));
        }
        if (flags.version == COMP_W95B) {
            con_print(_T("W95B"));
        }
        if (flags.version == COMP_W98) {
            con_print(_T("W98"));
        }

        if (flags.partition_type_lookup_table == INTERNAL) {
            con_set_cursor_xy(6, 1);
            con_print(_T("INT"));
        } else {
            con_set_cursor_xy(6, 1);
            con_print(_T("EXT"));
        }

        con_set_cursor_xy(10, 1);
        con_print(_T("LBA"));

        if (flags.fat32 == TRUE) {
            con_set_cursor_xy(14, 1);
            con_print(_T("FAT32"));
        }

        con_set_cursor_xy(68, 1);
        con_printf(_T("DLA%u"), flags.dla);

        if (flags.use_ambr == TRUE) {
            con_set_cursor_xy(73, 1);
            con_print(_T("AMBR"));
        }

        if (flags.partitions_have_changed == TRUE) {
            con_set_cursor_xy(78, 1);
            con_print(_T("C"));
        }

        if (flags.extended_options_flag == TRUE) {
            con_set_cursor_xy(80, 1);
            con_print(_T("X"));
        }
    }

#ifndef RELEASE
    con_set_cursor_xy(1, flags.extended_options_flag ? 2 : 1);
    con_print(_T("NON-RELEASE BUILD"));
    con_set_cursor_xy(61, flags.extended_options_flag ? 2 : 1);
    con_print(_T(__DATE__ " " __TIME__));
#endif

#ifdef DEBUG
    con_set_cursor_xy(61, 1);
    con_print(_T("DEBUG"));

    if (debug.write == FALSE) {
        con_set_cursor_xy(70, 1);
        con_print(_T("RO"));
    }
#endif
}

/* Print Centered Text */
void Print_Centered(int y, const TCHAR *text, int style) {
    int x = (int) (40 - _tcslen(text) / 2);
    int was_bold = con_get_bold();

    con_set_cursor_xy(x + 1, y + 1);

    if (style == BOLD) {
        con_set_bold(1);
    }
    con_print(text);
    con_set_bold(was_bold);
}

/* Print 7 Digit Unsigned Long Values */
void Print_UL(unsigned long number) {
    con_printf(_T("%7lu"), number);
}

/* Print 7 Digit Unsigned Long Values in bold print */
void Print_UL_B(unsigned long number) {
    con_printf(_T("\33[1m%7lu\33[22m"), number);
}

void PrintProductName(TCHAR product[]) {
    TCHAR name[20];
    _tcsncpy(name, product, sizeof(name)/sizeof(TCHAR));
    name[20 - 1] = _T('\0');
    int cx = con_get_cursor_x() + 10;
    con_set_cursor_xy(cx - _tcslen(name) / 2, con_get_cursor_y());
    con_printf(_T("%s"), name);
}

/* Dump the partition tables from all drives to screen */
void Dump_Partition_Information(void) {
    int index = 0;
    //flags.extended_options_flag=TRUE;

    do {
        flags.drive_number = index + 128;
        Display_CL_Partition_Table();
        index++;
    } while (index + 128 <= flags.maximum_drive_number);
}

void Display_CL_Partition_Table(void) {
    int index = 0;
    unsigned long usage = 0;
    Partition_Table *pDrive = &part_table[flags.drive_number - 0x80];
    Partition *p;

    Determine_Drive_Letters();

    /* NLS:Current fixed disk drive: */
    con_printf(_T("\n%s %1d"), svarlang_str(9, 0), flags.drive_number - 127);

    con_printf(_T("   %lu %s %lu/%03lu/%02lu"), pDrive->disk_size_sect,
               svarlang_str(9, 3), pDrive->total_cyl + 1,
               pDrive->total_head + 1, pDrive->total_sect);

    if (!Is_Pri_Tbl_Empty()) {
        /* NLS:Partition   Status   Mbytes   Description [...] */
        con_printf(svarlang_str(9, 10));
    } else {
        /*NLS:No partitions defined. */
        con_print(_T("\n\n"));
        con_print(svarlang_str(9, 4));
        con_print(_T("\n"));
    }

    index = 0;
    do {
        p = &pDrive->pri_part[index];
        if (p->num_type > 0) {
            /* Drive Letter of Partition */
            if (IsRecognizedFatPartition(p->num_type)) {
                con_printf(_T(" %1c:"), drive_letter_or_questionmark(
                               drive_lettering_buffer[(flags.drive_number - 128)][index]));
            } else {
                con_print(_T("   "));
            }

            /* Partition Number */
            con_printf(_T(" %1d"), (index + 1));

            /* Partition Type */
            con_printf(_T(" %3d"), (p->num_type));

            /* Status */
            if (p->active_status > 0) {
                con_print(_T("      A"));
            } else {
                con_print(_T("       "));
            }

            /* Mbytes */
            con_print(_T("    "));
            Print_UL(p->size_in_MB);

            /* Description */
            con_printf(_T("   %-15s"), part_type_descr(p->num_type));

            /* Usage */
            usage = Convert_To_Percentage(p->size_in_MB, pDrive->disk_size_mb);

            con_printf(_T("   %3lu%%"), usage);

            /* Starting Cylinder */
            con_printf(_T("%6lu/%03lu/%02lu"), p->start_cyl, p->start_head, p->start_sect);

            /* Ending Cylinder */
            con_printf(_T(" %6lu/%03lu/%02lu"), p->end_cyl, p->end_head, p->end_sect);
            con_putc(_T('\n'));
        }

        index++;
    } while (index < 4);
    /* NLS:Largest continious free space for primary partition */
    con_printf(svarlang_str(9, 5), Max_Pri_Free_Space_In_MB());

    if (pDrive->ptr_ext_part && !pDrive->ext_usable) {
        /* NLS:No usable extended partition found. */
        con_print(svarlang_str(8, 7));
        return;
    }

    /* Check to see if there are any drives to display */
    if ((brief_partition_table[(flags.drive_number - 128)][4] > 0) ||
        (brief_partition_table[(flags.drive_number - 128)][5] > 0)) {
        /* NLS:Contents of Extended DOS Partition: */
        con_printf(svarlang_str(9, 6));
        /* NLS:Drv Volume Label  Mbytes  System [...] */
        con_printf(svarlang_str(9, 11));

        /* Display information for each Logical DOS Drive */
        index = 4;
        do {
            p = &pDrive->log_drive[index - 4];
            if (brief_partition_table[(flags.drive_number - 128)][index] > 0) {
                if (IsRecognizedFatPartition(brief_partition_table[(flags.drive_number - 128)][index])) {
                    /* Display drive letter */
                    con_printf(_T(" %1c:"), drive_letter_or_questionmark(
                                   drive_lettering_buffer[(flags.drive_number - 128)][index]));

                    /* Display volume label */
                    con_printf(_T(" %11s"), A2T(p->vol_label));
                } else {
                    con_printf(_T("               "));
                }

                /* Display size in MB */
                con_print(_T("  "));
                Print_UL(p->size_in_MB);

                /* Display file system type */
                con_printf(_T("  %-15s"), part_type_descr(p->num_type));

                /* Display usage in % */
                usage =
                        Convert_To_Percentage(p->num_sect, pDrive->ext_num_sect);

                con_printf(_T("  %3lu%%"), usage);

                /* Starting Cylinder */
                con_printf(_T("%6lu/%03lu/%02lu"), p->start_cyl, p->start_head, p->start_sect);

                /* Ending Cylinder */
                con_printf(_T(" %6lu/%03lu/%02lu"), p->end_cyl, p->end_head, p->end_sect);
                con_putc(_T('\n'));
            }

            index++;
        } while (index < 27);
        /*NLS:Largest continious free space in extended partition [...] */
        con_printf(svarlang_str(9, 7), Max_Log_Free_Space_In_MB());
    }
    con_putc(_T('\n'));
}


/* Display information for all hard drives */
void Display_All_Drives(int y, const TCHAR *title, int bold) {
    int current_line = 4;
    int max_line = current_line;
    int drive = 1;
    int drive_letter_index = 0;
    int index;
    int last_line = (int) con_get_height() - 6;
    unsigned long usage;

    con_enable_attr( TRUE );
    con_clrscr();
    Determine_Drive_Letters();
    Print_Centered( y, title, bold );

    con_set_cursor_xy(3, 3);
    /* NLS:Disk       Product name       Drv   Mbytes    Free  Usage */
    con_print(svarlang_str(9, 12));

    for (drive = 1; drive <= flags.maximum_drive_number - 127; drive++) {
        int current_column_offset = 4;

        if (current_line > last_line) {
            // превысили лимит высоты, делаем паузу
            current_line = 4;
            con_set_cursor_xy(3, last_line + 4);
            Pause();
            con_clrscr();
            Print_Centered( y, title, bold );
            con_set_cursor_xy(3, 3);
            /* NLS:Disk       Product name       Drv   Mbytes    Free  Usage */
            con_print(svarlang_str(9, 12));
        }

        /* Print physical drive information */
        int current_column_offset_of_general_drive_information = current_column_offset;
        int current_line_of_general_drive_information = current_line;
        unsigned long space_used_on_drive_in_MB = 0;

        /* Print drive number */
        con_set_cursor_xy(current_column_offset_of_general_drive_information + 1, current_line);
        con_printf(ESC_BOLD_ON _T("%d") ESC_BOLD_OFF, drive);

        /* Print the product name */
        con_set_cursor_xy(current_column_offset_of_general_drive_information + 6, current_line);
        PrintProductName(part_table[drive - 1].product);

        /* Print size of drive */

        if (!part_table[drive - 1].usable) {
            /* NLS:-------- unusable --------- */
            con_set_cursor_xy(current_column_offset + 30, current_line );
            con_printf(svarlang_str(9, 8));
            current_line++;
            continue;
        }

        con_set_cursor_xy((current_column_offset_of_general_drive_information + 34), current_line);
        Print_UL(part_table[drive - 1].disk_size_mb);

        /* Get space_used_on_drive_in_MB */
        for (index = 0; index <= 3; index++) {
            if ((part_table[drive - 1].pri_part[index].num_type != 5) &&
                (part_table[drive - 1].pri_part[index].num_type != 15) &&
                (part_table[drive - 1].pri_part[index].num_type != 0)) {
                space_used_on_drive_in_MB += part_table[drive - 1].pri_part[index].size_in_MB;
            }
        }

        for (index = 0; index < MAX_LOGICAL_DRIVES; index++) {
            if (part_table[drive - 1].log_drive[index].num_type > 0) {
                space_used_on_drive_in_MB += part_table[drive - 1].log_drive[index].size_in_MB;
            }
        }

        /* Print logical drives on disk, if applicable */
        drive_letter_index = 0;
        do {
            if (drive_lettering_buffer[drive - 1][drive_letter_index] > 0) {
                current_line++;

                if (current_line > last_line) {
                    // превысили лимит высоты, делаем паузу
                    current_line = 4;
                    con_set_cursor_xy(3, last_line + 4);
                    Pause();
                    con_clrscr();
                    Print_Centered( y, title, bold );
                    con_set_cursor_xy(3, 3);
                    /* NLS:Disk       Product name       Drv   Mbytes    Free  Usage */
                    con_print(svarlang_str(9, 12));
                }

                /* Print drive letter of logical drive */
                if (((drive_lettering_buffer[drive - 1][drive_letter_index] >= _T('C')) &&
                     (drive_lettering_buffer[drive - 1][drive_letter_index] <= _T('Z'))) ||
                    (flags.del_non_dos_log_drives == TRUE)) {
                    con_set_cursor_xy(current_column_offset + 30, current_line);
                    con_printf(_T("%c:"), drive_lettering_buffer[drive - 1][drive_letter_index]);
                }

                /* Print size of logical drive */
                con_set_cursor_xy(current_column_offset + 34, current_line);

                if (drive_letter_index < 4) {
                    Print_UL(part_table[drive - 1].pri_part[drive_letter_index].size_in_MB);
                } else {
                    Print_UL(part_table[drive - 1].log_drive[(drive_letter_index - 4)].size_in_MB);
                }
            }

            if (current_line > max_line)
                max_line = current_line;
            drive_letter_index++;
        } while (drive_letter_index < 27);

        /* Print amount of free space on drive */
        if (part_table[drive - 1].disk_size_mb > space_used_on_drive_in_MB) {
            con_set_cursor_xy(
                (current_column_offset_of_general_drive_information + 42),
                current_line_of_general_drive_information );
            Print_UL(part_table[drive - 1].disk_size_mb -
                     space_used_on_drive_in_MB);
        }

        /* Print drive usage percentage */
        if (space_used_on_drive_in_MB == 0) {
            usage = 0;
        } else {
            usage = Convert_To_Percentage(space_used_on_drive_in_MB,
                                          part_table[drive - 1].disk_size_mb);
        }

        con_set_cursor_xy(
            (current_column_offset_of_general_drive_information + 52),
            current_line_of_general_drive_information);
        con_printf(_T("%3d%%"), usage);

        current_line++;
    }

    con_set_cursor_xy(5, max_line < last_line ? max_line + 3 : last_line+2);
    /*NLS:(1 Mbyte = 1048576 bytes) */
    con_print(svarlang_str(9, 9));
}


static struct {
    int id;
    const TCHAR *descr; /* 17 characters max. */
    const TCHAR *descr_short; /*  9 characters max. */
} pt_descr[] = {
    {0x00, _T("Unused")},
    {0x01, _T("FAT-12"), _T("FAT12")},
    {0x02, _T("CP/M (PK8000)"), _T("CP/M")},
    {0x04, _T("FAT-16 <32M"), _T("FAT16")},
    {0x05, _T("Extended"), _T("Ext")},
    {0x06, _T("FAT-16"), _T("FAT16")},
    {0x07, _T("NTFS / HPFS"), _T("NTFS")},
    {0x0b, _T("FAT-32"), _T("FAT32")},
    {0x0c, _T("FAT-32 LBA"), _T("FAT32 LBA")},
    {0x0e, _T("FAT-16 LBA"), _T("FAT16 LBA")},
    {0x0f, _T("Extended LBA"), _T("Ext LBA")},

    {0x11, _T("Hid. FAT-12"), _T("H FAT12")},
    {0x14, _T("Hid. FAT-16<32M"), _T("H FAT16")},
    {0x15, _T("Hid. Extended"), _T("H Ext")},
    {0x16, _T("Hid. FAT-16"), _T("H FAT16")},
    {0x17, _T("Hid. NTFS/HPFS"), _T("H NTFS")},
    {0x1b, _T("Hid. FAT-32"), _T("H FAT32")},
    {0x1c, _T("Hid. FAT-32 LBA"), _T("H FAT32 L")},
    {0x1e, _T("Hid. FAT-16 LBA"), _T("H FAT16 L")},
    {0x1f, _T("Hid. Ext. LBA"), _T("H Ext L")},

    {0x82, _T("Linux Swap"), _T("LinuxSwap")},
    {0x83, _T("Linux")},

    {0xa5, _T("BSD")},
    {0xa6, _T("OpenBSD")},
    {0xa9, _T("NetBSD")},

    {0xee, _T("GPT protective"), _T("GPTprot")},
    {0xeb, _T("EFI")},

    {-1, _T("Unknown")}
};

const TCHAR *part_type_descr(int id) {
    int i;

    for (i = 0; pt_descr[i].id != id && pt_descr[i].id >= 0; i++);
    return pt_descr[i].descr;
}

const TCHAR *part_type_descr_short(int id) {
    int i;

    for (i = 0; pt_descr[i].id != id && pt_descr[i].id >= 0; i++);
    return (pt_descr[i].descr_short) ? pt_descr[i].descr_short : pt_descr[i].descr;
}
