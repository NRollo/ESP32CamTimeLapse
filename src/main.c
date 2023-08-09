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
#include "soc/rtc_io_reg.h"
#include "soc/soc.h"
#include "soc/timer_group_reg.h"
#include <rom/ets_sys.h>

// Date 01-08-2023 as EPOC Sec.
#define DATE_01_08_2023_AS_EPOC 1690848000

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

// Deep sleep wakeup (pre-boot), wakeup the powerbank by applying
// a 150 mS load pulse
void RTC_IRAM_ATTR esp_wake_deep_sleep(void) {
    // Initalize the timing parameters
    ets_update_cpu_frequency_rom(ets_get_detected_xtal_freq() / 1000000);

    // Output - set RTC mux
    SET_PERI_REG_MASK(RTC_IO_XTAL_32K_PAD_REG, BIT(RTC_IO_X32N_MUX_SEL_S));
    // Output - clear the HOLD bit
    CLEAR_PERI_REG_MASK(RTC_IO_XTAL_32K_PAD_REG, BIT(RTC_IO_X32N_HOLD_S));
    // Output - enable GPIO output
    REG_WRITE(RTC_GPIO_ENABLE_W1TS_REG, BIT(RTC_GPIO_ENABLE_W1TS_S + GPIO_NUM_8));
 
    // Ouput - set the GPIO output high
    REG_WRITE(RTC_GPIO_OUT_W1TS_REG, BIT(RTC_GPIO_OUT_DATA_W1TS_S + GPIO_NUM_8));
    // 150 mS pulse
    ets_delay_us(160000);
    // Ouput - clear the GPIO output
    REG_WRITE(RTC_GPIO_OUT_W1TC_REG, BIT(RTC_GPIO_OUT_DATA_W1TS_S + GPIO_NUM_8));
}

void app_main()
{
    //esp_log_level_set("*", ESP_LOG_WARN);

    // Timestamp after deep sleep wakeup
    gettimeofday(&now, NULL);

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

        // Restart system time
        sleep_enter_time.tv_sec = DATE_01_08_2023_AS_EPOC;
        sleep_enter_time.tv_usec = 0;
        settimeofday(&sleep_enter_time, NULL);

        // Initialize Day/Night operation status
        DayOperation = true;
        NightOperation = false;
    }

    // Disable all forced hold on RTC GPIO's and take a new photo
    rtc_gpio_force_hold_dis_all();
    take_photo();
}