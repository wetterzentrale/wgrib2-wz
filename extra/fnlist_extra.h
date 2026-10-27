/**
 * Header file with internal function prototypes for the wgrib2 extra subdirectory.
 * 
 * Alyson Stahl 2/28/2025
 */

int f_mysql(ARG4);
int f_mysql_dump(ARG6);
int f_mysql_speed(ARG6);
int f_mysql_speed_ens(ARG6);

/* used by the mysql functions; declared in src/fnlist.h */
int f_lev(ARG0);