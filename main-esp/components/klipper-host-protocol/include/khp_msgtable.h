// Parses the decompressed JSON text of an MCU's data dictionary (see
// khp_dictionary.h) into a queryable table: message name/id lookups for
// commands, responses, and debug output messages, plus named
// enumeration groups. Mirrors klippy/msgproto.py's MessageParser
// (_init_messages, fill_enumerations) -- same JSON schema, same
// message-name-is-the-first-word-of-the-format-string convention.
//
// Deliberately out of scope here: parsing each format string's `%u`
// `%hu` `%c` `%*s` `:enum_name`-style parameter specifiers into a typed
// parameter list (klippy's lookup_params/param_types). That's what
// actual command encoding/decoding needs and is its own follow-up --
// this module only gets you from JSON text to "here's the id and raw
// format string for message X," which is enough to identify messages
// even before full parameter-level encode/decode exists.
//
// Also out of scope: the dictionary's "config" key (a flat map of MCU
// build-time constants) -- not needed for anything built so far.
#ifndef KHP_MSGTABLE_H
#define KHP_MSGTABLE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

enum khp_msg_type {
    KHP_MSG_COMMAND,   // host -> MCU
    KHP_MSG_RESPONSE,  // MCU -> host
    KHP_MSG_OUTPUT,    // MCU -> host, debug/log only (per OutputFormat in
                       // klippy, not part of normal command/response flow)
};

struct khp_msg_entry {
    uint32_t msgid;
    enum khp_msg_type type;
    char *format; // full format string, e.g. "identify offset=%u count=%u"
    char *name;   // format's first whitespace-separated token, e.g. "identify"
};

struct khp_enum_value {
    char *name;
    int value;
};

struct khp_enum_group {
    char *name;
    struct khp_enum_value *values;
    size_t count;
};

struct khp_msgtable {
    struct khp_msg_entry *entries;
    size_t entry_count;
    struct khp_enum_group *enum_groups;
    size_t enum_group_count;
    char *version;         // may be empty string, never NULL
    char *build_versions;  // may be empty string, never NULL
};

// Parses json (need not be null-terminated; json_len bytes are read) into
// *out. Returns false on a JSON syntax error or a missing/malformed
// 'commands'/'responses' key -- 'output' and 'enumerations' are
// optional and default to empty if absent, matching klippy's
// data.get('output', {}) / data.get('enumerations', {}).
bool khp_msgtable_parse(struct khp_msgtable *out, const char *json
                        , size_t json_len);
void khp_msgtable_free(struct khp_msgtable *t);

// NULL if no entry with that name/id exists. The returned pointer is
// owned by the table and only valid until khp_msgtable_free().
const struct khp_msg_entry *khp_msgtable_find_by_name(
    const struct khp_msgtable *t, const char *name);
const struct khp_msg_entry *khp_msgtable_find_by_id(
    const struct khp_msgtable *t, uint32_t msgid);

// Looks up a single enum value by (group name, entry name), e.g.
// ("pin", "PA4"). Writes *out_value and returns true if found.
bool khp_msgtable_enum_value(const struct khp_msgtable *t
                             , const char *group, const char *name
                             , int *out_value);

#endif // khp_msgtable.h
