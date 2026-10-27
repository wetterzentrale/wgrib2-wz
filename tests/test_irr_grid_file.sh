#!/usr/bin/env bash
set -euo pipefail
wgrib2=$1
sample=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
printf '10:20:30:40\n' > "$work/points"
"$wgrib2" "$sample" -d 1 -irr_grid '10:20:30:40' 100 "$work/inline" >/dev/null
"$wgrib2" "$sample" -d 1 -irr_grid "@$work/points" 100 "$work/file" >/dev/null
cmp "$work/inline" "$work/file"
# A list too large for a single exec argument is still accepted from a file.
awk 'BEGIN { for (i=0; i<50000; i++) printf "%s10:20", i ? ":" : ""; print "" }' > "$work/large"
"$wgrib2" "$sample" -d 1 -irr_grid "@$work/large" 100 "$work/large.grib2" >/dev/null
: > "$work/empty"
printf '10:20:30:bad' > "$work/bad"
printf '10:20junk' > "$work/trailing"
printf '10:nan' > "$work/nonfinite"
for name in missing empty bad trailing nonfinite; do
  if "$wgrib2" "$sample" -d 1 -irr_grid "@$work/$name" 100 "$work/invalid" > /dev/null 2>&1; then
    echo "Unexpected success for $name" >&2
    exit 1
  fi
done
