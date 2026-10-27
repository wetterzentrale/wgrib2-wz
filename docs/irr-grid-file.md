# Irregular grid coordinate files

`-irr_grid` accepts `@path` instead of the inline longitude/latitude list. The file contains the same colon-separated `longitude:latitude` sequence, with optional trailing whitespace. This avoids operating-system argument-size limits for large point lists. Inline lists remain supported.

```bash
wgrib2 input.grib2 -irr_grid @coordinates.points 100 cities.grib2
```

The radius argument retains its existing behavior; this change does not add a distance cutoff. The `@` prefix is reserved for filenames in this argument.
