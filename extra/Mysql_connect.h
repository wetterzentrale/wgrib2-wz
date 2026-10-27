/*
 * Mysql_connect.h: connect to MySQL / MariaDB for the -mysql* functions.
 *
 * Credentials are read from the [wgrib2] group in MySQL option files.
 * Use "-" as the user argument to read the user from that group as well.
 * The group can also specify socket=... for host "localhost".
 */
#ifndef WGRIB2_MYSQL_CONNECT_H
#define WGRIB2_MYSQL_CONNECT_H

#ifdef USE_MYSQL

#include <string.h>
#include <mysql.h>

static MYSQL *wgrib2_mysql_connect(MYSQL *conn, const char *server, const char *user,
        const char *database) {
    const char *u = (user != NULL && strcmp(user, "-") == 0) ? NULL : user;

    if (mysql_options(conn, MYSQL_READ_DEFAULT_GROUP, "wgrib2")) return NULL;
    return mysql_real_connect(conn, server, u, NULL, database, 0, NULL, 0);
}

#endif
#endif
