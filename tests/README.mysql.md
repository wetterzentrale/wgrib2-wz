# Testing the MySQL output options

## Automated integration test

The opt-in `mysql_integration` CTest test loads the checked-in simple-packing
fixture into a uniquely named table and checks the resulting rows. It needs a
running MySQL or MariaDB server, a `mysql` or `mariadb` command-line client,
and a disposable database in which the test user can create and drop tables.
Both the server and client must permit `LOAD DATA LOCAL INFILE`. The test does
not create or drop the database itself.

Create a private option file readable by the test runner. Both groups must
contain credentials for the same user; include `socket` in each group when
connecting through a non-default local socket:

```ini
[client]
user=TEST_USER
password=TEST_PASSWORD
socket=/path/to/mysql.sock
[wgrib2]
user=TEST_USER
password=TEST_PASSWORD
socket=/path/to/mysql.sock
```

Use `chmod 600` on the file, then configure and run:

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DBUILD_EXTRA=ON -DUSE_MYSQL=ON \
  -DMYSQL_INTEGRATION_TEST=ON -DMYSQL_TEST_DATABASE=wgrib2_test \
  -DMYSQL_TEST_HOST=localhost -DMYSQL_TEST_CONFIG=/path/to/test-my.cnf
cmake --build build
ctest --test-dir build -R '^mysql_integration$' --output-on-failure
```

The host defaults to `localhost`; database names may contain only letters,
digits and underscores. The test copies the option file to a private temporary
home directory and removes its table and files when it exits. An enabled test
fails if its database is unavailable. With `MYSQL_INTEGRATION_TEST=OFF` (the
default), normal tests do not require a database or MySQL command-line client.

## Manual tests

These tests require a disposable MySQL or MariaDB database, a client library and
server that both allow `LOAD DATA LOCAL INFILE`, and a user with permission to
read and insert into the test tables. Do not use production tables. Build with
`-DBUILD_EXTRA=ON -DUSE_MYSQL=ON`; the MySQL client development package must
provide `mariadb_config` or `mysql_config`. Run the following from the source
root, adjusting the binary path, host and database name to your setup.

Put the test user's credentials in `~/.my.cnf` under `[wgrib2]` (mode 600),
including `socket=...` if needed. The `-` user argument below uses that group; passwords are always read from
the option file. The SQL client may require separate credentials. Avoid putting a
password on the wgrib2 command line: it can appear in process listings and
error messages.

Create an empty table in the disposable database with the columns in this
order (the basic `-mysql` option loads by position):

```sql
CREATE TABLE mysql_test (
  rt DATETIME, vt DATETIME, lat DOUBLE, lon DOUBLE,
  param VARCHAR(100), level VARCHAR(100), value DOUBLE
);
```

Use a small GRIB2 file, for example `tests/data/gdas.t12z.pgrb2.1p00.anl.75r.grib2`.
Select a single field to make the comparison straightforward:

```sh
GRIB=tests/data/gdas.t12z.pgrb2.1p00.anl.75r.grib2
WGRIB2=build/src/wgrib2
"$WGRIB2" "$GRIB" -d 1 -mysql localhost - test_db mysql_test
"$WGRIB2" "$GRIB" -d 1 -csv /tmp/wgrib2-mysql-reference.csv
```

Inspect `SELECT * FROM mysql_test` and compare the row count, coordinates,
parameter, level, times and values to the CSV. Account for the basic MySQL
option converting longitudes greater than 180 to western longitudes and for
normal floating-point formatting differences. Repeat with a GRIB field containing
missing values and verify that undefined grid points are omitted. A successful
connection alone does not test the load.

For `-mysql_dump` and `-mysql_speed`, additionally create an empty
`wgrib2_parameter_mapping` table with the first seven columns ordered as
`center_id`, `wgrib_param_name`, `wgrib_param_levelname`, mapped parameter,
mapped level, conversion, precision. Create a destination table with `rt`,
`vt`, `lat`, `lon`, and one column for each selected parameter/level. With an
empty mapping table, the default column name is `PARAM_0` for surface or
`PARAM_<numeric level>` for other levels. Run each option separately against an
empty destination table, using arguments `localhost - test_db TABLE 1 0`,
and compare the results against the basic option. Then insert a mapping row and
check the renamed column and conversion. Test `W=0` and `W=1`, and `PV=0` and
`PV=1` with data that exercises the relevant filtering.

For `-mysql_speed_ens`, use an ensemble GRIB and an analogous
`wgrib2_parameter_mapping_ens` table; destination columns include the `_Pnn`
member suffix and the load omits `rt`. Check at least two members against their
inventories and decoded values. All bulk
options require compatible grids for fields combined into one load. `-mysql_dump`
also deletes existing rows for the processed run time, so use only a disposable
table. No database service or credentials are provided by the normal test suite.
