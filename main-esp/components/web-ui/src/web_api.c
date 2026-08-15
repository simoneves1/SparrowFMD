#include <string.h>
#include "web_api.h"
#include "cJSON.h"

static const char *STATE_NAMES[] = {
    [WEB_STATE_IDLE] = "idle", [WEB_STATE_HOMING] = "homing"
    , [WEB_STATE_PRINTING] = "printing", [WEB_STATE_PAUSED] = "paused"
    , [WEB_STATE_ERROR] = "error",
};
#define STATE_COUNT (sizeof(STATE_NAMES) / sizeof(STATE_NAMES[0]))

static const char *COMMAND_NAMES[] = {
    [WEB_CMD_START] = "start", [WEB_CMD_STOP] = "stop"
    , [WEB_CMD_PAUSE] = "pause", [WEB_CMD_RESUME] = "resume"
    , [WEB_CMD_HOME] = "home", [WEB_CMD_JOG] = "jog"
    , [WEB_CMD_SET_HOTEND_TEMP] = "set_hotend_temp"
    , [WEB_CMD_SET_BED_TEMP] = "set_bed_temp"
    , [WEB_CMD_FILAMENT_RETRACT] = "filament_retract"
    , [WEB_CMD_FILAMENT_EXTRUDE] = "filament_extrude"
    , [WEB_CMD_FILAMENT_LOAD] = "filament_load"
    , [WEB_CMD_FILAMENT_UNLOAD] = "filament_unload"
    , [WEB_CMD_GCODE] = "gcode"
    , [WEB_CMD_SET_SPEED_FACTOR] = "set_speed_factor"
    , [WEB_CMD_SET_FLOW_FACTOR] = "set_flow_factor"
    , [WEB_CMD_SET_Z_OFFSET] = "set_z_offset",
};
#define COMMAND_COUNT (sizeof(COMMAND_NAMES) / sizeof(COMMAND_NAMES[0]))

// jog_dz/jog_feedrate double as filament move distance/speed for these 4
// commands (see web_api.h's top comment) -- dx/dy don't apply to them.
static bool
cmd_uses_filament_move_fields(enum web_control_cmd cmd)
{
    switch (cmd) {
    case WEB_CMD_FILAMENT_RETRACT:
    case WEB_CMD_FILAMENT_EXTRUDE:
    case WEB_CMD_FILAMENT_LOAD:
    case WEB_CMD_FILAMENT_UNLOAD:
        return true;
    default:
        return false;
    }
}

static bool
cmd_is_temp_set(enum web_control_cmd cmd)
{
    return cmd == WEB_CMD_SET_HOTEND_TEMP || cmd == WEB_CMD_SET_BED_TEMP;
}

// "value" doubles as speed factor %, flow factor %, or Z-offset mm for
// these 3 Tune-panel commands (see web_api.h's top comment).
static bool
cmd_is_tune_value(enum web_control_cmd cmd)
{
    return cmd == WEB_CMD_SET_SPEED_FACTOR || cmd == WEB_CMD_SET_FLOW_FACTOR
        || cmd == WEB_CMD_SET_Z_OFFSET;
}

static bool
name_to_index(const char *name, const char *const *table, size_t count
             , int *out)
{
    for (size_t i = 0; i < count; i++) {
        if (table[i] && strcmp(table[i], name) == 0) {
            *out = (int)i;
            return true;
        }
    }
    return false;
}

enum web_msg_type
web_msg_type_of(const char *json, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root)
        return WEB_MSG_UNKNOWN;

    enum web_msg_type result = WEB_MSG_UNKNOWN;
    cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (cJSON_IsString(type)) {
        if (strcmp(type->valuestring, "status") == 0)
            result = WEB_MSG_STATUS;
        else if (strcmp(type->valuestring, "command") == 0)
            result = WEB_MSG_COMMAND;
        else if (strcmp(type->valuestring, "console_log") == 0)
            result = WEB_MSG_CONSOLE_LOG;
    }
    cJSON_Delete(root);
    return result;
}

char *
web_status_to_json(const struct web_status *s)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    bool ok = cJSON_AddStringToObject(root, "type", "status") != NULL
        && cJSON_AddStringToObject(root, "state", STATE_NAMES[s->state]) != NULL
        && cJSON_AddNumberToObject(root, "hotend_temp", s->hotend_temp) != NULL
        && cJSON_AddNumberToObject(root, "hotend_target", s->hotend_target) != NULL
        && cJSON_AddNumberToObject(root, "bed_temp", s->bed_temp) != NULL
        && cJSON_AddNumberToObject(root, "bed_target", s->bed_target) != NULL
        && cJSON_AddNumberToObject(root, "progress", s->progress_percent) != NULL
        && cJSON_AddNumberToObject(root, "elapsed_s", s->elapsed_s) != NULL
        && cJSON_AddStringToObject(root, "filename", s->filename) != NULL
        && cJSON_AddNumberToObject(root, "layer_current", s->layer_current) != NULL
        && cJSON_AddNumberToObject(root, "layer_total", s->layer_total) != NULL
        && cJSON_AddNumberToObject(root, "remaining_s", s->remaining_s) != NULL
        && cJSON_AddNumberToObject(root, "speed_factor", s->speed_factor) != NULL
        && cJSON_AddNumberToObject(root, "flow_factor", s->flow_factor) != NULL
        && cJSON_AddNumberToObject(root, "z_offset", s->z_offset) != NULL;

    char *out = ok ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    return out;
}

bool
web_status_from_json(const char *json, size_t len, struct web_status *out)
{
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root)
        return false;

    bool ok = false;
    cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    cJSON *state = cJSON_GetObjectItemCaseSensitive(root, "state");
    cJSON *hotend_temp = cJSON_GetObjectItemCaseSensitive(root, "hotend_temp");
    cJSON *hotend_target = cJSON_GetObjectItemCaseSensitive(root, "hotend_target");
    cJSON *bed_temp = cJSON_GetObjectItemCaseSensitive(root, "bed_temp");
    cJSON *bed_target = cJSON_GetObjectItemCaseSensitive(root, "bed_target");
    cJSON *progress = cJSON_GetObjectItemCaseSensitive(root, "progress");
    cJSON *elapsed = cJSON_GetObjectItemCaseSensitive(root, "elapsed_s");
    cJSON *filename = cJSON_GetObjectItemCaseSensitive(root, "filename");
    cJSON *layer_current = cJSON_GetObjectItemCaseSensitive(root, "layer_current");
    cJSON *layer_total = cJSON_GetObjectItemCaseSensitive(root, "layer_total");
    cJSON *remaining = cJSON_GetObjectItemCaseSensitive(root, "remaining_s");
    cJSON *speed_factor = cJSON_GetObjectItemCaseSensitive(root, "speed_factor");
    cJSON *flow_factor = cJSON_GetObjectItemCaseSensitive(root, "flow_factor");
    cJSON *z_offset = cJSON_GetObjectItemCaseSensitive(root, "z_offset");

    int state_idx;
    if (cJSON_IsString(type) && strcmp(type->valuestring, "status") == 0
        && cJSON_IsString(state)
        && name_to_index(state->valuestring, STATE_NAMES, STATE_COUNT, &state_idx)
        && cJSON_IsNumber(hotend_temp) && cJSON_IsNumber(hotend_target)
        && cJSON_IsNumber(bed_temp) && cJSON_IsNumber(bed_target)
        && cJSON_IsNumber(progress) && cJSON_IsNumber(elapsed)
        && cJSON_IsString(filename) && cJSON_IsNumber(layer_current)
        && cJSON_IsNumber(layer_total) && cJSON_IsNumber(remaining)
        && cJSON_IsNumber(speed_factor) && cJSON_IsNumber(flow_factor)
        && cJSON_IsNumber(z_offset)) {
        out->state = (enum web_print_state)state_idx;
        out->hotend_temp = hotend_temp->valuedouble;
        out->hotend_target = hotend_target->valuedouble;
        out->bed_temp = bed_temp->valuedouble;
        out->bed_target = bed_target->valuedouble;
        out->progress_percent = progress->valueint;
        out->elapsed_s = (uint32_t)elapsed->valuedouble;
        strncpy(out->filename, filename->valuestring, WEB_STATUS_FILENAME_LEN - 1);
        out->filename[WEB_STATUS_FILENAME_LEN - 1] = '\0';
        out->layer_current = layer_current->valueint;
        out->layer_total = layer_total->valueint;
        out->remaining_s = (uint32_t)remaining->valuedouble;
        out->speed_factor = speed_factor->valuedouble;
        out->flow_factor = flow_factor->valuedouble;
        out->z_offset = z_offset->valuedouble;
        ok = true;
    }

    cJSON_Delete(root);
    return ok;
}

char *
web_control_command_to_json(const struct web_control_command *c)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    bool ok = cJSON_AddStringToObject(root, "type", "command") != NULL
        && cJSON_AddStringToObject(root, "command"
                                   , COMMAND_NAMES[c->command]) != NULL;
    if (ok && c->command == WEB_CMD_JOG) {
        ok = cJSON_AddNumberToObject(root, "jog_dx", c->jog_dx) != NULL
            && cJSON_AddNumberToObject(root, "jog_dy", c->jog_dy) != NULL
            && cJSON_AddNumberToObject(root, "jog_dz", c->jog_dz) != NULL
            && cJSON_AddNumberToObject(root, "jog_feedrate"
                                       , c->jog_feedrate) != NULL;
    } else if (ok && cmd_uses_filament_move_fields(c->command)) {
        ok = cJSON_AddNumberToObject(root, "jog_dz", c->jog_dz) != NULL
            && cJSON_AddNumberToObject(root, "jog_feedrate"
                                       , c->jog_feedrate) != NULL;
    } else if (ok && cmd_is_temp_set(c->command)) {
        ok = cJSON_AddNumberToObject(root, "target_temp"
                                     , c->target_temp) != NULL;
    } else if (ok && c->command == WEB_CMD_START && c->filename[0] != '\0') {
        // Omitted entirely (not just empty-string) when no filename was
        // given, so a plain "resume whatever's loaded" start looks
        // exactly like it always has on the wire.
        ok = cJSON_AddStringToObject(root, "filename", c->filename) != NULL;
    } else if (ok && c->command == WEB_CMD_GCODE) {
        ok = cJSON_AddStringToObject(root, "gcode", c->gcode) != NULL;
    } else if (ok && cmd_is_tune_value(c->command)) {
        ok = cJSON_AddNumberToObject(root, "value", c->value) != NULL;
    }

    char *out = ok ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    return out;
}

bool
web_control_command_from_json(const char *json, size_t len
                              , struct web_control_command *out)
{
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root)
        return false;

    bool ok = false;
    cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    cJSON *command = cJSON_GetObjectItemCaseSensitive(root, "command");

    int cmd_idx;
    if (cJSON_IsString(type) && strcmp(type->valuestring, "command") == 0
        && cJSON_IsString(command)
        && name_to_index(command->valuestring, COMMAND_NAMES, COMMAND_COUNT
                         , &cmd_idx)) {
        memset(out, 0, sizeof(*out));
        out->command = (enum web_control_cmd)cmd_idx;
        if (out->command == WEB_CMD_JOG) {
            cJSON *dx = cJSON_GetObjectItemCaseSensitive(root, "jog_dx");
            cJSON *dy = cJSON_GetObjectItemCaseSensitive(root, "jog_dy");
            cJSON *dz = cJSON_GetObjectItemCaseSensitive(root, "jog_dz");
            cJSON *fr = cJSON_GetObjectItemCaseSensitive(root, "jog_feedrate");
            if (cJSON_IsNumber(dx) && cJSON_IsNumber(dy)
                && cJSON_IsNumber(dz) && cJSON_IsNumber(fr)) {
                out->jog_dx = dx->valuedouble;
                out->jog_dy = dy->valuedouble;
                out->jog_dz = dz->valuedouble;
                out->jog_feedrate = fr->valuedouble;
                ok = true;
            }
        } else if (cmd_uses_filament_move_fields(out->command)) {
            cJSON *dz = cJSON_GetObjectItemCaseSensitive(root, "jog_dz");
            cJSON *fr = cJSON_GetObjectItemCaseSensitive(root, "jog_feedrate");
            if (cJSON_IsNumber(dz) && cJSON_IsNumber(fr)) {
                out->jog_dz = dz->valuedouble;
                out->jog_feedrate = fr->valuedouble;
                ok = true;
            }
        } else if (cmd_is_temp_set(out->command)) {
            cJSON *temp = cJSON_GetObjectItemCaseSensitive(root, "target_temp");
            if (cJSON_IsNumber(temp)) {
                out->target_temp = temp->valuedouble;
                ok = true;
            }
        } else if (out->command == WEB_CMD_START) {
            // filename is optional -- out->filename is already "" from
            // the memset above if it's absent.
            cJSON *fname = cJSON_GetObjectItemCaseSensitive(root, "filename");
            if (fname == NULL) {
                ok = true;
            } else if (cJSON_IsString(fname)) {
                strncpy(out->filename, fname->valuestring, WEB_FILENAME_LEN - 1);
                out->filename[WEB_FILENAME_LEN - 1] = '\0';
                ok = true;
            }
        } else if (out->command == WEB_CMD_GCODE) {
            cJSON *gcode = cJSON_GetObjectItemCaseSensitive(root, "gcode");
            if (cJSON_IsString(gcode) && gcode->valuestring[0] != '\0') {
                strncpy(out->gcode, gcode->valuestring, WEB_GCODE_LINE_LEN - 1);
                out->gcode[WEB_GCODE_LINE_LEN - 1] = '\0';
                ok = true;
            }
        } else if (cmd_is_tune_value(out->command)) {
            cJSON *value = cJSON_GetObjectItemCaseSensitive(root, "value");
            if (cJSON_IsNumber(value)) {
                out->value = value->valuedouble;
                ok = true;
            }
        } else {
            ok = true;
        }
    }

    cJSON_Delete(root);
    return ok;
}

char *
web_console_log_to_json(const struct web_console_log *c)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    bool ok = cJSON_AddStringToObject(root, "type", "console_log") != NULL
        && cJSON_AddStringToObject(root, "line", c->line) != NULL;

    char *out = ok ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    return out;
}

bool
web_console_log_from_json(const char *json, size_t len
                          , struct web_console_log *out)
{
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root)
        return false;

    bool ok = false;
    cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    cJSON *line = cJSON_GetObjectItemCaseSensitive(root, "line");
    if (cJSON_IsString(type) && strcmp(type->valuestring, "console_log") == 0
        && cJSON_IsString(line)) {
        strncpy(out->line, line->valuestring, WEB_CONSOLE_LOG_LINE_LEN - 1);
        out->line[WEB_CONSOLE_LOG_LINE_LEN - 1] = '\0';
        ok = true;
    }

    cJSON_Delete(root);
    return ok;
}

char *
web_file_list_to_json(const struct web_file_entry *files, size_t count)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    cJSON *arr = cJSON_AddArrayToObject(root, "files");
    bool ok = arr != NULL;
    for (size_t i = 0; ok && i < count; i++) {
        cJSON *entry = cJSON_CreateObject();
        ok = entry != NULL
            && cJSON_AddStringToObject(entry, "name", files[i].name) != NULL
            && cJSON_AddNumberToObject(entry, "size_bytes"
                                       , files[i].size_bytes) != NULL
            && cJSON_AddNumberToObject(entry, "print_time_s"
                                       , files[i].print_time_s) != NULL;
        if (ok)
            cJSON_AddItemToArray(arr, entry);
        else if (entry)
            cJSON_Delete(entry);
    }

    char *out = ok ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    return out;
}

static const char *CAMERA_MODE_NAMES[] = {
    [WEB_CAMERA_NONE] = "none", [WEB_CAMERA_URL] = "url"
    , [WEB_CAMERA_LOCAL] = "local",
};

char *
web_camera_config_to_json(const struct web_camera_config *c)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    bool ok = cJSON_AddStringToObject(root, "mode"
                                      , CAMERA_MODE_NAMES[c->mode]) != NULL;
    if (ok && c->mode == WEB_CAMERA_URL)
        ok = cJSON_AddStringToObject(root, "url", c->url) != NULL;

    char *out = ok ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    return out;
}

#define CAMERA_MODE_COUNT (sizeof(CAMERA_MODE_NAMES) / sizeof(CAMERA_MODE_NAMES[0]))

bool
web_camera_config_from_json(const char *json, size_t len
                            , struct web_camera_config *out)
{
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root)
        return false;

    bool ok = false;
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(root, "mode");
    int mode_idx;
    if (cJSON_IsString(mode)
        && name_to_index(mode->valuestring, CAMERA_MODE_NAMES
                         , CAMERA_MODE_COUNT, &mode_idx)) {
        memset(out, 0, sizeof(*out));
        out->mode = (enum web_camera_mode)mode_idx;
        if (out->mode == WEB_CAMERA_URL) {
            cJSON *url = cJSON_GetObjectItemCaseSensitive(root, "url");
            if (cJSON_IsString(url) && url->valuestring[0] != '\0') {
                strncpy(out->url, url->valuestring, sizeof(out->url) - 1);
                out->url[sizeof(out->url) - 1] = '\0';
                ok = true;
            }
        } else {
            ok = true;
        }
    }

    cJSON_Delete(root);
    return ok;
}

void
web_json_free(char *s)
{
    cJSON_free(s);
}
