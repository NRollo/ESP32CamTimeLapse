#include "sdmmc_cmd.h"
#include "esp_log.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "esp_camera.h"
#include <freertos/event_groups.h>
#include <dirent.h>
#include <string.h>
#include "esp_sleep.h"

static const char *TAG = "SDcard";

#define MAX_SPI_FREQ 13333

// GPIO mapping when using SPI mode for the SD card
#define PIN_NUM_MISO 2
#define PIN_NUM_MOSI 15
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   13

extern EventGroupHandle_t xEventGroup;
extern esp_sleep_source_t WakeUpCause;

const char *VOLUME_NAME = "/sdcard";
const uint32_t SD_OPERATION_DONE = ( 1UL << 0UL );
RTC_DATA_ATTR uint64_t num;
static sdmmc_card_t *card;
static sdmmc_host_t host;

/* Initialize the SPI bus and mount the SD card*/
esp_err_t initi_sd_card(void)
{  
    esp_err_t ret;

    host = (sdmmc_host_t) SDSPI_HOST_DEFAULT();
    host.max_freq_khz = MAX_SPI_FREQ;

    //sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return (ESP_FAIL);
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(VOLUME_NAME, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return (1);
    }
    // Set the clock freq. to the SD SPI interface (in Khz)
    host.set_card_clk(host.slot, MAX_SPI_FREQ);

    // Card has been initialized, print its properties
    //sdmmc_card_print_info(stdout, card);

    // All done
    ESP_LOGI(TAG, "Volume: '%s' mounted", VOLUME_NAME);

    return(ESP_OK);
}

/* Count the number of jpeg files on the SD card */
uint64_t NumFilesOnSDcard(void) {
    DIR *dir;
    struct dirent *dent;
    uint64_t fileNum = 0;

    // Open the '/sdcard/' dir and count the number of files having 'jpg' or 'JPG' in their filename
    dir = opendir("/sdcard/");   
    if(dir != NULL){
        while((dent = readdir(dir)) != NULL)
        {
            if((strstr(dent->d_name, "jpg") != NULL || strstr(dent->d_name, "JPG") != NULL))
            {
                //ESP_LOGI(TAG, "File: %s found",dent->d_name);
                fileNum++;
            }
        }
        closedir(dir);
    }
    else
    {
        ESP_LOGE(TAG, "opendir failed!!");
        return (0);
    }

    return (fileNum);
}

/* Save the picture frame and it's ligthing information to the SD card */
esp_err_t SavePic(const camera_fb_t *pic, int light1, char *rem) {
    char photo_name[50] = "";
    FILE *file = NULL;

    initi_sd_card();

    if (pic != NULL) {
        // Initialize the 'num' counter in case it was a SoC reset
        if (WakeUpCause != ESP_SLEEP_WAKEUP_TIMER) {
            num = NumFilesOnSDcard();
            ESP_LOGI(TAG, "Number of files found on the SD card: %lld", num);
        }

        // Create the new picture file name
        num++;
        sprintf(photo_name, "%s/%lld.jpg", VOLUME_NAME, num);

        // Open the jpeg picture file
        file = fopen(photo_name, "w");
        if (file == NULL)
        {
            ESP_LOGE(TAG, "fopen failed!!");
        }
        else
        {   // Save the picture frame and close the file
            if ((pic->buf != NULL) && (pic->len != 0)) {
                fwrite(pic->buf, 1, pic->len, file);
                fflush(file);
            }
            else {
                ESP_LOGE(TAG, "Pic buffer NULL pointer or buffer length = 0: %d", pic->len);
            }

            fclose(file);
        }

        // Log the the lighting information and comment of this picture
        file = fopen("/sdcard/Light.txt", "a");
        if (file == NULL)
        {
            ESP_LOGE(TAG, "Light.txt --> fopen failed!!");
        }
        else
        {   // Save the lighting information of this picture frame
            fprintf(file, "Pic #: %lld Light: %d %s\n", num, light1, rem);
            fflush(file);
            fclose(file);
        }
    }
    else {
        if (rem != NULL) {
            // Log the the comment
            file = fopen("/sdcard/Light.txt", "a");
            if (file == NULL)
            {
                ESP_LOGE(TAG, "Light.txt --> fopen failed!!");
            }
            else
            {   // Save the lighting information of this picture frame
                fprintf(file, "%s\n", rem);
                fflush(file);
                fclose(file);
            }
        }
    }
    // Dismount the SD card and power down the SPI bus
    esp_vfs_fat_sdcard_unmount(VOLUME_NAME, card);
    spi_bus_free(host.slot);

    // SD card operations done
    xEventGroupSetBits(xEventGroup, SD_OPERATION_DONE);
    return(ESP_OK);
}
