#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "esp_http_client.h"
#include "cJSON.h"
#include "driver/gpio.h"

/* Wi-Fi Credentials */
#define WIFI_SSID "your-SSID"
#define WIFI_PASS "your-PASSWORD"

/* API Endpoints */
#define TOKEN_URL  "https://eapi-vijn.onrender.com/api/token/"
#define SWITCH_URL "https://eapi-vijn.onrender.com/api/switches/"

/* API Credentials */
#define USERNAME "admin"
#define PASSWORD "admin"

/* LED GPIO */
#define LED_GPIO GPIO_NUM_2

/* Logging Tag */
static const char *TAG = "ESP32_APP";

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

/* Store the JWT token */
static char jwt_token[512] = {0};

extern const uint8_t server_cert_pem_start[] asm("_binary_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_cert_pem_end");


/* Wi-Fi Event Handler */

/* Initialize Wi-Fi */

#define WIFI_RETRY_MAX  5  // Retry count if Wi-Fi fails

static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
static int retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < WIFI_RETRY_MAX) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGW("WIFI", "Retrying connection...");
        } else {
            ESP_LOGE("WIFI", "Failed to connect to Wi-Fi!");
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
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WIFI", "Wi-Fi setup complete. Waiting for connection...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

/* HTTP POST to get JWT Token */
static void get_jwt_token() {
    esp_http_client_config_t config = {
        .url = TOKEN_URL,
        .method = HTTP_METHOD_POST,
        .cert_pem = (const char *)server_cert_pem_start,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char post_data[128];
    snprintf(post_data, sizeof(post_data), "{\"username\":\"%s\",\"password\":\"%s\"}", USERNAME, PASSWORD);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        char response[512] = {0};
        esp_http_client_read(client, response, sizeof(response) - 1);

        /* Parse JSON */
        cJSON *json = cJSON_Parse(response);
        if (json) {
            cJSON *token = cJSON_GetObjectItem(json, "access");
            if (token) {
                strcpy(jwt_token, token->valuestring);
                ESP_LOGI(TAG, "JWT Token: %s", jwt_token);
            }
            cJSON_Delete(json);
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

/* HTTP GET request to fetch switch state */
static void fetch_switch_state() {
    if (strlen(jwt_token) == 0) {
        ESP_LOGE("HTTP", "JWT token is empty, cannot fetch switch state.");
        return;
    }

    char auth_header[512];
//    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", jwt_token);
//      snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %.490s", jwt_token);
strcpy(auth_header, "Authorization: Bearer ");
strncat(auth_header, jwt_token, sizeof(auth_header) - strlen(auth_header) - 1);


    esp_http_client_config_t config = {
        .url = "https://eapi-vijn.onrender.com/api/switches/",
        .method = HTTP_METHOD_GET,
        .cert_pem = (const char *)server_cert_pem_start,  // Make sure to add SSL cert if needed
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Authorization", auth_header);

    ESP_LOGI("HTTP", "Sending GET request...");
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int content_length = esp_http_client_get_content_length(client);
        ESP_LOGI("HTTP", "HTTP GET Success! Content-Length: %d", content_length);
    } else {
        ESP_LOGE("HTTP", "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}


/* Task to periodically fetch switch state */
void switch_control_task(void *pvParameter) {
    while (1) {
        fetch_switch_state();
        vTaskDelay(pdMS_TO_TICKS(5000)); // Fetch every 5 seconds
    }
}

void app_main() {
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Initialize GPIO for LED */
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    /* Initialize Wi-Fi */
    wifi_init();

    /* Wait for Wi-Fi connection */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    /* Get JWT token */
    get_jwt_token();

    /* Start the switch control task */
    xTaskCreate(&switch_control_task, "switch_control_task", 8192, NULL, 5, NULL);
}
