
/* Plant Watering System Example */

#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_ota.h>

#include <esp_rmaker_common_events.h>

#include <app_network.h>
#include <app_insights.h>

#include "app_priv.h"

static const char *TAG = "plant_watering_system";
esp_rmaker_device_t *pump_device;
esp_rmaker_device_t *moisture_sensor_device;

// Default values
#define DEFAULT_PUMP_STATE false
#define DEFAULT_MOISTURE_THRESHOLD 50
#define DEFAULT_AUTO_MODE true

// Global variables for system state
bool auto_mode = DEFAULT_AUTO_MODE;
int moisture_threshold = DEFAULT_MOISTURE_THRESHOLD;
int current_moisture = 0;

// static void moisture_read_task(void *arg)
// {
//     while (1) {
//         // Read moisture sensor value
//         current_moisture = app_moisture_sensor_read();
//         ESP_LOGI(TAG, "Moisture Level: %d%%", current_moisture);
        
//         // Update moisture parameter in RainMaker
//         esp_rmaker_param_t *moisture_param = esp_rmaker_device_get_param_by_name(moisture_sensor_device, "moisture");
//         if (moisture_param) {
//             esp_rmaker_param_update_and_report(moisture_param, esp_rmaker_int(current_moisture));
//         }
        
//         // Auto mode logic
//         if (auto_mode) {
//             bool pump_state = (current_moisture < moisture_threshold);
//             app_pump_set_state(pump_state);
            
//             // Update pump state in RainMaker if changed
//             esp_rmaker_param_t *power_param = esp_rmaker_device_get_param_by_name(pump_device, ESP_RMAKER_DEF_POWER_NAME);
//             if (power_param) {
//                 const esp_rmaker_param_val_t *current_val = esp_rmaker_param_get_val(power_param);
//                 if (current_val && current_val->val.b != pump_state) {
//                     esp_rmaker_param_update_and_report(power_param, esp_rmaker_bool(pump_state));
//                 }
//             }
//         }
        
//         vTaskDelay(5000 / portTICK_PERIOD_MS); // Read every 5 seconds
//     }
// }

static void moisture_read_task(void *arg)
{
    while (1) {
        // Read moisture sensor value
        current_moisture = app_moisture_sensor_read();
        
        // Only log if value changed significantly (reduce log spam)
        static int last_logged_moisture = -1;
        if (abs(current_moisture - last_logged_moisture) >= 5) { // 5% change threshold
            ESP_LOGI(TAG, "Moisture Level: %d%%", current_moisture);
            last_logged_moisture = current_moisture;
        }
        
        // Update moisture parameter in RainMaker
        esp_rmaker_param_t *moisture_param = esp_rmaker_device_get_param_by_name(moisture_sensor_device, "moisture");
        if (moisture_param) {
            esp_rmaker_param_update_and_report(moisture_param, esp_rmaker_int(current_moisture));
        }
        
        // Auto mode logic
        if (auto_mode) {
            bool pump_state = (current_moisture < moisture_threshold);
            app_pump_set_state(pump_state);
            
            // Update pump state in RainMaker if changed
            esp_rmaker_param_t *power_param = esp_rmaker_device_get_param_by_name(pump_device, ESP_RMAKER_DEF_POWER_NAME);
            if (power_param) {
                const esp_rmaker_param_val_t *current_val = esp_rmaker_param_get_val(power_param);
                if (current_val && current_val->val.b != pump_state) {
                    esp_rmaker_param_update_and_report(power_param, esp_rmaker_bool(pump_state));
                }
            }
        }
        
        vTaskDelay(10000 / portTICK_PERIOD_MS); // Reduced from 5s for more responsive updates
    }
}

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
            const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    
    const char *device_name = esp_rmaker_device_get_name(device);
    const char *param_name = esp_rmaker_param_get_name(param);
    
    // Handle pump control
    if (strcmp(device_name, "Water Pump") == 0) {
        if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
            ESP_LOGI(TAG, "Pump state set to %s", val.val.b ? "ON" : "OFF");
            app_pump_set_state(val.val.b);
            esp_rmaker_param_update(param, val);
            
            // When manually controlling pump, disable auto mode temporarily
            if (val.val.b != (current_moisture < moisture_threshold)) {
                auto_mode = false;
                esp_rmaker_param_t *auto_param = esp_rmaker_device_get_param_by_name(pump_device, "auto_mode");
                if (auto_param) {
                    esp_rmaker_param_update_and_report(auto_param, esp_rmaker_bool(false));
                }
            }
        } else if (strcmp(param_name, "auto_mode") == 0) {
            ESP_LOGI(TAG, "Auto mode set to %s", val.val.b ? "ON" : "OFF");
            auto_mode = val.val.b;
            esp_rmaker_param_update(param, val);
        } else if (strcmp(param_name, "moisture_threshold") == 0) {
            ESP_LOGI(TAG, "Moisture threshold set to %d", val.val.i);
            moisture_threshold = val.val.i;
            esp_rmaker_param_update(param, val);
        }
    }
    
    return ESP_OK;
}

/* Event handler for catching RainMaker events */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == RMAKER_EVENT) {
        switch (event_id) {
            case RMAKER_EVENT_INIT_DONE:
                ESP_LOGI(TAG, "RainMaker Initialised.");
                break;
            case RMAKER_EVENT_CLAIM_STARTED:
                ESP_LOGI(TAG, "RainMaker Claim Started.");
                break;
            case RMAKER_EVENT_CLAIM_SUCCESSFUL:
                ESP_LOGI(TAG, "RainMaker Claim Successful.");
                break;
            case RMAKER_EVENT_CLAIM_FAILED:
                ESP_LOGI(TAG, "RainMaker Claim Failed.");
                break;
            default:
                ESP_LOGW(TAG, "Unhandled RainMaker Event: %"PRIi32, event_id);
        }
    } else if (event_base == RMAKER_COMMON_EVENT) {
        switch (event_id) {
            case RMAKER_EVENT_REBOOT:
                ESP_LOGI(TAG, "Rebooting in %d seconds.", *((uint8_t *)event_data));
                break;
            case RMAKER_EVENT_WIFI_RESET:
                ESP_LOGI(TAG, "Wi-Fi credentials reset.");
                break;
            case RMAKER_EVENT_FACTORY_RESET:
                ESP_LOGI(TAG, "Node reset to factory defaults.");
                break;
            default:
                ESP_LOGW(TAG, "Unhandled RainMaker Common Event: %"PRIi32, event_id);
        }
    } else if (event_base == APP_NETWORK_EVENT) {
        switch (event_id) {
            case APP_NETWORK_EVENT_QR_DISPLAY:
                ESP_LOGI(TAG, "Provisioning QR : %s", (char *)event_data);
                break;
            case APP_NETWORK_EVENT_PROV_TIMEOUT:
                ESP_LOGI(TAG, "Provisioning Timed Out. Please reboot.");
                break;
            case APP_NETWORK_EVENT_PROV_RESTART:
                ESP_LOGI(TAG, "Provisioning has restarted due to failures.");
                break;
            default:
                ESP_LOGW(TAG, "Unhandled App Wi-Fi Event: %"PRIi32, event_id);
                break;
        }
    } else {
        ESP_LOGW(TAG, "Invalid event received!");
    }
}

void app_main()
{
    /* Initialize Application specific hardware drivers */
    app_driver_init();
    app_moisture_sensor_read();
    app_pump_set_state(DEFAULT_PUMP_STATE);

    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    /* Initialize Wi-Fi */
    app_network_init();

    /* Register an event handler to catch RainMaker events */
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_COMMON_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(APP_NETWORK_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    /* Initialize the ESP RainMaker Agent */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "Plant Watering System", "Watering");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    /* Create Water Pump device */
    pump_device = esp_rmaker_device_create("Water Pump", ESP_RMAKER_DEVICE_SWITCH, NULL);
    esp_rmaker_device_add_cb(pump_device, write_cb, NULL);
    esp_rmaker_device_add_param(pump_device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "Water Pump"));
    
    /* Add power parameter for pump control */
    esp_rmaker_param_t *power_param = esp_rmaker_power_param_create(ESP_RMAKER_DEF_POWER_NAME, DEFAULT_PUMP_STATE);
    esp_rmaker_device_add_param(pump_device, power_param);
    esp_rmaker_device_assign_primary_param(pump_device, power_param);
    
    /* Add auto mode parameter */
    esp_rmaker_param_t *auto_param = esp_rmaker_param_create("auto_mode", "Auto Mode", 
        esp_rmaker_bool(DEFAULT_AUTO_MODE), PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_rmaker_param_add_ui_type(auto_param, ESP_RMAKER_UI_TOGGLE);
    esp_rmaker_device_add_param(pump_device, auto_param);
    
    /* Add moisture threshold parameter */
    esp_rmaker_param_t *threshold_param = esp_rmaker_param_create("moisture_threshold", "Moisture Threshold", 
        esp_rmaker_int(DEFAULT_MOISTURE_THRESHOLD), PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_rmaker_param_add_ui_type(threshold_param, ESP_RMAKER_UI_SLIDER);
    esp_rmaker_param_add_bounds(threshold_param, esp_rmaker_int(0), esp_rmaker_int(100), esp_rmaker_int(5));
    esp_rmaker_device_add_param(pump_device, threshold_param);

    /* Create Moisture Sensor device */
    moisture_sensor_device = esp_rmaker_device_create("Soil Moisture", "esp.device.moisture-sensor", NULL);
    esp_rmaker_param_t *moisture_param = esp_rmaker_param_create("moisture", "Moisture Level", 
        esp_rmaker_int(0), PROP_FLAG_READ | PROP_FLAG_TIME_SERIES);
    esp_rmaker_param_add_ui_type(moisture_param, ESP_RMAKER_UI_TEXT);
    esp_rmaker_device_add_param(moisture_sensor_device, moisture_param);
    esp_rmaker_device_assign_primary_param(moisture_sensor_device, moisture_param);

    /* Add devices to node */
    esp_rmaker_node_add_device(node, pump_device);
    esp_rmaker_node_add_device(node, moisture_sensor_device);

    /* Enable OTA */
    esp_rmaker_ota_enable_default();

    /* Enable timezone service */
    esp_rmaker_timezone_service_enable();

    /* Enable scheduling */
    esp_rmaker_schedule_enable();

    /* Enable Scenes */
    esp_rmaker_scenes_enable();

    /* Enable Insights */
    app_insights_enable();

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    /* Start moisture reading task */
    xTaskCreate(&moisture_read_task, "moisture_read_task", 4096, NULL, 5, NULL);

    /* Start the Wi-Fi */
    err = app_network_set_custom_mfg_data(MGF_DATA_DEVICE_TYPE_SWITCH, MFG_DATA_DEVICE_SUBTYPE_SWITCH);
    err = app_network_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }
}
