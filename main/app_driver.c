
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "app_priv.h"

#define SOIL_SENSOR_ADC_CHANNEL    ADC_CHANNEL_2  // GPIO1 (ADC1_CH0)
#define PUMP_GPIO                  GPIO_NUM_4

// Calibration values
#define DRY_VALUE    4095  // ADC reading when dry (0%)
#define WET_VALUE    0  // ADC reading when wet (100%)

static const char *TAG = "app_driver";
static adc_oneshot_unit_handle_t adc1_handle;

// Keep values within min and max
static int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// map the ADC values to percentage %
static int map_to_percent(int adc_reading) {
    int moisture_percent = 100 - ((adc_reading - WET_VALUE) * 100) / (DRY_VALUE - WET_VALUE); // higher ADC = lower mositure %
    return clamp(moisture_percent, 0, 100);
}

void app_driver_init(void) {
    // Initialize ADC
    // oneshot: trigger a reading only when needed
    adc_oneshot_unit_init_cfg_t init_config = {  
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    // channel config
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, SOIL_SENSOR_ADC_CHANNEL, &config));

    // Initialize pump GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << PUMP_GPIO, // configures GPIO 4 as an output
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    gpio_set_level(PUMP_GPIO, 0); // initially sets pump OFF (0)
    ESP_LOGI(TAG, "Driver initialized.");
}


// Read soil moisture
int app_moisture_sensor_read(void) {
    int adc_reading = 0;
    esp_err_t err = adc_oneshot_read(adc1_handle, SOIL_SENSOR_ADC_CHANNEL, &adc_reading);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "Raw ADC value: %d", adc_reading);

    int moisture_percent = map_to_percent(adc_reading);
    ESP_LOGI(TAG, "Moisture: %d%%", moisture_percent);
    return moisture_percent;
}

// pump control for turning on/off
void app_pump_set_state(bool state) {
    gpio_set_level(PUMP_GPIO, state ? 1 : 0);
    ESP_LOGI(TAG, "Pump turned %s", state ? "ON" : "OFF");
}