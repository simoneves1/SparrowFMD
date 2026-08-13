#include <string.h>
#include "slp_messages.h"

static void
put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static uint16_t
get_u16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void
put_i16(uint8_t *p, int16_t v)
{
    put_u16(p, (uint16_t)v);
}

static int16_t
get_i16(const uint8_t *p)
{
    return (int16_t)get_u16(p);
}

static void
put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t
get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

size_t
slp_status_update_encode(uint8_t *out, const struct slp_status_update *s)
{
    uint8_t *p = out;
    *p++ = (uint8_t)s->state;
    put_i16(p, s->hotend_temp_c_x100); p += 2;
    put_i16(p, s->hotend_target_c_x100); p += 2;
    put_i16(p, s->bed_temp_c_x100); p += 2;
    put_i16(p, s->bed_target_c_x100); p += 2;
    *p++ = s->progress_percent;
    put_u32(p, s->elapsed_s); p += 4;
    // Copy+truncate, always null-terminate within the fixed field width.
    size_t name_len = strlen(s->filename);
    if (name_len > SLP_STATUS_UPDATE_FILENAME_LEN - 1)
        name_len = SLP_STATUS_UPDATE_FILENAME_LEN - 1;
    memcpy(p, s->filename, name_len);
    memset(p + name_len, 0, SLP_STATUS_UPDATE_FILENAME_LEN - name_len);
    p += SLP_STATUS_UPDATE_FILENAME_LEN;
    put_u16(p, s->layer_current); p += 2;
    put_u16(p, s->layer_total); p += 2;
    put_u32(p, s->remaining_s); p += 4;
    return (size_t)(p - out);
}

bool
slp_status_update_decode(const uint8_t *payload, size_t payload_len
                         , struct slp_status_update *out)
{
    if (payload_len != SLP_STATUS_UPDATE_WIRE_SIZE)
        return false;
    const uint8_t *p = payload;
    out->state = (enum slp_print_state)*p++;
    out->hotend_temp_c_x100 = get_i16(p); p += 2;
    out->hotend_target_c_x100 = get_i16(p); p += 2;
    out->bed_temp_c_x100 = get_i16(p); p += 2;
    out->bed_target_c_x100 = get_i16(p); p += 2;
    out->progress_percent = *p++;
    out->elapsed_s = get_u32(p); p += 4;
    memcpy(out->filename, p, SLP_STATUS_UPDATE_FILENAME_LEN);
    out->filename[SLP_STATUS_UPDATE_FILENAME_LEN - 1] = '\0';
    p += SLP_STATUS_UPDATE_FILENAME_LEN;
    out->layer_current = get_u16(p); p += 2;
    out->layer_total = get_u16(p); p += 2;
    out->remaining_s = get_u32(p); p += 4;
    return true;
}

size_t
slp_control_command_encode(uint8_t *out, const struct slp_control_command *c)
{
    uint8_t *p = out;
    *p++ = (uint8_t)c->command;
    put_i16(p, c->jog_dx_mm_x100); p += 2;
    put_i16(p, c->jog_dy_mm_x100); p += 2;
    put_i16(p, c->jog_dz_mm_x100); p += 2;
    put_u16(p, c->jog_feedrate_mm_min); p += 2;
    put_i16(p, c->target_temp_c_x100); p += 2;
    return (size_t)(p - out);
}

bool
slp_control_command_decode(const uint8_t *payload, size_t payload_len
                           , struct slp_control_command *out)
{
    if (payload_len != SLP_CONTROL_COMMAND_WIRE_SIZE)
        return false;
    const uint8_t *p = payload;
    out->command = (enum slp_control_cmd)*p++;
    out->jog_dx_mm_x100 = get_i16(p); p += 2;
    out->jog_dy_mm_x100 = get_i16(p); p += 2;
    out->jog_dz_mm_x100 = get_i16(p); p += 2;
    out->jog_feedrate_mm_min = get_u16(p); p += 2;
    out->target_temp_c_x100 = get_i16(p); p += 2;
    return true;
}
