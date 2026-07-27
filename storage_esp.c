#include "storage_backend.h"

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>

#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#define STORAGE_MOUNT_POINT "/sdcard"
#define STORAGE_SPI_HOST SPI2_HOST
#define STORAGE_PIN_CS GPIO_NUM_47
#define STORAGE_SPI_KHZ 10000
#define STORAGE_FULL_PATH_MAX 384

static const char *TAG = "storage";
static sdmmc_card_t *s_card;
static bool s_mounted;
static const char *s_status = "SD card unavailable";

bool storage_backend_mount(void)
{
    if (s_mounted) return true;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = STORAGE_SPI_HOST;
    host.max_freq_khz = STORAGE_SPI_KHZ;

    sdspi_device_config_t device = SDSPI_DEVICE_CONFIG_DEFAULT();
    device.gpio_cs = STORAGE_PIN_CS;
    device.host_id = STORAGE_SPI_HOST;

    const esp_vfs_fat_sdmmc_mount_config_t mount = {
        .format_if_mount_failed = false,
        .max_files = 6,
        .allocation_unit_size = 16 * 1024,
    };

    esp_err_t err = esp_vfs_fat_sdspi_mount(
        STORAGE_MOUNT_POINT, &host, &device, &mount, &s_card);
    if (err != ESP_OK) {
        s_status = "SD card unavailable";
        ESP_LOGW(TAG, "SD mount failed: %s", esp_err_to_name(err));
        return false;
    }

    s_mounted = true;
    s_status = "SD card ready";
    ESP_LOGI(TAG, "SD mounted at %s (%d kHz)",
             STORAGE_MOUNT_POINT, STORAGE_SPI_KHZ);
    sdmmc_card_print_info(stdout, s_card);
    return true;
}

bool storage_backend_ready(void)
{
    return s_mounted;
}

const char *storage_backend_status(void)
{
    return s_status;
}

static bool make_full_path(const char *relative, char *full, size_t size)
{
    const int length = snprintf(
        full, size, "%s/%s", STORAGE_MOUNT_POINT, relative);
    return length >= 0 && length < (int)size;
}

int storage_backend_list(const char *relative_dir,
                         storage_backend_visitor_t visitor,
                         void *ctx)
{
    if (!s_mounted || !relative_dir || !visitor) return -1;

    char directory[STORAGE_FULL_PATH_MAX];
    if (!make_full_path(relative_dir, directory, sizeof(directory))) return -1;

    DIR *dir = opendir(directory);
    if (!dir) return -1;

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char path[STORAGE_FULL_PATH_MAX];
        const int length = snprintf(
            path, sizeof(path), "%s/%s", directory, entry->d_name);
        if (length < 0 || length >= (int)sizeof(path)) continue;

        struct stat info;
        if (stat(path, &info) != 0 || S_ISDIR(info.st_mode)) continue;
        count++;
        if (!visitor(entry->d_name, ctx)) break;
    }
    closedir(dir);
    return count;
}

FILE *storage_backend_open(const char *relative_path, const char *mode)
{
    if (!s_mounted || !relative_path || !mode) return NULL;

    char full[STORAGE_FULL_PATH_MAX];
    if (!make_full_path(relative_path, full, sizeof(full))) return NULL;
    return fopen(full, mode);
}
