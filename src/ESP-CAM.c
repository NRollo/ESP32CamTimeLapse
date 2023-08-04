#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_camera.h"
#include "esp_log.h"
#include <freertos/event_groups.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/private_include/xclk.h"
#include "string.h"
#include "driver/rtc_io.h"

static const char *TAG = "Camera";
const int16_t CAM_PIN_PWDN = 32;
//#define CAM_PIN_PWDN    32 
#define CAM_PIN_RESET   -1 //software reset will be performed
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27

#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0       5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

#define CONFIG_XCLK_FREQ 20000000
#define NIGHT_LEVEL 25

extern esp_err_t SavePic(const camera_fb_t *pic, int light1, int light2);
extern EventGroupHandle_t xEventGroup;
extern const uint32_t SD_OPERATION_DONE;
extern u_int64_t NumFilesOnSDcard(void);
extern esp_sleep_source_t WakeUpCause;

u_int64_t sleep_time_sec;
const u_int64_t DEEP_SLEEP_8_HOUR = ((uint64_t) 8 * 60 * 60 * 1000000);
const u_int64_t DEEP_SLEEP_1_HOUR = ((uint64_t) 1 * 60 * 60 * 1000000);
//const u_int64_t DEEP_SLEEP_8_HOUR = ((uint64_t) 8 * 1000000);
//const u_int64_t DEEP_SLEEP_1_HOUR = ((uint64_t) 1 * 1000000);

// 15 min. deep sleep between wakeup in uS (15 x 60 x 1.000.000)
const u_int64_t SLEEP_TIME_IN_USEC = ((uint64_t) 15 * 60 * 1000000);
//const u_int64_t SLEEP_TIME_IN_USEC = (15 * 1000000);
const uint32_t CAM_OPERATION_DONE = ( 1UL << 1UL );
RTC_DATA_ATTR struct timeval sleep_enter_time;
RTC_DATA_ATTR bool DayOperation;
RTC_DATA_ATTR bool NightOperation;

camera_config_t camera_config = {
    .pin_pwdn  = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,
    .xclk_freq_hz = CONFIG_XCLK_FREQ,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QXGA,
    .jpeg_quality = 16,
    .fb_count = 1,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY // Sets when buffers should be filled
};

esp_err_t init_camera(void)
{
    // Turn on the CAM
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(CAM_PIN_PWDN);
    gpio_set_level(CAM_PIN_PWDN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "err: %s\n", esp_err_to_name(err));
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_hmirror(s, true);

    return ESP_OK;
}

void GotoDeepSleep (uint64_t sleepTime) {
    // Turn off the CAM
    gpio_set_level(CAM_PIN_PWDN, 1);

    // Wait for an empty UART Tx buffer
    uart_wait_tx_idle_polling(CONFIG_ESP_CONSOLE_UART_NUM);

    // Hold GPIO during deep sleep
    gpio_hold_en(CAM_PIN_PWDN);
    gpio_deep_sleep_hold_en();

    // Isolate GPIO's to save power during deep sleep
    gpio_set_level(GPIO_NUM_14, 0);
    rtc_gpio_isolate(GPIO_NUM_12);
    rtc_gpio_isolate(GPIO_NUM_4);

    // Load deep sleep timer and enter deep sleep
    esp_sleep_enable_timer_wakeup(sleepTime);
    esp_deep_sleep_start(); 

    // We will never arrive here!!! Reset is enforced after deep sleep
}

/* Night time operations:                                                     */
/* When in day operation and light level are below limit; sleep for 8 hours   */
/* After 8 hours, measure light level - if above limit; resume day operations */
/*                                    - if below limit; deep sleep 1 hour     */
void NightTimeOperation (void) {
    sensor_t *s;
    int lightNight = 0;

    if (DayOperation) {
        return;
    }
    else {
        if (NightOperation) {
            init_camera();
            camera_fb_t *pic = esp_camera_fb_get();

            s = esp_camera_sensor_get();
            // Get the light in this frame
            lightNight = s->get_reg(s, 0x56A1, 0xff);
            //int light2 = s->get_reg(s, 0x56A2, 0xff);
            ESP_LOGI(TAG, "Lighting: %d", lightNight);
            // Reset the OV5640 sensor... for consistency
            s->set_reg(s, 0x3008, 0xff, 0x80);
            esp_camera_fb_return(pic);

            if (lightNight >= (NIGHT_LEVEL + 5)) {
                // Lights is good; resume day operation
                DayOperation = true;
                NightOperation = false;
                ESP_LOGI(TAG, "Entering Day OPS");
                GotoDeepSleep (SLEEP_TIME_IN_USEC);

                // We will never arrive here!!! Reset is enforced after deep sleep
            }
            else {
                // Lights is bad; lets wait an hour
                ESP_LOGI(TAG, "Entering 1 hours Night OPS");
                GotoDeepSleep (DEEP_SLEEP_1_HOUR);
                
                // We will never arrive here!!! Reset is enforced after deep sleep
            }

        }
        else {
            // Just ended day operations
            ESP_LOGI(TAG, "Entering Night OPS");
            NightOperation = true;
            GotoDeepSleep (DEEP_SLEEP_8_HOUR);

            // We will never arrive here!!! Reset is enforced after deep sleep
        }
    }
}

void SetLight(void){
    sensor_t * s = esp_camera_sensor_get();

  s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
  s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
  s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
 // s->set_aec2(s, 0);           // 0 = disable , 1 = enable
  //s->set_ae_level(s, 2);       // -2 to 2
  //s->set_aec_value(s, 1200);    // 0 to 1200
  s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
  //s->set_agc_gain(s, 0);       // 0 to 30
  s->set_gainceiling(s, 255);  // 0 to 6
  s->set_bpc(s, 1);            // 0 = disable , 1 = enable
  s->set_wpc(s, 1);            // 0 = disable , 1 = enable
  s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
  s->set_lenc(s, 0);           // 0 = disable , 1 = enable
  //s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
  s->set_vflip(s, 0);          // 0 = disable , 1 = enable
  s->set_dcw(s, 0);            // 0 = disable , 1 = enable
  s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
  //s->set_quality(s, 8);
  s->set_brightness(s, 0);
  s->set_contrast(s, 0);
  s->set_saturation(s, -1);
  s->set_sharpness(s, 0);
  s->set_denoise(s, 3);
  s->set_aec_value(s, 0);
  s->set_agc_gain(s, 1);
  s->set_raw_gma(s,1);
  s->set_lenc(s, 1);
  s->set_bpc(s, 1);
  s->set_wpc(s, 1);  
}

void take_photo(void)
{
    struct timeval now;
    sensor_t *s;

    rtc_gpio_force_hold_dis_all();

    NightTimeOperation();

    ESP_LOGI(TAG, "Starting Taking Picture!");
    gettimeofday(&now, NULL);
    u_int64_t sleep_time_sec = now.tv_sec - sleep_enter_time.tv_sec;
    ESP_LOGI(TAG, "Wake up from: %d, Time spent in deep sleep: %lld Sec", esp_sleep_get_wakeup_cause(), sleep_time_sec);
    gettimeofday(&sleep_enter_time, NULL);

    init_camera();
    camera_fb_t *pic = esp_camera_fb_get();

    //Since we got the frame buffer, we reset the sensor and put it to sleep while saving the file
    s = esp_camera_sensor_get();
    // Get the light in this shoot
    int light = s->get_reg(s, 0x56A1, 0xff);
    int light2 = s->get_reg(s, 0x56A2, 0xff);
    ESP_LOGI(TAG, "Light level: 0x%02x 0x%02x", light, light2);
    if (light < NIGHT_LEVEL) {
        DayOperation = false;
    }
    // Reset the OV5640 sensor
    s->set_reg(s, 0x3008, 0xff, 0x80);
    // Turn off the CAM
    gpio_set_level(CAM_PIN_PWDN, 1);

    if (SavePic(pic, light, light2) == ESP_FAIL) {
        ESP_LOGE(TAG, "No valid frame taken or file creation failed");
    }

    xEventGroupWaitBits(xEventGroup, (SD_OPERATION_DONE), pdTRUE, pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "Finished Taking Picture!");
    // Free the frame buffer
    esp_camera_fb_return(pic);
    xEventGroupSetBits(xEventGroup, CAM_OPERATION_DONE);

    GotoDeepSleep (SLEEP_TIME_IN_USEC);

    // We will never arrive here!!! Reset is enforced after deep sleep
}
