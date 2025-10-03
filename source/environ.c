/*
// Module:   ENVIRON.C
// Module Description: New functionality for partition handling
//
// Originally Written By: Brian E. Reifsnyder
// Based on code (c) 1998-2008, under the terms of the GNU GPL, Version 2
//
// Modified and extended by: Andrey Hlus
// Copyright: 2025
//
// License: This file is distributed under the terms of the GNU GPL, Version 2
*/

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "environ.h"

#include "main.h"
#include "fdiskio.h"
#include "codepage/codepage.h"


/* Get Environment Settings */
int Get_Environment_Settings( TCHAR *environment[] )
{
   TCHAR command_buffer[255];
   TCHAR setting_buffer[255];

   int character_index = 0;
   int line_index = 0;
   int number;

   if ( environment[0][0] == _T('\0') ) {
      return ( 1 );
   }

   // fix: end of environment must be environment[line_index] != NULL
   //
   while ( ( environment[line_index] != NULL ) && ( line_index < 64 ) ) {
      /* Clear the command_buffer and setting_buffer */
      TCHAR *env = environment[line_index];
      character_index = 0;

      do {
         command_buffer[character_index] = 0x00;
         setting_buffer[character_index] = 0x00;

         character_index++;
      } while ( character_index < 255 );

      /* Extract the command and setting from the line_buffer */

      /* Find the command */
      character_index = 0;
      int sub_buffer_index = 0;
      int done_looking = FALSE;

      do {
         if ( env[character_index] != _T('=') ) {
            command_buffer[sub_buffer_index] = env[character_index];
         }

         if ( env[character_index] == _T('=') ) {
            done_looking = TRUE;
         }

         sub_buffer_index++;
         character_index++;
         if ( character_index >= 255 ) {
            done_looking = TRUE;
         }
      } while ( done_looking == FALSE );

      /* Find the setting */
      sub_buffer_index = 0;
      done_looking = FALSE;

      do {
         if ( ( env[character_index] == _T('\0') ) ||
              ( env[character_index] == _T(' ')) ) {
            done_looking = TRUE;
         }

         if ( ( env[character_index] != _T('\0') ) &&
              ( env[character_index] != _T('=') ) ) {
            setting_buffer[sub_buffer_index] = env[character_index];

            setting_buffer[sub_buffer_index] = _totupper( setting_buffer[sub_buffer_index] );

            sub_buffer_index++;
         }

         character_index++;
         if ( character_index >= 255 ) {
            done_looking = TRUE;
         }
      } while ( done_looking == FALSE );

      /* Adjust for the possibility of TRUE or FALSE in the environment. */
      if ( 0 == _tcscmp( setting_buffer, _T("TRUE") ) ) {
         _tcscpy( setting_buffer, _T("ON") );
      }
      if ( 0 == _tcscmp( setting_buffer, _T("FALSE") ) ) {
         _tcscpy( setting_buffer, _T("OFF") );
      }

      /* Process the command found in the line buffer */

      /* Align partitions to 4K */
      if ( 0 == _tcscmp( command_buffer, _T("FFD_ALIGN_4K") ) ) {
         bool_string_to_int( &flags.align_4k, setting_buffer );
      }

      /* Check for the ALLOW_4GB_FAT16 statement */
      if ( 0 == _tcscmp( command_buffer, _T("FFD_ALLOW_4GB_FAT16") ) ) {
         bool_string_to_int( &flags.allow_4gb_fat16, setting_buffer );
      }

      /* Check for the ALLOW_ABORT statement */
      if ( 0 == _tcscmp( command_buffer, _T("FFD_ALLOW_ABORT") ) ) {
         bool_string_to_int( &flags.allow_abort, setting_buffer );
      }

      /* Check for the AMBR statement */
      if ( 0 == _tcscmp( command_buffer, _T("FFD_AMBR") ) ) {
         bool_string_to_int( &flags.use_ambr, setting_buffer );
      }

      /* Check for the CHECKEXTRA statement */
      if ( 0 == _tcscmp( command_buffer, _T("FFD_CHECKEXTRA") ) ) {
         bool_string_to_int( &flags.check_for_extra_cylinder,
                             setting_buffer );
      }

      /* Check for the COLORS statement */
      if ( 0 == _tcscmp( command_buffer, _T("FFD_COLORS") ) ) {
         number = _tstoi( setting_buffer );

         if ( ( number >= 0 ) && ( number <= 127 ) ) {
            flags.screen_color = number;
         }
      }

      /* Check for the DEL_ND_LOG statement */
      if ( 0 == _tcscmp( command_buffer, _T("FFD_DEL_ND_LOG") ) ) {
         bool_string_to_int( &flags.del_non_dos_log_drives, setting_buffer );
      }

      /* Check for drive letter ordering */
      if ( 0 == _tcscmp( command_buffer, _T("FFD_DLA") ) ) {
         number = _tstoi( setting_buffer );

         if ( ( number >= 0 ) && ( number <= 2 ) ) {
            flags.dla = number;
         }
      }

      /* Check for the FLAG_SECTOR statement */
      if ( 0 == _tcscmp( command_buffer, _T("FFD_FLAG_SECTOR") ) ) {
         number = _tstoi( setting_buffer );
         if ( number == 0 ) {
            flags.flag_sector = 0;
         }
         if ( ( number >= 2 ) && ( number <= 64 ) ) {
            flags.flag_sector = number;
         }
         if ( number == 256 ) {
            flags.flag_sector = part_table[0].total_sect;
         }
      }

      /* Check for the LBA_MARKER statement */
      if ( 0 == _tcscmp( command_buffer, _T("FFD_LBA_MARKER") ) ) {
         bool_string_to_int( &flags.lba_marker, setting_buffer );
      }

      /* Check for the SET_ANY_ACT statement */
      if ( 0 == _tcscmp( command_buffer, _T("FFD_SET_ANY_ACT") ) ) {
         bool_string_to_int( &flags.set_any_pri_part_active, setting_buffer );
      }

      /* Check for the VERSION statement */
      if ( 0 == _tcscmp( command_buffer, _T("FFD_VERSION") ) ) {
         if ( 0 == _tcscmp( setting_buffer, _T("4") ) ) {
            flags.version = COMP_FOUR;
         }
         if ( 0 == _tcscmp( setting_buffer, _T("5") ) ) {
            flags.version = COMP_FIVE;
         }
         if ( 0 == _tcscmp( setting_buffer, _T("6") ) ) {
            flags.version = COMP_SIX;
         }
         if ( 0 == _tcscmp( setting_buffer, _T("W95") ) ) {
            flags.version = COMP_W95;
         }
         if ( 0 == _tcscmp( setting_buffer, _T("W95B") ) ) {
            flags.version = COMP_W95B;
         }
         if ( 0 == _tcscmp( setting_buffer, _T("W98") ) ) {
            flags.version = COMP_W98;
         }
         if ( 0 == _tcscmp( setting_buffer, _T("FD") ) ) {
            flags.version = COMP_FD;
         }
      }

      /* Check for the XO statement */
      if ( 0 == _tcscmp( command_buffer, _T("FFD_XO") ) ) {
         bool_string_to_int( &flags.extended_options_flag, setting_buffer );
      }

      /* Check for the LANG statement */
      if ( 0 == _tcscmp( command_buffer, _T("LANG") ) ) {
         setting_buffer[2] = _T('\0');
         flags.lang_type = GetLanguageType( setting_buffer );
      }

      line_index++;
   }

   return ( 0 );
}

