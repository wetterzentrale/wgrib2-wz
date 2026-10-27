/******************************************************************************************
 Copyright (C) 2008 Niklas Sondell, Storm Weather Center
 This file is part of wgrib2 and could be distributed under terms of the GNU General Public License
 v1.1 Jerry Stueve: tmpnam -> mkstemp
 1/2011 WNE change new_GDS to GDS_change_no
 4/2011 WNE change char temp_pathname[L_tmpnam] to char temp_pathname[STRING_SIZE];
 3/2017 WNE added MAX_SQL_INSERT: in response to overflow because of too many variables*levels
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grb2.h"
#include "wgrib2.h"
#include "fnlist_extra.h"

#ifdef USE_MYSQL

#include <mysql.h>
#include "Mysql_connect.h"
#include "Mysql_speed_common.h"


extern int decode, GDS_change_no, new_GDS, latlon;
extern double *lat, *lon;

//maximum number of rows to insert (similiar to number of gridpoints)
#define MAX_NXNY   2000000
#define MAX_SQL_INSERT 3000

/*
 * HEADER:100:mysql_speed:output:6:H=[host] U=[user] D=[db] T=[table] W=[western_lons:0|1] PV=[remove unlikely:0|1]
 */

int f_mysql_speed(ARG6) {

    char sql[MAX_SQL_INSERT];
    char server[100];
    char user[100];
    char database[100];
    char table[100];
    MYSQL_RES *res;
    MYSQL_ROW row;

    char name[100], desc[100], unit[100], level_buf[100];
    int year, month, day, hour, minute, second;
	
    char vt[20],rt[20];
//    unsigned char *p;
   		
    int i,j;
    double longitude,latitude,value;

    char new_level[50];
    char new_name[50];
    char conv[50];
    char precision[10];
    char last;
    char param[50];
    int ctr;
    int level;
    unsigned int load_local_infile;

    struct local_struct {
        MYSQL *conn;
        unsigned int npts;
        struct mysql_speed_row *rows;
        struct mysql_speed_stream stream;
        struct mysql_speed_mapping *mappings;
        int has_rt;
        char *params;
        unsigned int isset;
        unsigned int wlon;
        unsigned int remove_unlikely;
        char runtime[20], validtime[20];
	int last_GDS_change_no;
    };
    struct local_struct *save;

    strcpy(server,arg1);
    strcpy(user,arg2);
    strcpy(database,arg3);
    strcpy(table,arg4);

    /* initialization phase */

    if (mode == -1) {
        decode = latlon = 1;
        
	*local = save = (struct local_struct *) malloc( sizeof(struct local_struct));
	if (save == NULL) fatal_error("mysql_speed memory allocation ","");

        save->rows = NULL; save->npts = 0; save->mappings = NULL; save->has_rt = 1;
	save->last_GDS_change_no = 0;
	save->conn = mysql_init(NULL);
	save->params = (char *)  malloc(MAX_SQL_INSERT*sizeof(char));
	sprintf(save->params,"%s","");
	save->isset = 0;
	save->runtime[0] = 0;
	save->validtime[0] = 0;
	load_local_infile = 1;		// 1 = LOAD LOCAL INFILE,  0 = do not LOAD LOCAL INFILE

	if (sscanf(arg5,"%d", &save->wlon) != 1) {
            fatal_error("Argument 5, use western longitudes, has to be 0 or 1, error parsing %s", arg5);
        }
        if (sscanf(arg6,"%d", &save->remove_unlikely) != 1) {
            fatal_error("Argument 6, remove unlikely values, has to be 0 or 1, error parsing %s", arg6);
        }
	
	/* Set options for database */
	mysql_options(save->conn,MYSQL_OPT_LOCAL_INFILE, (char *) &load_local_infile);

	/* Connect to database */
	if (!wgrib2_mysql_connect(save->conn, server, user, database)) {
	   fatal_error("f_mysql_speed: could not connect to %s", mysql_error(save->conn));
	} 
        /* Inspect the result metadata without interpolating a column name. */
        snprintf(sql, sizeof(sql), "SELECT * FROM `%s` LIMIT 0", table);
        if (mysql_query(save->conn, sql)) fatal_error("f_mysql_speed: table lookup failed: %s", mysql_error(save->conn));
        res = mysql_store_result(save->conn);
        if (!res) fatal_error("f_mysql_speed: table metadata failed: %s", mysql_error(save->conn));
        save->has_rt = 0;
        for (i = 0; i < mysql_num_fields(res); i++) if (strcmp(mysql_fetch_fields(res)[i].name, "rt") == 0) save->has_rt = 1;
        mysql_free_result(res);
	return 0;
    }

    /* cleanup phase */

    if (mode == -2) {
        int rc;
        save = (struct local_struct *) *local;
        save->stream.rows = save->rows;
        save->stream.count = save->npts;
        mysql_set_local_infile_handler(save->conn, mysql_speed_init, mysql_speed_read, mysql_speed_end, mysql_speed_error, &save->stream);
        snprintf(sql, sizeof(sql), "LOAD DATA LOCAL INFILE 'wgrib2-memory' INTO TABLE %s FIELDS TERMINATED BY ',' ENCLOSED BY '\"' LINES TERMINATED BY '\\n' (%s%s)", table, (save->has_rt ? "rt,vt,lat,lon" : "vt,lat,lon"), save->params);
        rc = mysql_query(save->conn, sql);
        if (rc) fatal_error("f_mysql_speed: connection error %s", mysql_error(save->conn));
        for (j = 0; j < save->npts; j++) free(save->rows[j].text);
        mysql_speed_mapping_free(save->mappings);
        free(save->rows); free(save->params); mysql_close(save->conn); free(save); *local = NULL;
        return 0;
    }

    /* processing phase */

    save = (struct local_struct *) *local;
  	
//    if (new_GDS && save->isset == 0 ) {
    if ((save->last_GDS_change_no != GDS_change_no) && save->isset == 0 ) {
  	//save->npts = GB2_Sec3_npts(sec);
  	save->npts = ndata;
  	if (ndata > MAX_NXNY) fatal_error_i("f_mysql_speed: MAX_NXNY exceeded %d", MAX_NXNY);

        save->rows = calloc(ndata, sizeof(*save->rows));
        if (!save->rows && ndata) fatal_error("mysql_speed: row allocation failed", "");
        for (i = 0; i < ndata; i++) {
            save->rows[i].cap = 128;
            save->rows[i].text = calloc(1, save->rows[i].cap);
            if (!save->rows[i].text) fatal_error("mysql_speed: row allocation failed", "");
        }

	save->isset = 1;
//    } else if (new_GDS) {
    } else if (save->last_GDS_change_no != GDS_change_no) {
	fatal_error("f_mysql_speed, grid definition has to be the same for all fields","");
   }
   save->last_GDS_change_no = GDS_change_no;
   if (ndata != save->npts) fatal_error("mysql_speed: field grid size changed", "");
 
    /*Collect runtime and validtime into vt and rt*/

    reftime(sec, &year, &month, &day, &hour, &minute, &second);
//  p = sec[1];
//  sprintf(rt, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d", (p[12]<<8)+p[13], p[14],p[15],p[16],p[17],p[18]);
    sprintf(rt, "%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d", year,month,day,hour,minute,second);

    if (verftime(sec, &year, &month, &day, &hour, &minute, &second) == 0) {
        sprintf(vt,"%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d", year,month,day,hour,minute,second);
    }
    /*Check that runtimes are equal and validtimes are equal*/
    if (save->runtime[0] != 0 && strcmp(save->runtime,rt) != 0) {
	fprintf(stderr, "Error, runtime has to be equal for all fields\n");
    }
    strcpy(save->runtime,rt);

    if (save->validtime[0] != 0 && strcmp(save->validtime,vt) != 0) {
	fprintf(stderr, "Error, validtime has to be equal for all fields\n");
    }
    strcpy(save->validtime,vt);
	
    /*Get levels, parameter name, description and unit*/
    // f_lev(mode, sec, data, ndata, level_buf, local);
    f_lev(call_ARG0(level_buf, NULL));
    //if (ndata != save->npts && save->npts>0) fprintf(stderr,"ERROR: fields do not contain equally many gridpoints, %d , %d \n",save->npts,ndata);
 	   
    if (strcmp(level_buf, "reserved") == 0) return(0);
    getName(sec, mode, NULL, name, desc, unit);
    fprintf(stderr,"Start processing of %s at %s, runtime=%s, validtime=%s \n", name, level_buf,rt,vt);

    ctr = GB2_Center(sec);

    {
        /* Repeated ensemble members use the same mapping, including misses. */
        struct mysql_speed_mapping *cached = mysql_speed_mapping_find(save->mappings, ctr, name, level_buf);
        strcpy(new_name, ""); strcpy(new_level, "None"); strcpy(conv, "None"); strcpy(precision, "2");
        if (cached) {
            strcpy(new_name, cached->mapped_name); strcpy(new_level, cached->mapped_level);
            strcpy(conv, cached->conv); strcpy(precision, cached->precision);
        } else {
        sprintf(sql,"select * from wgrib2_parameter_mapping where center_id=%d and wgrib_param_name='%s' and wgrib_param_levelname='%s'", ctr, name, level_buf);
        fprintf(stderr,"SQL: %s\n", sql);
        if (mysql_query(save->conn, sql)) {
        fatal_error("f_mysql_speed: mysql error %s", mysql_error(save->conn));
        }
        printf("\nCenter_id: %d \n", ctr);
        res = mysql_use_result(save->conn);
        while ( (row = mysql_fetch_row(res) ) != NULL) {
        printf("our_name: %s \n", row[3]);
        sprintf(new_name,"%s", row[3]);
        sprintf(new_level,"%s", row[4]);
        sprintf(conv,"%s", row[5]);
        sprintf(precision, "%s", row[6]);
        }

        mysql_free_result(res);
        /* Preserve the last result if a mapping query returned multiple rows. */
            cached = malloc(sizeof(*cached)); if (!cached) fatal_error("mysql_speed: mapping allocation failed", "");
            cached->center = ctr; strcpy(cached->name, name); strcpy(cached->level, level_buf);
            strcpy(cached->mapped_name, new_name); strcpy(cached->mapped_level, new_level);
            strcpy(cached->conv, conv); strcpy(cached->precision, precision);
            cached->next = save->mappings; save->mappings = cached;
        }
    }
    printf("val_precision: %s \n", precision);
    printf("conversion: %s \n", conv);
    printf("our_levelname: %s \n", new_level);
	
    if (strcmp(new_level,"None") != 0) {
	fprintf(stderr,"Sets level %s as level %s\n", level_buf, new_level);
	sprintf(param, "%s_%s", new_name, new_level);
    } else {
	if (strcmp(level_buf,"surface") == 0) {
	    sprintf(param, "%s_0", name);
	} else {		
	    sscanf(level_buf,"%d %s", &level, &last);
 	    sprintf(param, "%s_%d", name, level);
	}
    }
	

    if (strlen(save->params) + strlen(param) + 2 >= MAX_SQL_INSERT) fatal_error("mysql_speed: too many columns", "");
    strcat(save->params,",");
    strcat(save->params,param);
	
    fprintf(stderr,"Gridpoints in data: %d\n", ndata);
    fprintf(stderr, "Gridpoints for insert: %d\n", save->npts);

    fprintf(stderr, "Remove unlikely: %d\n", save->remove_unlikely);
    fprintf(stderr, "Western longitudes: %d\n", save->wlon);

    {
        int conversion = mysql_speed_conversion(conv);
        int precipitation = !strcmp(name,"APCP") || !strcmp(name,"ASNOW") || !strcmp(name,"ACPCP");
        for (j = 0; j < ndata; j++) {
            struct mysql_speed_row *r = &save->rows[j];
            longitude = lon[j]; if (longitude > 180 && save->wlon == 1) longitude -= 360;
            latitude = lat[j]; value = data[j];
            if (save->remove_unlikely == 1 && value > 0 && value < 10e-8) value = 0;
            if (save->remove_unlikely == 1 && precipitation && value == 1) value = 0;
            value = mysql_speed_convert(value, conversion);
            if (r->len == 0) {
                if (save->has_rt) mysql_speed_append(r, "\"%s\",", rt);
                mysql_speed_append(r, "\"%s\",%g,%g", vt, latitude, longitude);
            }
            if (!UNDEFINED_VAL(data[j])) { r->has_value = 1; mysql_speed_append_value(r, value); }
            else mysql_speed_append_null(r);
        }
    }
    return 0;
}

#else

int f_mysql_speed(ARG6) {
    if (mode == -1) { fprintf(stderr,"mysql package not installed\n"); return 1; }
    return 0;
}

#endif

