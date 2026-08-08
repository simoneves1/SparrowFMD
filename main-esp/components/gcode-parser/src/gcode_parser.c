#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "gcode_parser.h"

enum gcode_status
gcode_parse_line(const char *line, size_t line_len, struct gcode_command *out)
{
    memset(out, 0, sizeof(*out));
    out->line_number = -1;
    out->subcode = -1;

    if (line_len >= GCODE_MAX_LINE)
        return GCODE_LINE_TOO_LONG;

    // Strip '(...)' inline comments and truncate at a ';' end-of-line
    // comment while copying into a scratch buffer.
    char buf[GCODE_MAX_LINE];
    size_t blen = 0;
    bool in_paren = false;
    for (size_t i = 0; i < line_len; i++) {
        char c = line[i];
        if (c == '(') { in_paren = true; continue; }
        if (c == ')') { in_paren = false; continue; }
        if (in_paren) continue;
        if (c == ';') break;
        buf[blen++] = c;
    }
    buf[blen] = '\0';

    size_t start = 0;
    while (start < blen && isspace((unsigned char)buf[start]))
        start++;
    size_t end = blen;
    while (end > start && isspace((unsigned char)buf[end - 1]))
        end--;

    if (start == end || buf[start] == '%')
        return GCODE_EMPTY;

    const char *p = buf + start;
    const char *line_end = buf + end;

    // Optional trailing "*<checksum>": classic XOR of every character
    // before the '*'.
    const char *star = memchr(p, '*', (size_t)(line_end - p));
    if (star) {
        uint8_t computed = 0;
        for (const char *c = p; c < star; c++)
            computed ^= (uint8_t)*c;
        char *endptr;
        long expected = strtol(star + 1, &endptr, 10);
        if (endptr == star + 1 || expected < 0 || expected > 255
            || (uint8_t)expected != computed)
            return GCODE_BAD_CHECKSUM;
        line_end = star;
        while (line_end > p && isspace((unsigned char)line_end[-1]))
            line_end--;
    }

    // Optional leading "N<line-number>".
    if ((*p == 'N' || *p == 'n') && p + 1 < line_end
        && isdigit((unsigned char)p[1])) {
        char *endptr;
        out->line_number = (int)strtol(p + 1, &endptr, 10);
        p = endptr;
        while (p < line_end && isspace((unsigned char)*p))
            p++;
    }

    if (p >= line_end)
        return GCODE_SYNTAX_ERROR; // nothing left after N.../checksum

    // Command word: <letter><digits>[.<digits>].
    if (!isalpha((unsigned char)*p))
        return GCODE_SYNTAX_ERROR;
    out->letter = (char)toupper((unsigned char)*p);
    p++;
    if (p >= line_end || !isdigit((unsigned char)*p))
        return GCODE_SYNTAX_ERROR;
    char *endptr;
    out->code = (int)strtol(p, &endptr, 10);
    p = endptr;
    if (p < line_end && *p == '.') {
        p++;
        if (p >= line_end || !isdigit((unsigned char)*p))
            return GCODE_SYNTAX_ERROR;
        out->subcode = (int)strtol(p, &endptr, 10);
        p = endptr;
    }

    const char *args_start = p;
    while (args_start < line_end && isspace((unsigned char)*args_start))
        args_start++;
    size_t args_len = (size_t)(line_end - args_start);
    if (args_len >= sizeof(out->raw_args))
        args_len = sizeof(out->raw_args) - 1;
    memcpy(out->raw_args, args_start, args_len);
    out->raw_args[args_len] = '\0';

    // Parameters: <letter><number> tokens, whitespace-separated. Stops
    // (without error) at the first token that doesn't fit that shape --
    // see the header comment for why that's a deliberate design choice,
    // not an oversight.
    p = args_start;
    while (p < line_end) {
        while (p < line_end && isspace((unsigned char)*p))
            p++;
        if (p >= line_end)
            break;
        if (!isalpha((unsigned char)*p))
            break;
        char letter = (char)toupper((unsigned char)*p);
        char *vend;
        double value = strtod(p + 1, &vend);
        if (vend == p + 1)
            break; // letter with no numeric value following
        if (out->param_count >= GCODE_MAX_PARAMS)
            return GCODE_TOO_MANY_PARAMS;
        out->params[out->param_count].letter = letter;
        out->params[out->param_count].value = value;
        out->param_count++;
        p = vend;
    }

    return GCODE_OK;
}

const struct gcode_param *
gcode_find_param(const struct gcode_command *cmd, char letter)
{
    letter = (char)toupper((unsigned char)letter);
    for (size_t i = 0; i < cmd->param_count; i++)
        if (cmd->params[i].letter == letter)
            return &cmd->params[i];
    return NULL;
}
