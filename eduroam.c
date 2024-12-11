#include <stdio.h>
#include "driver/uart.h"
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_eap_client.h"

// Configuración de credenciales
#define WIFI_SSID "eduroam"
#define EAP_IDENTITY "correo@ejemplo.cl"
#define EAP_USERNAME "correo@ejemplo.cl"
#define EAP_PASSWORD "contraseña"

// Configuración del pin para el LED azul
#define LED_PIN GPIO_NUM_2

static const char *TAG = "eduroam_connect";

// Manejador de eventos Wi-Fi
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "¡Intentando conectar a eduroam!");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Desconectado ¡Reintentando conexión!");
        esp_wifi_connect();
        // Apagar el LED si se pierde la conexión
        gpio_set_level(LED_PIN, 0);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "¡Conectado exitosamente! Dirección IP: " IPSTR, IP2STR(&event->ip_info.ip));
        // Encender el LED al conectarse
        gpio_set_level(LED_PIN, 1);
    }
}

// Inicialización de UART
void uart_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
}

// Inicialización de Wi-Fi y conexión
void wifi_init_sta(void) {
    // Inicialización de la pila TCP/IP
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    // Configuración por defecto de Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Registro de eventos
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL);

    // Configuración de conexión WPA2 Enterprise
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);

    // Configurar EAP PEAP
    esp_eap_client_set_identity((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY));
    esp_eap_client_set_username((uint8_t *)EAP_USERNAME, strlen(EAP_USERNAME));
    esp_eap_client_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));
    esp_wifi_sta_enterprise_enable();

    // Iniciar Wi-Fi
    esp_wifi_start();
}

// Inicialización del LED
void led_init(void) {
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0); // Apagar LED al inicio
}

void app_main(void) {
    // Inicialización de NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Inicializando UART [...]");
    uart_init();

    ESP_LOGI(TAG, "Inicializando LED [...]");
    led_init();

    ESP_LOGI(TAG, "Inicializando Wi-Fi en modo STA [...]");
    wifi_init_sta();
}