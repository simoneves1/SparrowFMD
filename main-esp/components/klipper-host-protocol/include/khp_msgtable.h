// Parses the decompressed JSON text of an MCU's data dictionary (see
// khp_dictionary.h) into a queryable table: message name/id lookups for
// commands, responses, and debug output messages, plus named
// enumeration groups and (via khp_msgtable_lookup_params) each
// message's typed parameter list. Mirrors klippy/msgproto.py's
// MessageParser (_init_messages, fill_enumerations, lookup_params) --
// same JSON schema, same message-name-is-the-first-word-of-the-format-
// string convention.
//
// Still out of scope: the dictionary's "config" key (a flat map of MCU
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

// A message format string's parameters, e.g. "identify offset=%u
// count=%u" has params [{name:"offset",type:UINT32}, {name:"count",
// type:UINT32}] -- everything after the first (name) token. Ported from
// klippy/msgproto.py's MessageTypes table + lookup_params.
enum khp_param_type {
    KHP_PARAM_UINT32,  // %u
    KHP_PARAM_INT32,   // %i
    KHP_PARAM_UINT16,  // %hu
    KHP_PARAM_INT16,   // %hi
    KHP_PARAM_BYTE,    // %c  (0-255, still VLQ-encoded on the wire)
    KHP_PARAM_STRING,  // %s  -- VLQ-length-prefixed bytes on the wire;
    KHP_PARAM_BUFFER,  // %*s -- these three differ in klippy only in
    KHP_PARAM_PROGMEM_BUFFER, // %.*s -- how the *caller* supplies the
                       // value, not in wire encoding, so they're kept
                       // as distinct enum values for fidelity to the
                       // dictionary but decode identically for now.
};

struct khp_param {
    char *name;
    enum khp_param_type type;
    const struct khp_enum_group *enum_group; // NULL if not enum-typed;
                                              // points into the same
                                              // khp_msgtable this was
                                              // parsed from
};

struct khp_param_list {
    struct khp_param *params;
    size_t count;
};

// Parses `format`'s parameter tokens (everything after the message
// name) against `t`'s enumeration groups for enum-typed parameter
// matching. `format` would typically come from a khp_msg_entry's
// `format` field, but any well-formed "name key1=%type1 key2=%type2..."
// string works. Returns false on an unrecognized %-type token.
bool khp_msgtable_lookup_params(const struct khp_msgtable *t
                                , const char *format
                                , struct khp_param_list *out);
void khp_param_list_free(struct khp_param_list *p);

#endif // khp_msgtable.h
