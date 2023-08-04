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

//static const char *TAG = "Main";

extern void take_photo(void);
extern RTC_DATA_ATTR bool DayOperation;
extern RTC_DATA_ATTR bool NightOperation;

esp_sleep_source_t WakeUpCause;

EventGroupHandle_t xEventGroup = NULL;
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

    // wakeup powerbank
    gpio_set_direction(GPIO_NUM_33, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_33, 1);
    vTaskDelay(pdMS_TO_TICKS(150));
    gpio_set_level(GPIO_NUM_33, 0);

    WakeUpCause = esp_sleep_get_wakeup_cause();
    configASSERT(xEventGroup = xEventGroupCreate());
    
    // Initialize operation status in case of SoC reset
    if (WakeUpCause != ESP_SLEEP_WAKEUP_TIMER) {
        DayOperation = true;
        NightOperation = false;
    }

    take_photo();
}