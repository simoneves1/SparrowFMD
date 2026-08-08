#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "khp_msgtable.h"
#include "cJSON.h"

// ---- small growable-array helpers -----------------------------------

struct entry_vec {
    struct khp_msg_entry *items;
    size_t count, cap;
};

static bool
entry_vec_push(struct entry_vec *v, struct khp_msg_entry e)
{
    if (v->count == v->cap) {
        size_t new_cap = v->cap ? v->cap * 2 : 16;
        struct khp_msg_entry *p = realloc(v->items, new_cap * sizeof(*p));
        if (!p)
            return false;
        v->items = p;
        v->cap = new_cap;
    }
    v->items[v->count++] = e;
    return true;
}

struct enumval_vec {
    struct khp_enum_value *items;
    size_t count, cap;
};

static bool
enumval_vec_push(struct enumval_vec *v, struct khp_enum_value e)
{
    if (v->count == v->cap) {
        size_t new_cap = v->cap ? v->cap * 2 : 8;
        struct khp_enum_value *p = realloc(v->items, new_cap * sizeof(*p));
        if (!p)
            return false;
        v->items = p;
        v->cap = new_cap;
    }
    v->items[v->count++] = e;
    return true;
}

struct group_vec {
    struct khp_enum_group *items;
    size_t count, cap;
};

static bool
group_vec_push(struct group_vec *v, struct khp_enum_group g)
{
    if (v->count == v->cap) {
        size_t new_cap = v->cap ? v->cap * 2 : 8;
        struct khp_enum_group *p = realloc(v->items, new_cap * sizeof(*p));
        if (!p)
            return false;
        v->items = p;
        v->cap = new_cap;
    }
    v->items[v->count++] = g;
    return true;
}

// ---- cleanup on failed parse ------------------------------------------

static void
free_entries(struct khp_msg_entry *entries, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        free(entries[i].format);
        free(entries[i].name);
    }
    free(entries);
}

static void
free_groups(struct khp_enum_group *groups, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        for (size_t j = 0; j < groups[i].count; j++)
            free(groups[i].values[j].name);
        free(groups[i].values);
        free(groups[i].name);
    }
    free(groups);
}

// ---- message name extraction -------------------------------------------

static char *
first_token_dup(const char *format)
{
    size_t i = 0;
    while (format[i] && !isspace((unsigned char)format[i]))
        i++;
    char *name = malloc(i + 1);
    if (!name)
        return NULL;
    memcpy(name, format, i);
    name[i] = '\0';
    return name;
}

// ---- commands/responses/output -----------------------------------------

static bool
add_messages_from(struct entry_vec *entries, const cJSON *root
                  , const char *key, enum khp_msg_type type, bool required)
{
    const cJSON *obj = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!obj) {
        if (required)
            return false;
        return true; // optional key absent -- nothing to add
    }
    if (!cJSON_IsObject(obj))
        return false;

    const cJSON *item;
    cJSON_ArrayForEach(item, obj) {
        if (!item->string || !cJSON_IsNumber(item))
            return false;
        struct khp_msg_entry e = {0};
        e.msgid = (uint32_t)item->valueint;
        e.type = type;
        e.format = strdup(item->string);
        e.name = first_token_dup(item->string);
        if (!e.format || !e.name) {
            free(e.format);
            free(e.name);
            return false;
        }
        if (!entry_vec_push(entries, e)) {
            free(e.format);
            free(e.name);
            return false;
        }
    }
    return true;
}

// ---- enumerations, including DECL_ENUMERATION_RANGE expansion ---------

static bool
add_enum_group(struct group_vec *groups, const cJSON *group_json
              , const char *group_name)
{
    struct enumval_vec values = {0};
    const cJSON *item;
    cJSON_ArrayForEach(item, group_json) {
        if (!item->string)
            goto fail;

        if (cJSON_IsNumber(item)) {
            struct khp_enum_value v = {strdup(item->string), item->valueint};
            if (!v.name || !enumval_vec_push(&values, v)) {
                free(v.name);
                goto fail;
            }
            continue;
        }

        // Range form: key's trailing digits (if any) give the starting
        // index, key's non-digit prefix is the name root, and the JSON
        // value is a 2-element [start_value, count] array. Mirrors
        // klippy/msgproto.py's fill_enumerations exactly.
        if (!cJSON_IsArray(item) || cJSON_GetArraySize(item) != 2)
            goto fail;
        const cJSON *start_v = cJSON_GetArrayItem(item, 0);
        const cJSON *count_v = cJSON_GetArrayItem(item, 1);
        if (!cJSON_IsNumber(start_v) || !cJSON_IsNumber(count_v))
            goto fail;

        const char *key = item->string;
        size_t len = strlen(key);
        size_t root_len = len;
        while (root_len > 0 && isdigit((unsigned char)key[root_len - 1]))
            root_len--;
        int start_enum = (root_len != len) ? atoi(key + root_len) : 0;
        int start_value = start_v->valueint;
        int count = count_v->valueint;

        for (int i = 0; i < count; i++) {
            char namebuf[64];
            snprintf(namebuf, sizeof(namebuf), "%.*s%d"
                    , (int)root_len, key, start_enum + i);
            struct khp_enum_value v = {strdup(namebuf), start_value + i};
            if (!v.name || !enumval_vec_push(&values, v)) {
                free(v.name);
                goto fail;
            }
        }
    }

    struct khp_enum_group g;
    g.name = strdup(group_name);
    g.values = values.items;
    g.count = values.count;
    if (!g.name) {
        free(g.name);
        goto fail;
    }
    if (!group_vec_push(groups, g)) {
        free(g.name);
        goto fail;
    }
    return true;

fail:
    for (size_t i = 0; i < values.count; i++)
        free(values.items[i].name);
    free(values.items);
    return false;
}

static bool
add_enumerations(struct group_vec *groups, const cJSON *root)
{
    const cJSON *enums = cJSON_GetObjectItemCaseSensitive(root, "enumerations");
    if (!enums)
        return true; // optional, defaults to empty
    if (!cJSON_IsObject(enums))
        return false;

    const cJSON *group_json;
    cJSON_ArrayForEach(group_json, enums) {
        if (!group_json->string || !cJSON_IsObject(group_json))
            return false;
        if (!add_enum_group(groups, group_json, group_json->string))
            return false;
    }
    return true;
}

// ---- public API ---------------------------------------------------------

bool
khp_msgtable_parse(struct khp_msgtable *out, const char *json, size_t json_len)
{
    memset(out, 0, sizeof(*out));

    cJSON *root = cJSON_ParseWithLength(json, json_len);
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }

    struct entry_vec entries = {0};
    struct group_vec groups = {0};
    bool ok = add_messages_from(&entries, root, "commands", KHP_MSG_COMMAND, true)
           && add_messages_from(&entries, root, "responses", KHP_MSG_RESPONSE, true)
           && add_messages_from(&entries, root, "output", KHP_MSG_OUTPUT, false)
           && add_enumerations(&groups, root);

    char *version = NULL, *build_versions = NULL;
    if (ok) {
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, "version");
        const cJSON *b = cJSON_GetObjectItemCaseSensitive(root, "build_versions");
        version = strdup((v && cJSON_IsString(v)) ? v->valuestring : "");
        build_versions = strdup((b && cJSON_IsString(b)) ? b->valuestring : "");
        ok = version && build_versions;
    }

    cJSON_Delete(root);

    if (!ok) {
        free_entries(entries.items, entries.count);
        free_groups(groups.items, groups.count);
        free(version);
        free(build_versions);
        return false;
    }

    out->entries = entries.items;
    out->entry_count = entries.count;
    out->enum_groups = groups.items;
    out->enum_group_count = groups.count;
    out->version = version;
    out->build_versions = build_versions;
    return true;
}

void
khp_msgtable_free(struct khp_msgtable *t)
{
    free_entries(t->entries, t->entry_count);
    free_groups(t->enum_groups, t->enum_group_count);
    free(t->version);
    free(t->build_versions);
    memset(t, 0, sizeof(*t));
}

const struct khp_msg_entry *
khp_msgtable_find_by_name(const struct khp_msgtable *t, const char *name)
{
    for (size_t i = 0; i < t->entry_count; i++)
        if (strcmp(t->entries[i].name, name) == 0)
            return &t->entries[i];
    return NULL;
}

const struct khp_msg_entry *
khp_msgtable_find_by_id(const struct khp_msgtable *t, uint32_t msgid)
{
    for (size_t i = 0; i < t->entry_count; i++)
        if (t->entries[i].msgid == msgid)
            return &t->entries[i];
    return NULL;
}

bool
khp_msgtable_enum_value(const struct khp_msgtable *t, const char *group
                        , const char *name, int *out_value)
{
    for (size_t i = 0; i < t->enum_group_count; i++) {
        if (strcmp(t->enum_groups[i].name, group) != 0)
            continue;
        for (size_t j = 0; j < t->enum_groups[i].count; j++) {
            if (strcmp(t->enum_groups[i].values[j].name, name) == 0) {
                *out_value = t->enum_groups[i].values[j].value;
                return true;
            }
        }
        return false;
    }
    return false;
}
