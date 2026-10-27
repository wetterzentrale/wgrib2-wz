#ifndef MYSQL_SPEED_COMMON_H
#define MYSQL_SPEED_COMMON_H
#include <stdarg.h>

/* Mapping entries, including misses, live for one loader invocation. */
struct mysql_speed_mapping {
    int center;
    char name[100], level[100], mapped_name[50], mapped_level[50], conv[50], precision[10];
    struct mysql_speed_mapping *next;
};
static struct mysql_speed_mapping *mysql_speed_mapping_find(struct mysql_speed_mapping *p, int center, const char *name, const char *level) {
    for (; p; p = p->next) if (p->center == center && strcmp(p->name, name) == 0 && strcmp(p->level, level) == 0) return p;
    return NULL;
}
static void mysql_speed_mapping_free(struct mysql_speed_mapping *p) {
    while (p) { struct mysql_speed_mapping *next = p->next; free(p); p = next; }
}
static int mysql_speed_conversion(const char *s) {
    if (!strcmp(s, "GPH_M")) return 1;
    if (!strcmp(s, "M_MM")) return 2;
    if (!strcmp(s, "PERC_PART") || !strcmp(s, "PA_HPA")) return 3;
    if (!strcmp(s, "PART_PERC")) return 4;
    if (!strcmp(s, "K_C")) return 5;
    return 0;
}
static double mysql_speed_convert(double value, int c) {
    switch (c) { case 1: return value / 9.82; case 2: return value * 1000; case 3: return value / 100; case 4: return value * 100; case 5: return value - 273.16; default: return value; }
}
struct mysql_speed_row { char *text; size_t len, cap; unsigned char has_value; };
/* The client may request any chunk size, including part of a row. */
struct mysql_speed_stream { struct mysql_speed_row *rows; unsigned int count, index; size_t offset; };
static void mysql_speed_append(struct mysql_speed_row *r, const char *fmt, ...) {
    va_list ap, copy; int n; size_t pos = r->len;
    va_start(ap, fmt); va_copy(copy, ap); n = vsnprintf(r->text + pos, r->cap - pos, fmt, copy); va_end(copy);
    if (n < 0) fatal_error("mysql_speed: row formatting failed", "");
    if (pos + (size_t)n + 1 > r->cap) {
        size_t cap = r->cap; char *p;
        while (cap < pos + (size_t)n + 1) cap *= 2;
        p = realloc(r->text, cap); if (!p) fatal_error("mysql_speed: row allocation failed", "");
        r->text = p; r->cap = cap;
        vsnprintf(r->text + pos, r->cap - pos, fmt, ap);
    }
    va_end(ap); r->len = pos + (size_t)n;
}
/* Preserve the original %lg representation used by LOAD DATA. */
static void mysql_speed_append_value(struct mysql_speed_row *r, double value) {
    int n = snprintf(r->text + r->len, r->cap - r->len, ",%lg", value);
    if (n < 0) fatal_error("mysql_speed: row formatting failed", "");
    if ((size_t)n >= r->cap - r->len) {
        size_t cap = r->cap; char *p;
        while (cap - r->len <= (size_t)n) cap *= 2;
        p = realloc(r->text, cap); if (!p) fatal_error("mysql_speed: row allocation failed", "");
        r->text = p; r->cap = cap;
        snprintf(r->text + r->len, r->cap - r->len, ",%lg", value);
    }
    r->len += (size_t)n;
}
static void mysql_speed_append_null(struct mysql_speed_row *r) {
    if (r->cap - r->len <= 5) {
        size_t cap = r->cap; char *p;
        while (cap - r->len <= 5) cap *= 2;
        p = realloc(r->text, cap); if (!p) fatal_error("mysql_speed: row allocation failed", "");
        r->text = p; r->cap = cap;
    }
    memcpy(r->text + r->len, ",NULL", 6); r->len += 5;
}
static int mysql_speed_init(void **ptr, const char *filename, void *userdata) {
    struct mysql_speed_stream *s = userdata; (void)filename; s->index = 0; s->offset = 0; *ptr = s; return 0;
}
static int mysql_speed_read(void *ptr, char *buf, unsigned int size) {
    struct mysql_speed_stream *s = ptr; unsigned int used = 0;
    while (used < size && s->index < s->count) {
        struct mysql_speed_row *r = &s->rows[s->index]; size_t n;
        if (!r->has_value) { s->index++; continue; }
        if (s->offset < r->len) {
            n = r->len - s->offset; if (n > size - used) n = size - used;
            memcpy(buf + used, r->text + s->offset, n); used += n; s->offset += n;
        } else { buf[used++] = '\n'; s->index++; s->offset = 0; }
    }
    return (int)used;
}
static void mysql_speed_end(void *ptr) { (void)ptr; }
static int mysql_speed_error(void *ptr, char *buf, unsigned int len) { (void)ptr; if (len) snprintf(buf, len, "mysql_speed: stream error"); return 1; }
#endif
