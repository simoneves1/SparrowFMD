// Decompresses a completed identify handshake's raw byte buffer (zlib
// format: 2-byte header + DEFLATE stream + 4-byte big-endian Adler-32)
// into the JSON text of the MCU's data dictionary.
//
// Parsing that JSON into a queryable command/response/config table is
// its own separate piece of work, not covered here -- this module's job
// ends at "here is the decompressed, checksum-verified JSON text."
#ifndef KHP_DICTIONARY_H
#define KHP_DICTIONARY_H

#include <stdint.h>
#include <stddef.h>

enum khp_dictionary_status {
    KHP_DICT_OK,
    KHP_DICT_BAD_ZLIB_HEADER,  // not a valid zlib stream, or uses a preset
                               // dictionary (RFC1950 FDICT flag), which
                               // Klipper never does and we don't support
    KHP_DICT_INFLATE_ERROR,    // puff() rejected the DEFLATE data itself
    KHP_DICT_ADLER_MISMATCH,   // inflate succeeded but the trailing
                               // Adler-32 doesn't match -- treated as a
                               // hard failure, not a warning, since it
                               // means either transit corruption slipped
                               // past the message-level CRCs or there's
                               // a bug in this decoder or the caller's
                               // chunk reassembly
    KHP_DICT_OOM,
};

struct khp_dictionary {
    uint8_t *json;    // null-terminated for convenience; json_len excludes
                       // that trailing NUL, same convention as strlen()
    size_t json_len;
};

enum khp_dictionary_status khp_dictionary_inflate(struct khp_dictionary *out
                                                   , const uint8_t *zlib_data
                                                   , size_t zlib_len);
void khp_dictionary_free(struct khp_dictionary *d);

#endif // khp_dictionary.h
