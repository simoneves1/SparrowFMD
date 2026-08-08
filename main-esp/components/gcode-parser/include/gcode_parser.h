// Tokenizes one line of G-code text into a command word (letter + code
// + optional subcode, e.g. "G1" or "G92.1") and its parameters, without
// any knowledge of what a given command means -- see main-esp/src/README.md's
// "gcode-parser" note: this stays decoupled from kinematics on purpose,
// so it doesn't need to know which kinematics type is active.
//
// Handles, in line with common RepRap-flavor G-code (the dialect most
// slicers emit and Klipper itself accepts): ';' end-of-line comments,
// '(...)' inline comments, an optional leading "N<line-number>", and an
// optional trailing "*<checksum>" (classic XOR checksum, validated if
// present).
//
// Deliberately lenient about parameters: a parameter token that doesn't
// look like <letter><number> (e.g. "M117 Hello World"'s "Hello") isn't
// a parse error -- param collection just stops there, and `raw_args`
// still holds the complete, untouched remainder of the line after the
// command word for commands (M117/M118, custom macros with NAME=value
// syntax, etc.) that don't use letter=value parameters at all. A
// genuinely malformed command *word* (no letter, or a letter with no
// digits after it) is still a real syntax error, though -- that's
// structural, not a "this command has free-text args" situation.
#ifndef GCODE_PARSER_H
#define GCODE_PARSER_H

#include <stddef.h>
#include <stdint.h>

#define GCODE_MAX_LINE 128
#define GCODE_MAX_PARAMS 12

struct gcode_param {
    char letter; // uppercase
    double value;
};

struct gcode_command {
    int line_number; // -1 if no "N<n>" prefix was present
    char letter;      // uppercase command letter, e.g. 'G', 'M', 'T'
    int code;          // integer part, e.g. 1 for "G1"
    int subcode;        // integer part after '.', e.g. 1 for "G92.1"; -1 if absent

    struct gcode_param params[GCODE_MAX_PARAMS];
    size_t param_count;

    // Everything after the command word, comments/checksum already
    // stripped, leading whitespace trimmed, null-terminated. Always
    // valid (possibly empty) on GCODE_OK, regardless of whether
    // `params` captured anything from it.
    char raw_args[GCODE_MAX_LINE];
};

enum gcode_status {
    GCODE_OK,
    GCODE_EMPTY,          // blank, comment-only, or "%" line -- nothing to do
    GCODE_BAD_CHECKSUM,   // a "*<n>" was present and didn't match
    GCODE_SYNTAX_ERROR,   // no command word, or a malformed one (see above)
    GCODE_LINE_TOO_LONG,  // line_len >= GCODE_MAX_LINE
    GCODE_TOO_MANY_PARAMS,// more than GCODE_MAX_PARAMS letter=value tokens
};

enum gcode_status gcode_parse_line(const char *line, size_t line_len
                                   , struct gcode_command *out);

// NULL if no parameter with that letter is present.
const struct gcode_param *gcode_find_param(const struct gcode_command *cmd
                                           , char letter);

#endif // gcode_parser.h
