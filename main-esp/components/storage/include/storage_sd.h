// Mounts the SD card's FAT filesystem via ESP-IDF's SDMMC peripheral
// (4-bit mode, using SDMMC_SLOT_CONFIG_DEFAULT() -- ESP-IDF's own
// per-chip default pinout, so this picks up the right pins for whatever
// chip it's built for automatically) so it's readable through standard
// POSIX file I/O (fopen/fread/fgets/...) at mount_point -- "G-code file
// read/write" per main-esp/src/README.md's storage module note doesn't
// need custom code beyond this: once mounted, gcode-parser's caller can
// just fgets() lines from a file under mount_point and feed them to
// gcode_parse_line().
//
// *** UNVERIFIED AGAINST REAL HARDWARE ***
// Same caveat as klipper-host-protocol's khp_uart_transport: this
// cross-compiles for both esp32p4 (main-esp's actual target) and
// esp32s3 (a portability check, see main-esp/README.md), and that's the
// only thing that's actually been checked. Nothing here has run against
// a real SD card or a real SDMMC peripheral. Treat it as a skeleton to
// build on and test against real hardware, not as verified working code.
#ifndef STORAGE_SD_H
#define STORAGE_SD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Mounts the SD card at mount_point (e.g. "/sdcard"). Returns false (and
// logs the ESP-IDF error) if the SDMMC peripheral or SD card init fails,
// or the FAT filesystem can't be mounted. Does not format the card on a
// failed mount -- an unformatted/corrupt card should be a loud failure,
// not something this function silently "fixes" by erasing it.
bool storage_sd_mount(const char *mount_point);

// Unmounts and releases the card. Safe to call even if mount was never
// called or already failed (a no-op in that case).
void storage_sd_unmount(const char *mount_point);

#define STORAGE_SD_MAX_FILENAME 64

struct storage_sd_file_entry {
    char name[STORAGE_SD_MAX_FILENAME]; // filename only, not full path
    uint32_t size_bytes;
};

// Lists regular files directly under dir_path whose name ends in
// ".gcode" or ".g" (case-insensitive), up to max entries, into out. Not
// recursive -- matches a typical flat print-file folder, same scope
// klipper's virtual_sdcard assumes by default. Returns the number of
// entries written; 0 if dir_path can't be opened (e.g. the SD card
// isn't mounted -- not distinguishable here from "mounted but empty",
// callers that care should check storage_sd_mount()'s own return value
// first) or genuinely has no matching files.
//
// Pure POSIX (opendir/readdir/stat) -- unlike storage_sd_mount/unmount
// above, this is host-testable against a real directory without a real
// SD card or ESP-IDF, since ESP-IDF's FATFS VFS exposes the same POSIX
// calls once mounted.
size_t storage_sd_list_gcode_files(const char *dir_path
                                   , struct storage_sd_file_entry *out
                                   , size_t max);

#endif // storage_sd.h
