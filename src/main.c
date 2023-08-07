#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/event_groups.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_camera.h"
#include "driver/private_include/xclk.h"
#include "string.h"
#include "driver/rtc_io.h"

static const char *TAG = "Main";


extern void take_photo(void);
extern RTC_DATA_ATTR struct timeval sleep_enter_time;
extern RTC_DATA_ATTR bool DayOperation;
extern RTC_DATA_ATTR bool NightOperation;
extern esp_err_t SavePic(const camera_fb_t *pic, int light1, char *rem);

void GetResetCauseInText(esp_reset_reason_t res, char *returnStr) {
    switch (res)
    {
        case ESP_RST_UNKNOWN:
            strcpy(returnStr, "Reset reason can not be determined");
            break;
        case ESP_RST_POWERON:
            strcpy(returnStr, "Reset due to power-on event");
            break;
        case ESP_RST_EXT:
            strcpy(returnStr, "Reset by external pin (not applicable for ESP32)");
            break;
        case ESP_RST_SW:
            strcpy(returnStr, "Software reset via esp_restart");
            break;
        case ESP_RST_PANIC:
            strcpy(returnStr, "Software reset due to exception/panic");
            break;
        case ESP_RST_INT_WDT:
            strcpy(returnStr, "Reset (software or hardware) due to interrupt watchdog");
            break;
        case ESP_RST_TASK_WDT:
            strcpy(returnStr, "Reset due to task watchdog");
            break;
        case ESP_RST_WDT:
            strcpy(returnStr, "Reset due to other watchdogs");
            break;
        case ESP_RST_DEEPSLEEP:
            strcpy(returnStr, "Reset after exiting deep sleep mode");
            break;
        case ESP_RST_BROWNOUT:
            strcpy(returnStr, "Brownout reset (software or hardware)");
            break;
        case ESP_RST_SDIO:
            strcpy(returnStr, "Reset over SDIO");
            break;

        default:
            sprintf(returnStr, "Reset cause (%d) is out of range", res);  
    }
}

esp_sleep_source_t WakeUpCause;
EventGroupHandle_t xEventGroup = NULL;
uint64_t sleep_time_sec;
struct timeval now;

/*
void RTC_IRAM_ATTR esp_wake_deep_sleep(void) {
    esp_default_wake_deep_sleep();
    gpio_set_direction(GPIO_NUM_33, GPIO_MODE_OUTPUT);

    GPIO.out1_w1ts.val = ((uint32_t)1 << 1);
    gpio_set_level(GPIO_NUM_33, 1);
}
*/

void app_main()
{
    //esp_log_level_set("*", ESP_LOG_WARN);

    // Wakeup the powerbank (load the powerbank with ~100 mA)
    gpio_set_direction(GPIO_NUM_33, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_33, 1);

    // Timestamp after deep sleep wakeup
    gettimeofday(&now, NULL);

    // Remove the load from the powerbank after ~150 mS
    vTaskDelay(pdMS_TO_TICKS(150));
    gpio_set_level(GPIO_NUM_33, 0);

    WakeUpCause = esp_sleep_get_wakeup_cause();
    configASSERT(xEventGroup = xEventGroupCreate());
    
    sleep_time_sec = now.tv_sec - sleep_enter_time.tv_sec;
    ESP_LOGI(TAG, "Time spent in deep sleep: %lld Sec.", sleep_time_sec);

    // In case of SoC reset
    if (WakeUpCause != ESP_SLEEP_WAKEUP_TIMER) {
        // Log the SoC reset cause
        char cause[60] = "";
        char text[120] = "";
        GetResetCauseInText(esp_reset_reason(), cause);
        sprintf(text, "SoC reset: %s @[%lld] Time: %lld", cause, sleep_enter_time.tv_sec, now.tv_sec);
        SavePic(NULL, 0, text);

        // Restart watch from 01-08-2023 (EPOC = 1690848000 sec.)
        sleep_enter_time.tv_sec = 1690848000;
        sleep_enter_time.tv_usec = 0;
        settimeofday(&sleep_enter_time, NULL);

        // Initialize DAy/Night operation status
        DayOperation = true;
        NightOperation = false;
    }

    // Disable all forced hold on RTC GPIO's and take a new photo
    rtc_gpio_force_hold_dis_all();
    take_photo();
}