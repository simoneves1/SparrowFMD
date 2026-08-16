// See storage_sd.h -- storage_sd_list_gcode_files() only. Split into its
// own file (separate from storage_sd.c's mount/unmount) because this
// part is pure POSIX and can be host-tested without ESP-IDF, unlike the
// SDMMC/FATFS mount code.
#include "storage_sd.h"

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

static bool
has_gcode_extension(const char *name)
{
    size_t len = strlen(name);
    static const char *exts[] = {".gcode", ".g"};
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
        size_t ext_len = strlen(exts[i]);
        if (len <= ext_len)
            continue;
        const char *suffix = name + (len - ext_len);
        size_t j;
        for (j = 0; j < ext_len; j++) {
            char a = suffix[j], b = exts[i][j];
            if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
            if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
            if (a != b)
                break;
        }
        if (j == ext_len)
            return true;
    }
    return false;
}

size_t
storage_sd_list_gcode_files(const char *dir_path
                            , struct storage_sd_file_entry *out, size_t max)
{
    DIR *dir = opendir(dir_path);
    if (!dir)
        return 0;

    size_t n = 0;
    struct dirent *ent;
    while (n < max && (ent = readdir(dir)) != NULL) {
        if (!has_gcode_extension(ent->d_name))
            continue;

        char path[512];
        int r = snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name);
        if (r < 0 || (size_t)r >= sizeof(path))
            continue; // path too long, skip rather than truncate-and-stat wrong file

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue; // subdirectories and stat failures are silently skipped

        strncpy(out[n].name, ent->d_name, STORAGE_SD_MAX_FILENAME - 1);
        out[n].name[STORAGE_SD_MAX_FILENAME - 1] = '\0';
        out[n].size_bytes = (uint32_t)st.st_size;
        n++;
    }

    closedir(dir);
    return n;
}
