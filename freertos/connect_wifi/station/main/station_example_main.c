/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_http_client.h"
#include "esp_netif.h"

#include "driver/gpio.h"
#include "rom/gpio.h"

#include "driver/adc.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define WIFI_SSID      "Such_5g"
#define WIFI_PASS      "tenida007"
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

// ThingSpeak API
#define THINGSPEAK_WRITE_API_KEY "ECC2HDT4VMNMN3CI" //"YOUR_THINGSPEAK_WRITE_API_KEY"
#define THINGSPEAK_READ_API_KEY "OX32XZWOENRHNLDU" //"YOUR_THINGSPEAK_READ_API_KEY"
#define THINGSPEAK_URL "http://api.thingspeak.com/update"

/* PUSH button related data */
#define BTN_GPIO 25


#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG_WIFI = "wifi station";
static const char *TAG_HTTPS = "https";
static const char *TAG_GPIO = "gpio state";

static int s_retry_num = 0;
int last_button_state; // Defaults high because of pull-up
int button_state;

/* WIFI event handler code */
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    esp_err_t ret;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ret = esp_wifi_connect();
        ESP_LOGI(TAG_WIFI, "1: esp_wifi_connect failed with %d", ret);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            ret = esp_wifi_connect();
            ESP_LOGI(TAG_WIFI, "2: esp_wifi_connect failed with %d", ret);
            s_retry_num++;
            ESP_LOGI(TAG_WIFI, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG_WIFI,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG_WIFI, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* WIFI connect code */
void wifi_init_sta(void *arg)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG_WIFI, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG_WIFI, "connected to ap SSID:%s password:%s",
                 WIFI_SSID, WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG_WIFI, "Failed to connect to SSID:%s, password:%s",
                 WIFI_SSID, WIFI_PASS);
    } else {
        ESP_LOGE(TAG_WIFI, "UNEXPECTED EVENT");
    }

}


/* HTTPS event handler */
static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG_HTTPS, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG_HTTPS, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG_HTTPS, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG_HTTPS, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG_HTTPS, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // Uncomment below if you want to print response body
            // printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG_HTTPS, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG_HTTPS, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            ESP_LOGI(TAG_HTTPS, "HTTP_EVENT_UNKNOWN");
            break;
    }
    return ESP_OK;
}

static void http_post_button_state(int state_1, int state_2, int state_3)
{
    char url[256];
    srand(time(NULL));

    int i = rand() % 100 + 1;
    int j = rand() % 100 + 1;
    int k = rand() % 100 + 1;
    // Construct URL with API key and button state as field1
    snprintf(url, sizeof(url), "%s?api_key=%s&field1=%d&field2=%d&field3=%d", 
                THINGSPEAK_URL, THINGSPEAK_WRITE_API_KEY, state_1 + i, state_2 + j, state_3 +k);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = _http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG_HTTPS, "HTTP POST Status = %d", status_code);
    } else {
        ESP_LOGE(TAG_HTTPS, "HTTP POST failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

//C:\Espressif\frameworks\esp-idf-v5.4\components\driver\deprecated\driver\adc_types_legacy.h
// Soil moisture sensor ADC configuration
// Assuming the soil moisture sensor is connected to ADC1 channel 0
#define SOIL_MOISTURE_ADC_CHANNEL ADC1_CHANNEL_0  // Example ADC channel 
#define MOISTURE_SENSOR_ATTENUATION ADC_ATTEN_DB_11

static const char *TAG_SOIL_MOIST = "soil_moisture";
// Function to initialize ADC
// void adc_init() {
//     adc1_config_width(ADC_WIDTH_BIT_12);
//     adc1_config_channel_atten(SOIL_MOISTURE_ADC_CHANNEL, MOISTURE_SENSOR_ATTENUATION);
// }

// Task to read soil moisture sensor periodically
void soil_moisture_task(void *pvParameters) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(SOIL_MOISTURE_ADC_CHANNEL, MOISTURE_SENSOR_ATTENUATION);
    int core_id = xPortGetCoreID();
    printf("Soil moisture task started on core %d\n", core_id);

    while (1) {
        int adc_reading = adc1_get_raw(SOIL_MOISTURE_ADC_CHANNEL);
        
        // Normalize ADC value to percentage (simple inverse mapping example)
        float moisture = 100.0f - ((adc_reading / 4095.0f) * 100.0f);

        ESP_LOGI(TAG_SOIL_MOIST, "ADC Raw Value: %d, Soil Moisture: %f%%", adc_reading, moisture);

        vTaskDelay(pdMS_TO_TICKS(1000)); // Read every 1 second
    }
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG_GPIO, "GPIO Button experiment starts...");

    ESP_LOGI(TAG_WIFI, "ESP_WIFI_MODE_STA");
    // Create WiFi task on core 0 (PRO_CPU)
    xTaskCreatePinnedToCore(wifi_init_sta, "WiFi_Task", 4096, NULL, 5, NULL, 0);
    //wifi_init_sta();

#if 0
    // Create push button task on core 1 (APP_CPU)
    xTaskCreatePinnedToCore(push_button_task, "Push_Button_Task", 2048, NULL, 5, NULL, 1);
    last_button_state = 1; // Defaults high because of pull-up
    /* */
    // Configure button GPIO as input with internal pull-up
    gpio_pad_select_gpio(BTN_GPIO);
    gpio_set_direction(BTN_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_GPIO, GPIO_PULLUP_ONLY);
    while (1) {
        //ESP_LOGI(TAG_GPIO, ".-");
        button_state = gpio_get_level(BTN_GPIO);
        if (button_state == 0 && last_button_state == 1) {
            // Button pressed (transition from HIGH to LOW)
            ESP_LOGI(TAG_GPIO, "Button pressed! Sending data...");
            http_post_button_state(1, 2, 3);
            vTaskDelay(pdMS_TO_TICKS(1000)); // debounce and to avoid flooding
        } else if (button_state == 1 && last_button_state == 0) {
            // Button released, optional: send 0 if you want to track release
            ESP_LOGI(TAG_GPIO, "Button released");
            http_post_button_state(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        last_button_state = button_state;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
#endif  // PUSH BTN Code
    http_post_button_state(1, 2, 3);
    xTaskCreatePinnedToCore(soil_moisture_task, "soil_task", 2048, NULL, 4, NULL, 1);

}
