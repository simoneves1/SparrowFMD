// Parses main-esp's per-printer runtime config file -- see
// main-esp/README.md's "Runtime config, not compile-time" note: the
// configurator website generates this file, main-esp just loads it.
//
// Deliberately INI-style (like Klipper's own printer.cfg, for user/
// tooling familiarity), but this is main-esp's own config format, not a
// requirement to byte-match Klipper's config parser -- so it's a fresh,
// simpler implementation, not a port. Notably NOT supported (unlike
// Klipper's config parser): multi-line continuation values -- main-esp
// doesn't need Klipper's gcode_macro-style multi-line script blocks,
// and dropping that keeps this parser meaningfully simpler.
//
//   [section_name]
//   key: value
//   key2 = value2   ; both ':' and '=' delimiters are accepted
//   # full-line comments (and ';') are ignored, as are blank lines
//
// Fixed-capacity, no dynamic allocation (CFG_MAX_* below) -- deliberate
// for an embedded target: a fixed memory budget beats fragmentation
// risk for something read once at boot.
#ifndef CFG_PARSER_H
#define CFG_PARSER_H

#include <stddef.h>
#include <stdbool.h>

#define CFG_MAX_SECTIONS 16
#define CFG_MAX_ENTRIES_PER_SECTION 32
#define CFG_MAX_SECTION_NAME_LEN 32
#define CFG_MAX_KEY_LEN 32
#define CFG_MAX_VALUE_LEN 64
#define CFG_MAX_LINE_LEN 128

struct cfg_entry {
    char key[CFG_MAX_KEY_LEN];
    char value[CFG_MAX_VALUE_LEN];
};

struct cfg_section {
    char name[CFG_MAX_SECTION_NAME_LEN];
    struct cfg_entry entries[CFG_MAX_ENTRIES_PER_SECTION];
    size_t entry_count;
};

struct cfg_file {
    struct cfg_section sections[CFG_MAX_SECTIONS];
    size_t section_count;
};

enum cfg_status {
    CFG_OK,
    CFG_SYNTAX_ERROR,        // key=value before any [section], or a
                              // malformed line that's neither a section
                              // header, a key=value pair, a comment, nor blank
    CFG_TOO_MANY_SECTIONS,
    CFG_TOO_MANY_ENTRIES,    // too many keys within one section
    CFG_LINE_TOO_LONG,       // a line >= CFG_MAX_LINE_LEN
    CFG_NAME_TOO_LONG,       // a section name, key, or value overflowed
                              // its fixed-size field
};

// Parses the whole config file text in one call. On any error, *out is
// left in whatever partial state parsing reached -- callers should
// treat a non-CFG_OK status as "don't trust this config," not "use what
// got parsed so far."
enum cfg_status cfg_parse(const char *text, size_t text_len
                          , struct cfg_file *out);

// NULL if no section with that name exists.
const struct cfg_section *cfg_find_section(const struct cfg_file *cfg
                                           , const char *name);

// NULL if the section has no entry with that key.
const char *cfg_get(const struct cfg_section *section, const char *key);
const char *cfg_get_default(const struct cfg_section *section
                            , const char *key, const char *default_value);

// Typed accessors: parse the string value as an integer/float. Return
// false (leaving *out untouched) if the key is absent or the value
// isn't a valid number -- callers that want a default on parse failure
// should check the return value themselves, same as cfg_get vs.
// cfg_get_default.
bool cfg_get_long(const struct cfg_section *section, const char *key
                  , long *out);
bool cfg_get_double(const struct cfg_section *section, const char *key
                    , double *out);

#endif // cfg_parser.h
