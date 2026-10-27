#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
static void fatal_error(const char *s, const char *p) { fprintf(stderr, s, p); exit(1); }
#include "../extra/Mysql_speed_common.h"
int main(void) {
    const char *names[] = {"none", "GPH_M", "M_MM", "PERC_PART", "PA_HPA", "PART_PERC", "K_C"};
    const double expected[] = {12.0, 12.0/9.82, 12000, 0.12, 0.12, 1200, 12.0-273.16};
    const double values[] = {0, -0.0, 1e-30, -1e30, 123.456789, 1.0};
    struct mysql_speed_row row = {0}; struct mysql_speed_mapping m = {0}; char ref[256]; size_t i;
    for (i=0; i<7; i++) if (mysql_speed_convert(12.0,mysql_speed_conversion(names[i])) != expected[i]) return 1;
    m.center=7; strcpy(m.name,"TMP"); strcpy(m.level,"2 m"); strcpy(m.mapped_level,"None");
    if (mysql_speed_mapping_find(&m,7,"TMP","2 m") != &m || mysql_speed_mapping_find(&m,7,"TMP","850 mb")) return 2;
    row.cap=2; row.text=calloc(1,row.cap); if (!row.text) return 3;
    for (i=0; i<sizeof(values)/sizeof(values[0]); i++) {
        snprintf(ref,sizeof(ref),",%lg",values[i]); mysql_speed_append_value(&row,values[i]);
        if (strcmp(row.text + row.len - strlen(ref),ref)) return 4;
    }
    mysql_speed_append_null(&row); if (strcmp(row.text+row.len-5,",NULL")) return 5;
    free(row.text);
    {
        struct mysql_speed_row rows[] = {{"abc", 3, 4, 1}, {"ignored", 7, 8, 0}, {"de", 2, 3, 1}};
        struct mysql_speed_stream stream = {0};
        void *ptr; char out[32]; int n, total = 0;
        stream.rows = rows; stream.count = 3;
        if (mysql_speed_init(&ptr, "test", &stream)) return 6;
        while ((n = mysql_speed_read(ptr, out + total, 2)) > 0) total += n;
        out[total] = 0;
        mysql_speed_end(ptr);
        if (n < 0 || strcmp(out, "abc\nde\n")) return 7;
    }
    return 0;
}
