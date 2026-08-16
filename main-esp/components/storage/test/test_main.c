// Host-buildable unit tests for storage's cfg_parser and
// storage_sd_list_gcode_files (storage_sd_mount/unmount are ESP-IDF-only
// and deliberately excluded here, see storage_sd.h's own note).
// Build with any C compiler, no ESP-IDF required:
//   gcc -Wall -I../include -o storage_test test_main.c
//     ../src/cfg_parser.c ../src/storage_sd_list.c
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "cfg_parser.h"
#include "storage_sd.h"

// mkdir()'s signature differs between platforms -- confined to this test
// file's own fixture setup, storage_sd_list.c itself never creates
// directories.
#ifdef _WIN32
#include <direct.h>
#define TEST_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define TEST_MKDIR(path) mkdir(path, 0755)
#endif

static int g_failures = 0;

#define CHECK(desc, cond) do { \
    if (cond) { \
        printf("PASS: %s\n", desc); \
    } else { \
        printf("FAIL: %s\n", desc); \
        g_failures++; \
    } \
} while (0)

static enum cfg_status
parse(const char *text, struct cfg_file *out)
{
    return cfg_parse(text, strlen(text), out);
}

static bool
dbl_eq(double a, double b)
{
    return fabs(a - b) < 1e-9;
}

static void
test_basic_multi_section(void)
{
    const char *text =
        "[board]\n"
        "chip: esp32p4\n"
        "\n"
        "[stepper_x]\n"
        "step_pin = 12\n"
        "microsteps: 16\n"
        "rotation_distance = 40.0\n";

    struct cfg_file cfg;
    enum cfg_status st = parse(text, &cfg);
    CHECK("basic multi-section config parses OK", st == CFG_OK);
    CHECK("basic multi-section config: 2 sections", cfg.section_count == 2);

    const struct cfg_section *board = cfg_find_section(&cfg, "board");
    const struct cfg_section *stepper = cfg_find_section(&cfg, "stepper_x");
    CHECK("both sections found", board != NULL && stepper != NULL);
    CHECK("colon delimiter parses", strcmp(cfg_get(board, "chip"), "esp32p4") == 0);
    CHECK("equals delimiter parses (step_pin)"
         , strcmp(cfg_get(stepper, "step_pin"), "12") == 0);
    CHECK("colon delimiter parses (microsteps)"
         , strcmp(cfg_get(stepper, "microsteps"), "16") == 0);
    CHECK("unknown section is not found", cfg_find_section(&cfg, "nope") == NULL);
    CHECK("unknown key returns NULL", cfg_get(board, "nope") == NULL);
}

static void
test_comments_and_blank_lines_ignored(void)
{
    const char *text =
        "# a full-line hash comment\n"
        "; a full-line semicolon comment\n"
        "\n"
        "[section]\n"
        "  \n"
        "key: value  # trailing comment\n"
        "key2: value2  ; trailing comment\n";

    struct cfg_file cfg;
    enum cfg_status st = parse(text, &cfg);
    CHECK("comments/blank lines parse OK", st == CFG_OK);
    const struct cfg_section *sec = cfg_find_section(&cfg, "section");
    CHECK("section found despite surrounding comments", sec != NULL);
    CHECK("trailing hash comment stripped from value"
         , sec && strcmp(cfg_get(sec, "key"), "value") == 0);
    CHECK("trailing semicolon comment stripped from value"
         , sec && strcmp(cfg_get(sec, "key2"), "value2") == 0);
}

static void
test_get_default(void)
{
    struct cfg_file cfg;
    parse("[s]\nfoo: bar\n", &cfg);
    const struct cfg_section *sec = cfg_find_section(&cfg, "s");
    CHECK("get_default returns the real value when present"
         , strcmp(cfg_get_default(sec, "foo", "fallback"), "bar") == 0);
    CHECK("get_default returns the fallback when absent"
         , strcmp(cfg_get_default(sec, "missing", "fallback"), "fallback") == 0);
    CHECK("cfg_get/cfg_get_default on a NULL section don't crash"
         , cfg_get(NULL, "x") == NULL
           && strcmp(cfg_get_default(NULL, "x", "d"), "d") == 0);
}

static void
test_typed_accessors(void)
{
    struct cfg_file cfg;
    parse("[extruder]\nmax_temp: 260\nrotation_distance: 22.6789\n"
          "garbage: not_a_number\n", &cfg);
    const struct cfg_section *sec = cfg_find_section(&cfg, "extruder");

    long l;
    CHECK("cfg_get_long parses an integer value"
         , cfg_get_long(sec, "max_temp", &l) && l == 260);
    double d;
    CHECK("cfg_get_double parses a float value"
         , cfg_get_double(sec, "rotation_distance", &d) && dbl_eq(d, 22.6789));
    CHECK("cfg_get_long rejects a non-numeric value"
         , !cfg_get_long(sec, "garbage", &l));
    CHECK("cfg_get_long returns false for a missing key"
         , !cfg_get_long(sec, "does_not_exist", &l));
}

static void
test_key_value_before_any_section_is_syntax_error(void)
{
    struct cfg_file cfg;
    CHECK("key=value before any [section] is a syntax error"
         , parse("foo: bar\n[section]\n", &cfg) == CFG_SYNTAX_ERROR);
}

static void
test_malformed_section_header(void)
{
    struct cfg_file cfg;
    CHECK("section header missing closing bracket is a syntax error"
         , parse("[section\nfoo: bar\n", &cfg) == CFG_SYNTAX_ERROR);
}

static void
test_line_with_no_delimiter_is_syntax_error(void)
{
    struct cfg_file cfg;
    CHECK("a non-comment line with neither '[' nor a delimiter is a syntax error"
         , parse("[s]\nthis is not valid\n", &cfg) == CFG_SYNTAX_ERROR);
}

static void
test_too_many_sections(void)
{
    char text[4096] = "";
    for (int i = 0; i < CFG_MAX_SECTIONS + 1; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "[s%d]\nk: v\n", i);
        strcat(text, buf);
    }
    struct cfg_file cfg;
    CHECK("exceeding CFG_MAX_SECTIONS is rejected"
         , parse(text, &cfg) == CFG_TOO_MANY_SECTIONS);
}

static void
test_too_many_entries(void)
{
    char text[4096] = "[s]\n";
    for (int i = 0; i < CFG_MAX_ENTRIES_PER_SECTION + 1; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "k%d: v\n", i);
        strcat(text, buf);
    }
    struct cfg_file cfg;
    CHECK("exceeding CFG_MAX_ENTRIES_PER_SECTION is rejected"
         , parse(text, &cfg) == CFG_TOO_MANY_ENTRIES);
}

static void
test_line_too_long(void)
{
    char text[CFG_MAX_LINE_LEN + 32];
    memset(text, 'a', sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
    struct cfg_file cfg;
    CHECK("a line at/over CFG_MAX_LINE_LEN is rejected"
         , cfg_parse(text, strlen(text), &cfg) == CFG_LINE_TOO_LONG);
}

static void
test_crlf_tolerated(void)
{
    struct cfg_file cfg;
    enum cfg_status st = parse("[s]\r\nkey: value\r\n", &cfg);
    CHECK("CRLF line endings are tolerated", st == CFG_OK);
    const struct cfg_section *sec = cfg_find_section(&cfg, "s");
    CHECK("CRLF: value doesn't retain a trailing \\r"
         , sec && strcmp(cfg_get(sec, "key"), "value") == 0);
}

static void
test_whitespace_trimmed(void)
{
    struct cfg_file cfg;
    parse("[  s  ]\n   key   :   value with spaces   \n", &cfg);
    // Section name trimming: our implementation takes exactly what's
    // between '[' and ']' after the whole line was already trimmed --
    // confirm the practical behavior rather than assume it.
    const struct cfg_section *sec = cfg.section_count == 1
        ? &cfg.sections[0] : NULL;
    CHECK("whitespace around key/value is trimmed"
         , sec && strcmp(cfg_get(sec, "key"), "value with spaces") == 0);
}

#define FIXTURE_DIR "storage_sd_list_test_fixture"

static void
write_fixture_file(const char *name, const char *contents)
{
    char path[256];
    snprintf(path, sizeof(path), FIXTURE_DIR "/%s", name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        printf("FAIL: could not create fixture file %s\n", path);
        g_failures++;
        return;
    }
    fwrite(contents, 1, strlen(contents), f);
    fclose(f);
}

static bool
find_entry(const struct storage_sd_file_entry *entries, size_t count
          , const char *name)
{
    for (size_t i = 0; i < count; i++)
        if (strcmp(entries[i].name, name) == 0)
            return true;
    return false;
}

static void
test_storage_sd_list_gcode_files(void)
{
    TEST_MKDIR(FIXTURE_DIR);
    TEST_MKDIR(FIXTURE_DIR "/subdir"); // must be skipped, not a regular file

    write_fixture_file("benchy.gcode", "; a gcode file\nG28\n");
    write_fixture_file("CALIBRATION.G", "G28\n"); // uppercase extension
    write_fixture_file("notes.txt", "not a gcode file");
    write_fixture_file("no_extension", "also not a gcode file");

    struct storage_sd_file_entry entries[16];
    size_t n = storage_sd_list_gcode_files(FIXTURE_DIR, entries, 16);

    CHECK("lists exactly the 2 gcode files, ignoring non-gcode and subdirs"
         , n == 2);
    CHECK("finds the lowercase .gcode file", find_entry(entries, n, "benchy.gcode"));
    CHECK("finds the file with an uppercase .G extension"
         , find_entry(entries, n, "CALIBRATION.G"));
    CHECK("does not list the .txt file", !find_entry(entries, n, "notes.txt"));
    CHECK("does not list the extensionless file"
         , !find_entry(entries, n, "no_extension"));

    for (size_t i = 0; i < n; i++) {
        if (strcmp(entries[i].name, "benchy.gcode") == 0)
            CHECK("benchy.gcode's reported size matches its real contents"
                 , entries[i].size_bytes == strlen("; a gcode file\nG28\n"));
    }
}

static void
test_storage_sd_list_gcode_files_respects_max(void)
{
    struct storage_sd_file_entry entries[1];
    size_t n = storage_sd_list_gcode_files(FIXTURE_DIR, entries, 1);
    CHECK("a max of 1 never returns more than 1 entry even though 2 exist"
         , n == 1);
}

static void
test_storage_sd_list_gcode_files_missing_dir(void)
{
    struct storage_sd_file_entry entries[16];
    size_t n = storage_sd_list_gcode_files("no_such_directory_at_all"
                                           , entries, 16);
    CHECK("a directory that can't be opened returns 0, not a crash", n == 0);
}

int
main(void)
{
    test_basic_multi_section();
    test_comments_and_blank_lines_ignored();
    test_get_default();
    test_typed_accessors();
    test_key_value_before_any_section_is_syntax_error();
    test_malformed_section_header();
    test_line_with_no_delimiter_is_syntax_error();
    test_too_many_sections();
    test_too_many_entries();
    test_line_too_long();
    test_crlf_tolerated();
    test_whitespace_trimmed();
    test_storage_sd_list_gcode_files();
    test_storage_sd_list_gcode_files_respects_max();
    test_storage_sd_list_gcode_files_missing_dir();

    printf("\n%s (%d failure%s)\n"
          , g_failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED"
          , g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
