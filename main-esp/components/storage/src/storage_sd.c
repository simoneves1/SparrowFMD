// See storage_sd.h -- unverified against real hardware.
#include "storage_sd.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_default_configs.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"

static const char *TAG = "storage_sd";
static sdmmc_card_t *s_card = NULL;

bool
storage_sd_mount(const char *mount_point)
{
    if (s_card) {
        ESP_LOGW(TAG, "storage_sd_mount called while already mounted");
        return true;
    }

    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    esp_err_t err = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config
                                            , &mount_config, &s_card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD mount at %s failed: %s", mount_point
                , esp_err_to_name(err));
        s_card = NULL;
        return false;
    }
    return true;
}

void
storage_sd_unmount(const char *mount_point)
{
    if (!s_card)
        return;
    esp_vfs_fat_sdcard_unmount(mount_point, s_card);
    s_card = NULL;
}
