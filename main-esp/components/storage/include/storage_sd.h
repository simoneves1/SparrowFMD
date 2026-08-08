// Mounts the SD card's FAT filesystem via ESP-IDF's SDMMC peripheral
// (4-bit mode, esp32p4's default SDMMC slot pinout) so it's readable
// through standard POSIX file I/O (fopen/fread/fgets/...) at
// mount_point -- "G-code file read/write" per main-esp/src/README.md's
// storage module note doesn't need custom code beyond this: once
// mounted, gcode-parser's caller can just fgets() lines from a file
// under mount_point and feed them to gcode_parse_line().
//
// *** UNVERIFIED AGAINST REAL HARDWARE ***
// Same caveat as klipper-host-protocol's khp_uart_transport: this
// cross-compiles for esp32p4, and that's the only thing that's actually
// been checked. Nothing here has run against a real SD card or a real
// SDMMC peripheral. Treat it as a skeleton to build on and test against
// real hardware, not as verified working code.
#ifndef STORAGE_SD_H
#define STORAGE_SD_H

#include <stdbool.h>

// Mounts the SD card at mount_point (e.g. "/sdcard"). Returns false (and
// logs the ESP-IDF error) if the SDMMC peripheral or SD card init fails,
// or the FAT filesystem can't be mounted. Does not format the card on a
// failed mount -- an unformatted/corrupt card should be a loud failure,
// not something this function silently "fixes" by erasing it.
bool storage_sd_mount(const char *mount_point);

// Unmounts and releases the card. Safe to call even if mount was never
// called or already failed (a no-op in that case).
void storage_sd_unmount(const char *mount_point);

#endif // storage_sd.h
