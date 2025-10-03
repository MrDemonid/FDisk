/*
// Program:  Free FDISK
// Module:   HELPSCR.C
// Module Description: User Interface Code Module
// Written By: Brian E. Reifsnyder
// Version:  1.3.1
// Copyright: 1998-2008 under the terms of the GNU GPL, Version 2
//
// Modifications (Windows adaptation, 2025):
//   © Andrey Hlus
/*

/////////////////////////////////////////////////////////////////////////////
//  INCLUDES
/////////////////////////////////////////////////////////////////////////////
*/

#include <tchar.h>
#include <string.h>

#include "ansicon.h"
#include "display.h"
#include "helpscr.h"
#include "main.h"
#include "printf.h"
#include "svarlang/svarlang.h"

/*
/////////////////////////////////////////////////////////////////////////////
//  FUNCTIONS
/////////////////////////////////////////////////////////////////////////////
*/

/* Display Help Screens */
void Display_Help_Screen( void )
{
   TCHAR version[40];
   TCHAR name[40];
   unsigned char i;
   unsigned char screenh = con_get_height();

   flags.do_not_pause_help_information = FALSE;

   _tcscpy( name, _T(FD_NAME) );

   _tcscpy( version, _T(" V") );
   _tcscat( version, _T(VERSION) );

   con_printf( _T("%-30s                   %30s\n"), name, version );

   /* dump the entire help on screen */
   unsigned char linestopause = screenh - 1; /* number of lines before screen is full */
   for ( i = 0; i < 250; i++ ) {
      const TCHAR *s = A2T(svarlang_strid( i ));
      if ( *s == 0 ) {
         continue;
      }
      if ( i == 200 ) { /* special case: COPYLEFT needs to be inserted */
         con_printf( s, _T(COPYLEFT) );
         con_putc( _T('\n') );
      }
      else {
         con_puts( s );
      }

      /* is it time for a pause? */
      if ( ( flags.do_not_pause_help_information == FALSE ) &&
           ( --linestopause <= 2 ) ) {
         linestopause = screenh;
         Pause();
      }
   }
}
