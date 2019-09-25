/* 
 * ESP32 template project using esp-idf 
 * Do basic stuff like: Scan wifi APs and connect to given AP, 
 * blink the onboard led periodically and print system information over UART
 * License: Public domain - CC0
 */

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "wlan.h"

#define INIT_MSG \
    "\n\n" \
    "Appname: esp-idf-template\n" \
    "Built: " __DATE__ " " __TIME__ "\n" \
    "\n"

#define LED_GPIO 2
#define ESP_MAXIMUM_RETRY 5

static SemaphoreHandle_t blink_task_sync;

// Wifi events functionality  
static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static int s_retry_num = 0;
static bool scan_done = false;

void chip_info_display(void)
{
    // Print out the chip info using miscellaneous system functions
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    printf("\n***ESP32 chip info***\n");
    printf("No. of cores: %d\nChip revision number: %d\n", chip_info.cores, 
           chip_info.revision);

    printf("Flash size: %d MB\n", spi_flash_get_chip_size()/(1024 * 1024));
    printf("Features:\n%s flash\n%s \n%s \n%s \n", \
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external",
            (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "2.4 GHz Wifi" : "",
            (chip_info.features & CHIP_FEATURE_BT) ? "Bluetooth classic" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "Bluetooth LE" : "");   
    printf("ESP-IDF version: %s\n", esp_get_idf_version());
    printf("***End ESP32 chip info***\n\n");

            
}

void blink_task(void *args)
{
    // Blink the onboard LED and print out messages 
    // This func is executed by freeRTOS, and it should not return 
    xSemaphoreTake(blink_task_sync, portMAX_DELAY);
    printf("***Blink task started!***\n");

    while (true) {
        printf("Uptime: %lld s\t Free heap: %u kB\n", 
                (int64_t) esp_timer_get_time() / 1000 / 1000, 
                esp_get_free_heap_size() / 1024 );
      
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

const char* decode_authmode(int authmode)
{
    switch (authmode) {
        case WIFI_AUTH_OPEN:
            return "OPEN";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2_PSK";
        default:
            return "Unkown";
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
   
    // Wifi event handler 
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if ( scan_done ) {
            esp_wifi_connect();
        }

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED ) {
        
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            // Try to connect for a predefined ammout of times 
            esp_wifi_connect();
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            s_retry_num++;
            printf("Trying to reconnect to AP, attempt %d...\n", s_retry_num);
        }

        // Freeze the onboard LED on faliure  
        printf("Connection to AP failed!");
        gpio_set_level(LED_GPIO, 1);

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE ) {
    
        // Initialize AP record struct
        uint16_t ap_num = 0;
        esp_wifi_scan_get_ap_num(&ap_num);

        wifi_ap_record_t *ap_list = (wifi_ap_record_t *) \
                                    malloc(sizeof(wifi_ap_record_t) * ap_num);

        // Print some info 
        if (esp_wifi_scan_get_ap_records(&ap_num, ap_list) != ESP_OK) {
            printf("Error during AP scan!\n");
            return;
        }
        printf("***AP scan complete, found %d APs***\n", ap_num);
        printf("---SSID---\t---RSSI---\t---AUTH---\n");
        for (int i = 0; i < ap_num; i++) {
            printf("%s \t %d \t\t %s\n", ap_list[i].ssid, ap_list[i].rssi, \
                                    decode_authmode(ap_list[i].authmode));
        }
        printf("***Proceed with connecting to wifi***\n\n");
        
        free(ap_list);
        scan_done = true;
        esp_wifi_connect();
        
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        
        // When you get the IP start blinking and run the serial print task
        uint8_t mac[6] = {0}; 
        if (esp_efuse_mac_get_custom(mac) == ESP_OK) {
            printf("MAC address: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n", 
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }

        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        printf("Assigned IP address: %s\n", ip4addr_ntoa(&event->ip_info.ip));
        printf("Gateway IP: %s\n", ip4addr_ntoa(&event->ip_info.gw)); 
        printf("***End of wifi stuff***\n\n");

        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        
        // Start blinking on successfull connection 
        blink_task_sync = xSemaphoreCreateBinary();
        xTaskCreate(blink_task, "blink", 4096, NULL, 25, NULL);
        xSemaphoreGive(blink_task_sync);
    }
}

void wifi_setup(void)
{
    printf("***ESP 32 wifi setup***\n");
    tcpip_adapter_init();
    
    /* Alocate memory for wifi event with freeRTOS, and register
     * created event with a default event loop for system events (wifi)    */
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                               &wifi_event_handler, NULL));
    
    // Initialize wifi driver using default configuration macro 
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    
    /* Switch to station mode, with SSID and password defined inside a
     * separate header */
    wifi_config_t sta_config = {
        .sta = {
            .ssid      = WIFI_SSID,
            .password  = WIFI_PASS,
            .bssid_set = false,
            .channel   = 0,
        },
    };
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    // Create station control block, do a scan 
    wifi_scan_config_t scan_config =  {
        .channel = 0,
        .show_hidden = true
    };
    ESP_ERROR_CHECK( esp_wifi_start() );
    ESP_ERROR_CHECK( esp_wifi_scan_start(&scan_config, false) );
}

void app_main(void)
{

    // Set all the log events to error only, and select wifi/dhcp levels 
    esp_log_level_set("*", ESP_LOG_ERROR);        
    esp_log_level_set("wifi", ESP_LOG_WARN);      
    esp_log_level_set("dhcpc", ESP_LOG_WARN);

    // Initialize default non-volatile storage flash partition 
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    printf(INIT_MSG);
    chip_info_display();

    // LED pin setup, blink is set up using wifi events 
    gpio_pad_select_gpio(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    
    // Try to connect to wifi 
    wifi_setup();
}
