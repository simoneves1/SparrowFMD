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
    , [WEB_CMD_HOME] = "home", [WEB_CMD_JOG] = "jog",
};
#define COMMAND_COUNT (sizeof(COMMAND_NAMES) / sizeof(COMMAND_NAMES[0]))

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
        && cJSON_AddNumberToObject(root, "elapsed_s", s->elapsed_s) != NULL;

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

    int state_idx;
    if (cJSON_IsString(type) && strcmp(type->valuestring, "status") == 0
        && cJSON_IsString(state)
        && name_to_index(state->valuestring, STATE_NAMES, STATE_COUNT, &state_idx)
        && cJSON_IsNumber(hotend_temp) && cJSON_IsNumber(hotend_target)
        && cJSON_IsNumber(bed_temp) && cJSON_IsNumber(bed_target)
        && cJSON_IsNumber(progress) && cJSON_IsNumber(elapsed)) {
        out->state = (enum web_print_state)state_idx;
        out->hotend_temp = hotend_temp->valuedouble;
        out->hotend_target = hotend_target->valuedouble;
        out->bed_temp = bed_temp->valuedouble;
        out->bed_target = bed_target->valuedouble;
        out->progress_percent = progress->valueint;
        out->elapsed_s = (uint32_t)elapsed->valuedouble;
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
