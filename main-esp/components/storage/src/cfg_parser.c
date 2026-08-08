#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "cfg_parser.h"

enum cfg_status
cfg_parse(const char *text, size_t text_len, struct cfg_file *out)
{
    memset(out, 0, sizeof(*out));
    struct cfg_section *cur = NULL;

    size_t i = 0;
    while (i < text_len) {
        size_t line_start = i;
        while (i < text_len && text[i] != '\n')
            i++;
        size_t line_end = i;
        if (i < text_len)
            i++; // skip the '\n' itself

        if (line_end > line_start && text[line_end - 1] == '\r')
            line_end--; // tolerate CRLF

        size_t raw_len = line_end - line_start;
        if (raw_len >= CFG_MAX_LINE_LEN)
            return CFG_LINE_TOO_LONG;

        char line[CFG_MAX_LINE_LEN];
        memcpy(line, text + line_start, raw_len);

        for (size_t j = 0; j < raw_len; j++) {
            if (line[j] == '#' || line[j] == ';') {
                raw_len = j;
                break;
            }
        }

        size_t s = 0;
        while (s < raw_len && isspace((unsigned char)line[s]))
            s++;
        size_t e = raw_len;
        while (e > s && isspace((unsigned char)line[e - 1]))
            e--;
        if (s == e)
            continue; // blank or comment-only line

        memmove(line, line + s, e - s);
        size_t len = e - s;
        line[len] = '\0';

        if (line[0] == '[') {
            if (len < 2 || line[len - 1] != ']')
                return CFG_SYNTAX_ERROR;
            size_t name_len = len - 2;
            if (name_len == 0 || name_len >= CFG_MAX_SECTION_NAME_LEN)
                return CFG_NAME_TOO_LONG;
            if (out->section_count >= CFG_MAX_SECTIONS)
                return CFG_TOO_MANY_SECTIONS;

            cur = &out->sections[out->section_count++];
            memset(cur, 0, sizeof(*cur));
            memcpy(cur->name, line + 1, name_len);
            cur->name[name_len] = '\0';
            continue;
        }

        size_t delim = (size_t)-1;
        for (size_t j = 0; j < len; j++) {
            if (line[j] == '=' || line[j] == ':') {
                delim = j;
                break;
            }
        }
        if (delim == (size_t)-1 || cur == NULL)
            return CFG_SYNTAX_ERROR;

        size_t key_end = delim;
        while (key_end > 0 && isspace((unsigned char)line[key_end - 1]))
            key_end--;
        if (key_end == 0 || key_end >= CFG_MAX_KEY_LEN)
            return CFG_NAME_TOO_LONG;

        size_t val_start = delim + 1;
        while (val_start < len && isspace((unsigned char)line[val_start]))
            val_start++;
        size_t val_len = len - val_start;
        if (val_len >= CFG_MAX_VALUE_LEN)
            return CFG_NAME_TOO_LONG;

        if (cur->entry_count >= CFG_MAX_ENTRIES_PER_SECTION)
            return CFG_TOO_MANY_ENTRIES;

        struct cfg_entry *ent = &cur->entries[cur->entry_count++];
        memcpy(ent->key, line, key_end);
        ent->key[key_end] = '\0';
        memcpy(ent->value, line + val_start, val_len);
        ent->value[val_len] = '\0';
    }

    return CFG_OK;
}

const struct cfg_section *
cfg_find_section(const struct cfg_file *cfg, const char *name)
{
    for (size_t i = 0; i < cfg->section_count; i++)
        if (strcmp(cfg->sections[i].name, name) == 0)
            return &cfg->sections[i];
    return NULL;
}

const char *
cfg_get(const struct cfg_section *section, const char *key)
{
    if (!section)
        return NULL;
    for (size_t i = 0; i < section->entry_count; i++)
        if (strcmp(section->entries[i].key, key) == 0)
            return section->entries[i].value;
    return NULL;
}

const char *
cfg_get_default(const struct cfg_section *section, const char *key
                , const char *default_value)
{
    const char *v = cfg_get(section, key);
    return v ? v : default_value;
}

bool
cfg_get_long(const struct cfg_section *section, const char *key, long *out)
{
    const char *v = cfg_get(section, key);
    if (!v)
        return false;
    char *endptr;
    long val = strtol(v, &endptr, 10);
    if (endptr == v || *endptr != '\0')
        return false;
    *out = val;
    return true;
}

bool
cfg_get_double(const struct cfg_section *section, const char *key, double *out)
{
    const char *v = cfg_get(section, key);
    if (!v)
        return false;
    char *endptr;
    double val = strtod(v, &endptr);
    if (endptr == v || *endptr != '\0')
        return false;
    *out = val;
    return true;
}
