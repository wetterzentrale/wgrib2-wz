#!/bin/sh
# Integration test against an explicitly supplied disposable database.
set -eu

wgrib2=$1
input=$2
client=$3
host=$4
database=$5
config=$6

case $database in
  ''|*[!A-Za-z0-9_]*) echo 'MYSQL_TEST_DATABASE must contain only letters, digits and underscores' >&2; exit 1 ;;
esac

test_dir=$(mktemp -d "${TMPDIR:-/tmp}/wgrib2_mysql_test_XXXXXXXX")
table=$(basename "$test_dir")
created=0
sql() {
  "$client" --defaults-file="$test_dir/.my.cnf" --host="$host" --database="$database" --batch --skip-column-names "$@"
}
cleanup() {
  if [ "$created" -eq 1 ]; then
    sql -e "DROP TABLE IF EXISTS \`$table\`" || echo "Could not remove test table $table" >&2
  fi
  rm -rf "$test_dir"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
cp "$config" "$test_dir/.my.cnf"
chmod 600 "$test_dir/.my.cnf"

sql -e "CREATE TABLE \`$table\` (rt DATETIME, vt DATETIME, lat DOUBLE, lon DOUBLE, param VARCHAR(100), level VARCHAR(100), value DOUBLE)"
created=1
HOME="$test_dir" "$wgrib2" "$input" -d 1 -mysql "$host" - "$database" "$table"

result=$(sql -e "SELECT COUNT(*), COALESCE(SUM(rt <> '2009-06-05 00:00:00' OR vt <> '2009-06-12 12:00:00' OR param <> 'TMP' OR level <> '500 mb' OR rt IS NULL OR vt IS NULL OR param IS NULL OR level IS NULL OR lat IS NULL OR lon IS NULL OR value IS NULL), 0), COALESCE(SUM(lat = -90 AND lon = 0 AND value = 223), 0), COALESCE(SUM(lat = -90 AND lon = -1 AND value = 223), 0) FROM \`$table\`")
expected=$(printf '65160\t0\t1\t1')
if [ "$result" != "$expected" ]; then
  printf 'Unexpected MySQL results (count, invalid metadata, samples): %s\nExpected: %s\n' "$result" "$expected" >&2
  exit 1
fi
