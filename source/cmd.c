/*
// Program:  Free FDISK
// Module:   CMD.C
// Module Description: Command handling code module
// Written By: Brian E. Reifsnyder
// Version:  1.2.1
// Copyright: 1998-2001 under the terms of the GNU GPL, Version 2
//
// Modifications (Windows adaptation, 2025):
//   © Andrey Hlus
*/

#define _CRT_SECURE_NO_WARNINGS

#include <tchar.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "ansicon.h"
#include "cmd.h"
#include "display.h"
#include "fdiskio.h"
#include "main.h"
#include "pcompute.h"
#include "pdiskio.h"
#include "printf.h"
#include "svarlang/svarlang.h"



/* /CLEARFLAG command line option */
void Command_Line_Clear_Flag( void )
{
   int option_count = 1;

   if ( ( 0 == _tcscmp( arg[1].choice, _T("ALL") ) ) && ( arg[0].value != 0 ) ) {
      /* NLS:Syntax Error\n\nProgram Terminated */
      con_print( svarlang_str( 8, 0 ) );
      halt( 1 );
   }

   if ( 0 == _tcscmp( arg[1].choice, _T("ALL") ) ) {
      int index = 1;

      option_count = 2;

      do {
         Clear_Flag( index );

         index++;
      } while ( index <= 64 );

      /* NLS:All flags have been cleared. */
      con_print( svarlang_str( 8, 41 ) );
   }
   else {
      if ( Clear_Flag( (int)arg[0].value ) != 0 ) {
         /* NLS:Error clearing flag. */
         con_print( svarlang_str( 8, 1 ) );
         halt( 9 );
      }

      /* NLS:Flag %d has been cleared */
      con_printf( svarlang_str( 8, 2 ), arg[0].value );
   }

   Shift_Command_Line_Options( option_count );
}

/* /EXT command line options */
void Command_Line_Create_Extended_Partition( void )
{
   unsigned long maximum_possible_percentage;
   unsigned long maximum_partition_size_in_MB;
   int error_code = 0;

   Partition_Table *pDrive = &part_table[flags.drive_number - 0x80];

   if ( arg[0].value == 0 ) {
      /* NLS:Invalid partition size specifed. */
      con_print( svarlang_str( 8, 3 ) );
      halt( 9 );
   }

   if ( pDrive->ptr_ext_part ) {
      /* NLS:Extended partition already exists. */
      con_print( svarlang_str( 8, 4 ) );
      halt( 9 );
   }

   Determine_Free_Space();

   maximum_partition_size_in_MB = Max_Pri_Part_Size_In_MB( EXTENDED );

   maximum_possible_percentage = Convert_To_Percentage(
      maximum_partition_size_in_MB, pDrive->disk_size_mb );

   if ( arg[0].extra_value == 100 ) {
      /* Set limit on percentage. */
      if ( arg[0].value > 100 ) {
         arg[0].value = 100;
      }
      if ( arg[0].value > maximum_possible_percentage ) {
         arg[0].value = maximum_possible_percentage;
      }

      /* Determine partition size. */
      if ( maximum_possible_percentage > 0 ) {
         arg[0].value = ( arg[0].value * maximum_partition_size_in_MB ) /
                        maximum_possible_percentage;
      }
      else {
         arg[0].value = 0;
      }
   }

   error_code = Create_Primary_Partition( 5, arg[0].value );

   if ( error_code == 99 ) {
      /* NLS:Error creating extended partition.*/
      con_print( svarlang_str( 8, 5 ) );
      halt( 9 );
   }

   Shift_Command_Line_Options( 1 );
}

/* /LOG and /LOGO command line options */
void Command_Line_Create_Logical_DOS_Drive( void )
{
   unsigned long maximum_possible_percentage;
   unsigned long maximum_partition_size_in_MB;
   int option_count = 1;
   int error_code;

   Partition_Table *pDrive = &part_table[flags.drive_number - 0x80];

   if ( arg[0].value == 0 ) {
      /* NLS:Invalid partition size specifed. */
      con_print( svarlang_str( 8, 3 ) );
      halt( 9 );
   }

   if ( !pDrive->ext_usable ) {
      /* NLS:No usable extended partition found. */
      con_print( svarlang_str( 8, 7 ) );
      halt( 9 );
   }

   Determine_Free_Space();

   if ( Determine_Drive_Letters() >= _T('Z') ) {
      /* maximum number of Logical DOS Drives installed */
      con_puts( _T("") );
      con_print( svarlang_str( 10, 71 ) );
      con_puts( _T("") );
      halt( 9 );
   }

   maximum_partition_size_in_MB = Max_Log_Part_Size_In_MB();

   maximum_possible_percentage = Convert_To_Percentage(
      maximum_partition_size_in_MB, pDrive->ext_size_mb );

   if ( arg[0].extra_value == 100 ) {
      /* Set limit on percentage. */
      if ( arg[0].value > 100 ) {
         arg[0].value = 100;
      }
      if ( arg[0].value > maximum_possible_percentage ) {
         arg[0].value = maximum_possible_percentage;
      }

      /* Determine partition size. */
      if ( maximum_possible_percentage > 0 ) {
         arg[0].value = ( arg[0].value * maximum_partition_size_in_MB ) /
                        maximum_possible_percentage;
      }
      else {
         arg[0].value = 0;
      }
   }

   if ( 0 != _tcscmp( arg[1].choice, _T("SPEC") ) ) {
      /* If no special partition type is defined. */

      error_code = Create_Logical_Drive(
         Partition_Type_To_Create( arg[0].value, 0 ), arg[0].value );
   }
   else {
      /* If a special partition type is defined. */
      option_count = 2;

      error_code = Create_Logical_Drive( (int)arg[1].value, arg[0].value );
   }

   if ( error_code == 99 ) {
      /* NLS:Error creating logical drive. */
      con_print( svarlang_str( 8, 8 ) );
      halt( 9 );
   }

   Shift_Command_Line_Options( option_count );
}

/* /PRI and /PRIO command line options */
void Command_Line_Create_Primary_Partition( void )
{
   unsigned long maximum_possible_percentage;
   unsigned long maximum_partition_size_in_MB;
   int option_count = 1;
   int part_no = 0;
   int part_type = 0;

   Partition_Table *pDrive = &part_table[flags.drive_number - 0x80];

   if ( arg[0].value == 0 ) {
      /* NLS:Invalid partition size specified. */
      con_print( svarlang_str( 8, 3 ) );
      halt( 9 );
   }

   Determine_Free_Space();

   maximum_partition_size_in_MB = Max_Pri_Part_Size_In_MB( PRIMARY );

   maximum_possible_percentage = Convert_To_Percentage(
      maximum_partition_size_in_MB, pDrive->disk_size_mb );

   if ( arg[0].extra_value == 100 ) {
      /* Set limit on percentage. */
      if ( arg[0].value > 100 ) {
         arg[0].value = 100;
      }
      if ( arg[0].value > maximum_possible_percentage ) {
         arg[0].value = maximum_possible_percentage;
      }

      /* Determine partition size. */
      if ( maximum_possible_percentage > 0 ) {
         arg[0].value = ( arg[0].value * maximum_partition_size_in_MB ) /
                        maximum_possible_percentage;
      }
      else {
         arg[0].value = 0;
      }
   }

   if ( 0 != _tcscmp( arg[1].choice, _T("SPEC") ) ) {
      part_type = Partition_Type_To_Create( arg[0].value, 0 );
   }
   else {
      /* If a special partition type is defined. */
      option_count = 2;
      part_type = (int)arg[1].value;
   }

   part_no = Create_Primary_Partition( part_type, arg[0].value );
   if ( part_no == 99 ) {
      /* NLS:Error creating primary partition. */
      con_print( svarlang_str( 8, 10 ) );
      halt( 9 );
   }

   Shift_Command_Line_Options( option_count );
}

/* /DELETE command line option */
void Command_Line_Delete( void )
{
   Partition_Table *pDrive = &part_table[flags.drive_number - 0x80];
   int error_code = 0;
   int part_num;

   /* Delete the primary partition */
   if ( 0 == _tcscmp( arg[1].choice, _T("PRI") ) ) {
      if ( arg[1].value != 0 ) /* specified what to delete */
      {
         if ( arg[1].value < 1 || arg[1].value > 4 ) {
            /* NLS:primary partition # (%ld) must be 1..4. */
            con_printf( svarlang_str( 8, 11 ), (long)arg[1].value );
            halt( 9 );
         }

         error_code = Delete_Primary_Partition( (int)( arg[1].value - 1 ) );
      }
      else { /* no number given, delete 'the' partition */
         int index, found = -1, count;

         for ( count = 0, index = 0; index < 4; index++ ) {
            if ( IsRecognizedFatPartition( pDrive->pri_part[index].num_type ) ) {
               count++;
               found = index;
            }
         }
         if ( count == 0 ) {
            /* NLS:No partition to delete found. */
            con_print( svarlang_str( 8, 12 ) ); /* but continue */
         }
         else if ( count > 1 ) {
            /* NLS:%d primary partitions found, you must specify number... */
            con_printf( svarlang_str( 8, 13 ), count );
            halt( 9 );
         }
         else {
            error_code = Delete_Primary_Partition( found );
         }
      }
      if ( error_code == 99 ) {
         /* NLS:Error deleting primary partition. */
         con_printf( svarlang_str( 8, 14 ) );
         halt( 9 );
      }
   } /* end PRI */

   /* Delete the extended partition */
   else if ( 0 == _tcscmp( arg[1].choice, _T("EXT") ) ) {
      error_code = Delete_Extended_Partition();

      if ( error_code == 99 ) {
         /* NLS:Error deleting extended partition. */
         con_print( svarlang_str( 8, 15 ) );
         halt( 9 );
      }
   }

   /* Delete a Logical DOS Drive */
   else if ( 0 == _tcscmp( arg[1].choice, _T("LOG") ) ) {
      if ( ( arg[1].value >= 1 ) && ( arg[1].value <= MAX_LOGICAL_DRIVES ) &&
           ( ( part_num = Nth_Log_Part_Defined( pDrive, arg[1].value - 1  ) ) < MAX_LOGICAL_DRIVES ) ) {
      
         if ( pDrive->ptr_ext_part && !pDrive->ext_usable ) {
            /* NLS:No usable extended partition found. */
            con_print( svarlang_str( 8, 7 ) );
            halt( 9 );
         }         
         error_code = Delete_Logical_Drive( part_num );
      }
      else {
         /* NLS:Logical drive number %d is out of range. */
         con_printf( svarlang_str( 8, 16 ), arg[1].value );
         halt( 9 );
      }
   }

   /* Delete the partition by the number of the partition */
   else if ( 0 == _tcscmp( arg[1].choice, _T("NUM") ) ) {
      if ( ( arg[1].value >= 1 ) && ( arg[1].value <= 4 ) ) {
         error_code = Delete_Primary_Partition( (int)( arg[1].value - 1 ) );
      }
      else if ( ( arg[1].value >= 5 ) && ( arg[1].value <= 28 ) &&
              ( ( part_num = Nth_Log_Part_Defined( pDrive, arg[1].value - 5  ) ) < MAX_LOGICAL_DRIVES ) ) {

         if ( pDrive->ptr_ext_part && !pDrive->ext_usable ) {
            /* NLS:No usable extended partition found. */
            con_print( svarlang_str( 8, 7 ) );
            halt( 9 );
         } 
         error_code = Delete_Logical_Drive( (int)( part_num ) );
      }
      else {
         /* NLS:Partition number is out of range. */
         con_print( svarlang_str( 8, 17 ) );
         halt( 9 );
      }
   }

   else {
      /* NLS:Invalid delete argument. */
      con_print( svarlang_str( 8, 18 ) );
      halt( 9 );
   }

   if ( error_code != 0 ) {
      /* NLS:Error deleting logical drive. */
      con_print( svarlang_str( 8, 19 ) );
      halt( 9 );
   }

   Shift_Command_Line_Options( 2 );
}

/* /INFO command line option */
void Command_Line_Info( void )
{
   int option_count = 1;

   if ( 0 == _tcscmp( arg[1].choice, _T("TECH") ) ) {
      /* for backwards compatibility. full info is always shown */
      option_count = 2;

      /*flags.extended_options_flag = TRUE;*/
   }

   Display_CL_Partition_Table();

   Shift_Command_Line_Options( option_count );
}

/* /MODIFY command line option */
void Command_Line_Modify( void )
{

   if ( ( arg[0].extra_value == 0 ) || ( arg[0].extra_value > 255 ) ) {
      /* NLS:New partition type is out of range. */
      con_print( svarlang_str( 8, 20 ) );
      halt( 9 );
   }

   if ( Modify_Partition_Type( (int)( arg[0].value - 1 ),
                               arg[0].extra_value ) != 0 ) {
      /* NLS:Error modifying partition type. */
      con_print( svarlang_str( 8, 21 ) );
      halt( 9 );
   }

   Shift_Command_Line_Options( 1 );
}

/* /MOVE command line option */
void Command_Line_Move( void )
{
   if ( ( arg[0].value < 1 ) || ( arg[0].value > 4 ) ) {
      /* NLS:Source partition number is out of range. */
      con_print( svarlang_str( 8, 22 ) );
      halt( 9 );
   }

   if ( ( arg[0].extra_value < 1 ) || ( arg[0].extra_value > 4 ) ) {
      /* NLS:Destination partition number is out of range. */
      con_print( svarlang_str( 8, 23 ) );
      halt( 9 );
   }

   if ( Primary_Partition_Slot_Transfer( MOVE, (int)arg[0].value,
                                         arg[0].extra_value ) != 0 ) {
      /* NLS:Error moving partition slot. */
      con_print( svarlang_str( 8, 24 ) );
      halt( 9 );
   }

   Shift_Command_Line_Options( 1 );
}

/* /SETFLAG command line option */
void Command_Line_Set_Flag( void )
{
   if ( ( arg[0].value < 1 ) || ( arg[0].value > 64 ) ) {
      /* NLS:Invalid flag number. */
      con_print( svarlang_str( 8, 25 ) );
      halt( 9 );
   }

   if ( arg[0].extra_value == 0 ) {
      arg[0].extra_value = 1;
   }

   if ( ( arg[0].extra_value < 1 ) || ( arg[0].extra_value > 64 ) ) {
      /* NLS:Flag value is out of range. */
      con_print( svarlang_str( 8, 26 ) );
      halt( 9 );
   }

   if ( Set_Flag( (int)arg[0].value, arg[0].extra_value ) != 0 ) {
      /* NLS:Error setting flag. */
      con_print( svarlang_str( 8, 27 ) );
      halt( 9 );
   }

   /* NLS:Flag %d has been set to %d. */
   con_printf( svarlang_str( 8, 28 ), arg[0].value, arg[0].extra_value );

   Shift_Command_Line_Options( 1 );
}

/* /STATUS command line option */
void Command_Line_Status( void )
{
   /* NLS:Fixed Disk Drive Status */
   Display_All_Drives(1, svarlang_str( 8, 29 ), 0 );

   Shift_Command_Line_Options( 1 );
}

/* /SWAP command line option */
void Command_Line_Swap( void )
{
   if ( ( arg[0].value < 1 ) || ( arg[0].value > 4 ) ) {
      /* NLS:Source partition number is out of range. */
      con_print( svarlang_str( 8, 22 ) );
      halt( 9 );
   }

   if ( ( arg[0].extra_value < 1 ) || ( arg[0].extra_value > 4 ) ) {
      /* NLS:Destination partition number is out of range. */
      con_print( svarlang_str( 8, 23 ) );
      halt( 9 );
   }

   if ( Primary_Partition_Slot_Transfer( SWAP, (int)arg[0].value,
                                         arg[0].extra_value ) != 0 ) {
      /* NLS:Error swapping partitions. */
      con_print( svarlang_str( 8, 30 ) );
      halt( 9 );
   }

   Shift_Command_Line_Options( 1 );
}

/* /TESTFLAG command line option */
void Command_Line_Test_Flag( void )
{
   int flag;

   flag = Test_Flag( (int)arg[0].value );

   if ( arg[0].extra_value > 0 ) {
      /* If testing for a particular value, return a true or false answer. */
      /* The error level returned will be 20 for false and 21 for true.    */

      if ( flag == arg[0].extra_value ) {
         /* NLS:Flag %d is set to %d. */
         con_printf( svarlang_str( 8, 31 ), arg[0].value,
                     arg[0].extra_value );
         halt( 21 );
      }
      else {
         /* NLS:Flag %d is not set to %d. */
         con_printf( svarlang_str( 8, 32 ), arg[0].value,
                     arg[0].extra_value );
         halt( 20 );
      }
   }
   else {
      /* If not testing the flag for a particular value, return the value */
      /* the flag is set to.  The error level returned will be the value  */
      /* of the flag + 30.                                                */

      con_printf( svarlang_str( 8, 31 ), arg[0].value, flag );
      halt( ( 30 + flag ) );
   }
}


static int check_comma(TCHAR **argptr, BOOL isHalt) {
    TCHAR *ptr = *argptr;
    if (*ptr == 0) {
        if (isHalt) {
            /* NLS:<%s> ',' expected; terminated */
            con_printf(svarlang_str(8, 38), ptr);
            halt(9);
        }
        /* done */
        return 0;
    }

    if (*ptr != _T(',')) {
        /* NLS:<%s> ',' expected; terminated */
        con_printf(svarlang_str(8, 38), ptr);
        halt(9);
    }
    *argptr = ++ptr;

    return -1;
}

/* Get the command line options */
int Get_Options(TCHAR *argv[], int argc) {
    int number_of_options = 0;
    flags.drive_number = 0x80;

    argc--, argv++; /* absorb program name */

    for (int i = 0; i < 20; i++) {
        _tcscpy(arg[i].choice, _T(""));
        _tcscpy(arg[i].filename, _T(""));
        arg[i].value = 0;
        arg[i].extra_value = 0;
        arg[i].additional_value = 0;
    }

    for (; argc > 0; number_of_options++, argv++, argc--) {
        if (number_of_options >= 20) {
            break;
        }

        /* Limit the possible number of options to 20 to prevent an overflow of */
        /* the arg[] structure.                                                 */

        TCHAR *argptr = *argv;

        if (1 == _tcslen(argptr)) {
            if (!_istdigit(*argptr)) {
                /* NLS:<%s> should be a digit; terminated */
                con_printf(svarlang_str(8, 34), argptr);
                halt(9);
            }

            if (flags.using_default_drive_number) {
                flags.drive_number = (argptr[0] - _T('0')) + 127;
                flags.using_default_drive_number = FALSE;
            } else if (flags.drive_number != (argptr[0] - _T('0')) + 127) {
                /* NLS:more than one drive specified; terminated */
                con_print(svarlang_str(8, 35));
                halt(9);
            }
            number_of_options--;
            continue;
        }

        if (*argptr != _T('-') && *argptr != _T('/')) {
            /* NLS:<%s> should start with '-' or '/'; terminated */
            con_printf(svarlang_str(8, 36), argptr);
            halt(9);
        }

        argptr++; /* skip -/ */

        /* parse commandline
                      /ARGUMENT:number,number   */
        for (int i = 0;; argptr++, i++) {
            if (!_istalpha(*argptr) && *argptr != _T('_')) {
                break;
            }

            if (i < sizeof(arg[0].choice) / sizeof(TCHAR) - 1) {
                arg[number_of_options].choice[i] = _totupper(*argptr);
                arg[number_of_options].choice[i + 1] = 0;
            }
        }

        if (*argptr == 0) {
            /* done */
            continue;
        }

        if (*argptr != _T(':')) {
            /* NLS:<%s> ':' expected; terminated */
            con_printf(svarlang_str(8, 37), argptr);
            halt(9);
        }

        argptr++;

        if (_istdigit(*argptr)) {
            arg[number_of_options].value = _tstol(argptr);

            while (_istdigit(*argptr)) {
                /* skip number */
                argptr++;
            }

            if (!check_comma(&argptr, FALSE)) {
                continue;
            }
            arg[number_of_options].extra_value = (int) _tstol(argptr);

            while (_istdigit(*argptr)) {
                /* skip number */
                argptr++;
            }
        } else if (!_tcsicmp(argptr, _T("MAX"))) {
            arg[number_of_options].value = 100;
            arg[number_of_options].extra_value = 100;
            argptr += 3;
        } else {
            if (0 == _tcscmp(arg[number_of_options].choice, _T("IMAGE"))) {
                /*
                 * Парсим параметры для подключения образа диска из файла
                 */
                for (int i = 0;; argptr++, i++) {
                    if ( *argptr == _T('\0') || (!_istalpha(*argptr) && *argptr == _T(',')) ) {
                        break;
                    }
                    if (i < sizeof(arg[0].filename) / sizeof(TCHAR) - 1) {
                        arg[number_of_options].filename[i] = *argptr;
                        arg[number_of_options].filename[i + 1] = 0;
                    }
                }
                // cylinders
                check_comma(&argptr, TRUE);
                arg[number_of_options].value =_tstol(argptr);
                while (_istdigit(*argptr)) {
                    argptr++;
                }
                // heads
                check_comma(&argptr, TRUE);
                arg[number_of_options].extra_value = (int) _tstol(argptr);
                while (_istdigit(*argptr)) {
                    argptr++;
                }
                // sectors
                check_comma(&argptr, TRUE);
                arg[number_of_options].additional_value = (int) _tstol(argptr);
                while (_istdigit(*argptr)) {
                    argptr++;
                }
            }
        }

        if (*argptr != 0) /* done */
        {
            svarlang_str(8, 39);
            con_printf(svarlang_str(8, 39), argptr);
            halt(9);
        }
    }

    /* check to make sure the drive is a legitimate number */
    if ((flags.drive_number < 0x80) ||
        (flags.drive_number > flags.maximum_drive_number)) {
        /* NLS:Invalid drive designation. */
        con_print(svarlang_str(8, 40));
        halt(5);
    }

    return (number_of_options);
}


void Shift_Command_Line_Options( int number_of_places )
{
   for ( int index = 0; index < 20 - number_of_places; index++ ) {
      memcpy(&arg[index], &arg[index + number_of_places], sizeof( Arg_Structure));
      // _tcscpy( arg[index].choice, arg[index + number_of_places].choice );
      // arg[index].value = arg[index + number_of_places].value;
      // arg[index].extra_value = arg[index + number_of_places].extra_value;
   }

   number_of_command_line_options -= number_of_places;
}
