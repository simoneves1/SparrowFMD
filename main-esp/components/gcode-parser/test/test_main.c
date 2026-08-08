// Host-buildable unit tests for gcode-parser.
// Build with any C compiler, no ESP-IDF required:
//   gcc -Wall -I../include -o gcode_test test_main.c ../src/gcode_parser.c
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "gcode_parser.h"

static int g_failures = 0;

#define CHECK(desc, cond) do { \
    if (cond) { \
        printf("PASS: %s\n", desc); \
    } else { \
        printf("FAIL: %s\n", desc); \
        g_failures++; \
    } \
} while (0)

static enum gcode_status
parse(const char *s, struct gcode_command *out)
{
    return gcode_parse_line(s, strlen(s), out);
}

static bool
dbl_eq(double a, double b)
{
    return fabs(a - b) < 1e-9;
}

static void
test_simple_move(void)
{
    struct gcode_command cmd;
    enum gcode_status st = parse("G1 X10.5 Y-20 F3000", &cmd);
    CHECK("simple move parses OK", st == GCODE_OK);
    CHECK("simple move: command letter/code"
         , cmd.letter == 'G' && cmd.code == 1 && cmd.subcode == -1);
    CHECK("simple move: 3 params", cmd.param_count == 3);

    const struct gcode_param *x = gcode_find_param(&cmd, 'X');
    const struct gcode_param *y = gcode_find_param(&cmd, 'Y');
    const struct gcode_param *f = gcode_find_param(&cmd, 'F');
    CHECK("simple move: X/Y/F values correct"
         , x && dbl_eq(x->value, 10.5) && y && dbl_eq(y->value, -20)
           && f && dbl_eq(f->value, 3000));
    CHECK("simple move: missing param returns NULL"
         , gcode_find_param(&cmd, 'Z') == NULL);
    CHECK("simple move: raw_args captures the full param text"
         , strcmp(cmd.raw_args, "X10.5 Y-20 F3000") == 0);
}

static void
test_lowercase_letters_normalized(void)
{
    struct gcode_command cmd;
    enum gcode_status st = parse("g1 x5 y5", &cmd);
    CHECK("lowercase command/params parse OK", st == GCODE_OK);
    CHECK("lowercase letters are normalized to uppercase"
         , cmd.letter == 'G' && gcode_find_param(&cmd, 'X') != NULL);
}

static void
test_blank_and_comment_lines(void)
{
    struct gcode_command cmd;
    CHECK("empty line is EMPTY", parse("", &cmd) == GCODE_EMPTY);
    CHECK("whitespace-only line is EMPTY", parse("   \t  ", &cmd) == GCODE_EMPTY);
    CHECK("semicolon comment-only line is EMPTY"
         , parse("; just a comment", &cmd) == GCODE_EMPTY);
    CHECK("percent line is EMPTY", parse("%", &cmd) == GCODE_EMPTY);
}

static void
test_semicolon_comment_after_command(void)
{
    struct gcode_command cmd;
    enum gcode_status st = parse("G1 X10 ; move to X10", &cmd);
    CHECK("trailing semicolon comment parses OK", st == GCODE_OK);
    CHECK("trailing semicolon comment stripped from raw_args"
         , strcmp(cmd.raw_args, "X10") == 0);
    CHECK("trailing semicolon comment doesn't affect params"
         , cmd.param_count == 1 && gcode_find_param(&cmd, 'X') != NULL);
}

static void
test_inline_paren_comment(void)
{
    struct gcode_command cmd;
    enum gcode_status st = parse("G1 X10 (linear move) Y20", &cmd);
    CHECK("inline paren comment parses OK", st == GCODE_OK);
    CHECK("inline paren comment: both params still parsed"
         , cmd.param_count == 2
           && dbl_eq(gcode_find_param(&cmd, 'X')->value, 10)
           && dbl_eq(gcode_find_param(&cmd, 'Y')->value, 20));
}

static uint8_t
xor_checksum(const char *s)
{
    uint8_t cs = 0;
    for (const char *p = s; *p; p++)
        cs ^= (uint8_t)*p;
    return cs;
}

static void
test_valid_checksum(void)
{
    const char *body = "N5 G1 X10";
    char line[64];
    snprintf(line, sizeof(line), "%s*%u", body, xor_checksum(body));

    struct gcode_command cmd;
    enum gcode_status st = parse(line, &cmd);
    CHECK("valid checksum line parses OK", st == GCODE_OK);
    CHECK("valid checksum line: line_number captured", cmd.line_number == 5);
    CHECK("valid checksum line: command/param still correct"
         , cmd.letter == 'G' && cmd.code == 1
           && dbl_eq(gcode_find_param(&cmd, 'X')->value, 10));
}

static void
test_invalid_checksum(void)
{
    struct gcode_command cmd;
    enum gcode_status st = parse("N5 G1 X10*99", &cmd);
    CHECK("invalid checksum is rejected", st == GCODE_BAD_CHECKSUM);
}

static void
test_line_number_without_command_is_syntax_error(void)
{
    struct gcode_command cmd;
    CHECK("a bare line number with no command is a syntax error"
         , parse("N5", &cmd) == GCODE_SYNTAX_ERROR);
}

static void
test_malformed_command_word(void)
{
    struct gcode_command cmd;
    CHECK("command word starting with a digit is a syntax error"
         , parse("1G0", &cmd) == GCODE_SYNTAX_ERROR);
    CHECK("command letter with no digits is a syntax error"
         , parse("G", &cmd) == GCODE_SYNTAX_ERROR);
}

static void
test_subcode(void)
{
    struct gcode_command cmd;
    enum gcode_status st = parse("G92.1", &cmd);
    CHECK("subcode command parses OK", st == GCODE_OK);
    CHECK("subcode parsed correctly"
         , cmd.letter == 'G' && cmd.code == 92 && cmd.subcode == 1);
}

static void
test_free_text_command_falls_back_to_raw_args(void)
{
    // M117 (display message) doesn't use letter=value params -- the
    // parser should stop param collection gracefully, not error out,
    // and raw_args should still have the full message.
    struct gcode_command cmd;
    enum gcode_status st = parse("M117 Hello World", &cmd);
    CHECK("M117 with free text parses OK (not a syntax error)"
         , st == GCODE_OK);
    CHECK("M117: no params collected from free text"
         , cmd.param_count == 0);
    CHECK("M117: raw_args has the full message"
         , strcmp(cmd.raw_args, "Hello World") == 0);
}

static void
test_too_many_params(void)
{
    // GCODE_MAX_PARAMS is 12; give it 13.
    char line[128] = "G1";
    for (int i = 0; i < 13; i++) {
        char tok[8];
        snprintf(tok, sizeof(tok), " A%d", i);
        strcat(line, tok);
    }
    struct gcode_command cmd;
    CHECK("more than GCODE_MAX_PARAMS tokens is rejected"
         , parse(line, &cmd) == GCODE_TOO_MANY_PARAMS);
}

static void
test_line_too_long(void)
{
    char line[GCODE_MAX_LINE + 16];
    memset(line, 'X', sizeof(line) - 1);
    line[0] = 'G';
    line[1] = '1';
    line[sizeof(line) - 1] = '\0';

    struct gcode_command cmd;
    enum gcode_status st = gcode_parse_line(line, strlen(line), &cmd);
    CHECK("a line at/over GCODE_MAX_LINE is rejected", st == GCODE_LINE_TOO_LONG);
}

int
main(void)
{
    test_simple_move();
    test_lowercase_letters_normalized();
    test_blank_and_comment_lines();
    test_semicolon_comment_after_command();
    test_inline_paren_comment();
    test_valid_checksum();
    test_invalid_checksum();
    test_line_number_without_command_is_syntax_error();
    test_malformed_command_word();
    test_subcode();
    test_free_text_command_falls_back_to_raw_args();
    test_too_many_params();
    test_line_too_long();

    printf("\n%s (%d failure%s)\n"
          , g_failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED"
          , g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
