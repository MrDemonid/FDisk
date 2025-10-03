#ifndef KBDINPUT_H
#define KBDINPUT_H

#include <tchar.h>
#include <stdint.h>


/* Definitions for the input routine */
enum kbdinput_type {
   YN = 0,
   NUM = 1,
   NUMP = 2,
   ESC = 3,
   ESCR = 4,
   ESCE = 5,
   ESCC = 6,
   RCHAR = 7,
   NONE = 8,
   CHARNUM = 9,
   NUMCHAR = 10,
   NUMYN = 11
};

unsigned long Input( int size_of_field, int x_position, int y_position,
                     enum kbdinput_type type, unsigned long min_range,
                     unsigned long max_range, int return_message,
                     long default_value,
                     unsigned long maximum_possible_percentage,
                     TCHAR optional_char_1, TCHAR optional_char_2 );

#endif /* KBDINPUT_H */
