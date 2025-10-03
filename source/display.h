/* contains UI calls that required by command-line operations */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <tchar.h>


void Display_Information( void );
void Dump_Partition_Information( void );
void Display_CL_Partition_Table( void );
void Print_UL( unsigned long number );
void Print_UL_B( unsigned long number );
void Print_Centered( int y, const TCHAR *text, int style );
void PrintProductName(TCHAR product[]);
void Display_All_Drives(int y, const TCHAR *title, int bold);
void Pause( void );

const TCHAR *part_type_descr( int id );
const TCHAR *part_type_descr_short( int id );

#endif
