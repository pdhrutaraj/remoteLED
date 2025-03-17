#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_crt_bundle.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "driver/gpio.h"

#define WIFI_SSID "Redmi Note 11S"
#define WIFI_PASS "Patientpay2015"
//#define WIFI_SSID "Airtel-B310-7EAB"
//#define WIFI_PASS "3B5EC2BC2E4"

//#define TAG "ESP32_SSL"
//#define MAX_RESPONSE_SIZE 1024

#define LED_GPIO GPIO_NUM_2
static char auth_token[512] = {0};  // Stores extracted JWT token
static const char *TAG = "ESP32_SSL";  // Logging tag

// **Wi-Fi **
//void wifi_init(void);

/* Initialize Wi-Fi */

#define WIFI_RETRY_MAX  10  // Retry count if Wi-Fi fails

static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
static int retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < WIFI_RETRY_MAX) {
            retry_num++;
            ESP_LOGW("WIFI", "Retrying connection... Attempt %d/%d", retry_num, WIFI_RETRY_MAX);
            esp_wifi_connect();
        } else {
            ESP_LOGE("WIFI", "Wi-Fi connection failed! Restarting ESP32...");
            esp_restart();  // Restart ESP32 after too many failed attempts
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WIFI", "Connected! IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init() {
    ESP_LOGI("WIFI", "Initializing Wi-Fi...");
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
    .sta = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
        .threshold.authmode = WIFI_AUTH_WPA2_PSK, // Force WPA2
        .pmf_cfg = {
            .capable = true,
            .required = false
        },
    },
};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WIFI", "Wi-Fi setup complete. Waiting for connection...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}
//
//led control
void control_led(const char *state) {
    if (strcmp(state, "on") == 0) {
        gpio_set_level(LED_GPIO, 1);
        ESP_LOGI(TAG, "LED TURNED ON");
    } else {
        gpio_set_level(LED_GPIO, 0);
        ESP_LOGI(TAG, "LED TURNED OFF");
    }
}
//
// **HTTP Event Handler_1 wotking
//
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    static char response_buffer[1024];  // Buffer to store response data
    static int response_index = 0;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (response_index + evt->data_len < sizeof(response_buffer)) {
                memcpy(response_buffer + response_index, evt->data, evt->data_len);
                response_index += evt->data_len;
                response_buffer[response_index] = '\0';  // Null-terminate the response
            }
            break;
        
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "Full Response: %s", response_buffer);
            cJSON *json = cJSON_Parse(response_buffer);
            if (json) {
                cJSON *access = cJSON_GetObjectItem(json, "access");
                if (cJSON_IsString(access)) {
                    strncpy(auth_token, access->valuestring, sizeof(auth_token) - 1);
                    ESP_LOGI(TAG, "Auth Token: %s", auth_token);
                } else {
                    //ESP_LOGE(TAG, "Error: Access token not found in response");
                }
                cJSON_Delete(json);
	    	if(auth_token > 0){
			ESP_LOGI(TAG, "Auth token available...");
			//
		/*
		cJSON *switch_obj = cJSON_GetArrayItem(json, 0);  // Get first switch object
                if (switch_obj) {
                    cJSON *state = cJSON_GetObjectItem(json, "state");
                    if (cJSON_IsString(state)) {
                        ESP_LOGI(TAG, "Switch State: %s", state->valuestring);
                        control_led(state->valuestring);  // Control LED
                    }
                 } else {
                    ESP_LOGE(TAG, "Error: No switch data found");
                }
                cJSON_Delete(json);
		*/
			//
		}

            } else {
                ESP_LOGE(TAG, "Error: Failed to parse JSON");
            }
            response_index = 0;  // Reset buffer index for next request
            break;

        default:
            break;
    }
    return ESP_OK;
}

//
//working handler
/*
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len > 0) {
                ESP_LOGI(TAG, "Full Response: %.*s", evt->data_len, (char *)evt->data);
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}
*/
//
/*working minimal
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        strncpy(auth_token, (char *)evt->data, evt->data_len);
    }
    return ESP_OK;
}
*/
//
void get_auth_token() {
/*
    esp_http_client_config_t config = {
        .url = "https://eapi-vijn.onrender.com/api/token/",
        .event_handler = _http_event_handler,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
*/
     esp_http_client_config_t config = {
    	.url = "https://eapi-vijn.onrender.com/api/token/",
    	.event_handler = _http_event_handler,
    	.method = HTTP_METHOD_POST,
    	.crt_bundle_attach = esp_crt_bundle_attach,
    //.timeout_ms = 15000,  // Increase timeout to 15 seconds
    	.timeout_ms = 10000,  // Increase timeout to 1 seconds
	};
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    const char *post_data = "{\"username\": \"admin\", \"password\": \"admin\"}";
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        char response[512] = {0};
        int read_len = esp_http_client_read(client, response, sizeof(response) - 1);
        response[read_len] = '\0';  // **Ensure null termination**

        /* Parse JSON */
        cJSON *json = cJSON_Parse(response);
        if (json) {
            cJSON *access_token = cJSON_GetObjectItem(json, "access");
            if (access_token && cJSON_IsString(access_token)) {
                strncpy(auth_token, access_token->valuestring, sizeof(auth_token) - 1);
                ESP_LOGI(TAG, "JWT Token: %s", auth_token);
            }
            cJSON_Delete(json);
        }
    } else {
        ESP_LOGI(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}


// Function to fetch switch state

//working

void fetch_switch_state() {
    char auth_header[600];  // Buffer to store the Authorization header
    //ESP_LOGI(TAG, "Auth Token: %s", auth_token);
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", auth_token);

    esp_http_client_config_t config = {
        .url = "https://eapi-vijn.onrender.com/api/switches/",
        .event_handler = _http_event_handler,
        .method = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,  // SSL/TLS certificate validation
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set Authorization header
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        //ESP_LOGI(TAG, "Switch state request successful.");
	
    } else {
        ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// Task to continuously fetch switch state
void switch_task(void *pvParameters) {
    while (1) {
        fetch_switch_state();
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

// **Main Function**

void app_main(void) {
    
    nvs_flash_init();

    wifi_init(); 
    
    get_auth_token();  // **Fetch Token Securely**

    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    
    xTaskCreate(&switch_task, "switch_task", 8192, NULL, 5, NULL);
}


