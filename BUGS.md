## 1. main.c
```c++
main() {
    ....
   if ( ( argv[1][1] == '?' ) && ( ( argc == 2 ) || ( argc == 3 ) ) ) {
      flags.do_not_pause_help_information = FALSE;
      flags.screen_color = 7;

      if ( ( argv[2][1] == 'N' ) || ( argv[2][1] == 'n' ) ) {
         flags.do_not_pause_help_information = TRUE;
         Shift_Command_Line_Options( 1 );
      }

      Display_Help_Screen();
      exit( 0 );
   }
```
`argv[1] - the argument may be NULL` (crash)  
`argv[2] - the argument may be NULL` (crash)  


## fdiskio.c
```c++
void Process_Fdiskini_File(void) {
...
user_defined_chs_settings[drive].total_heads = _tstol(conversion_buffer);
...
user_defined_chs_settings[drive].total_cylinders = _tstol(conversion_buffer);
...
}
```
In the program, total_heads and total_cylinders are used as the index of the last head 
and cylinder, but here the fact is not taken into account that head and cylinder numbering 
starts from zero.
