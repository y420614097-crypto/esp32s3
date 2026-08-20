#include <string.h>
#include "wifi_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

static const char *TAG = "WIFI_MGR";

// 定义 Wi-Fi 名称和密码
#define WIFI_SSID      "1102"      // 替换为你的 Wi-Fi 名称
#define WIFI_PASS      "86279190"  // 替换为你的 Wi-Fi 密码
#define MAX_RETRY      5                     // 最大重试次数

static int s_retry_num = 0;

// 定义一个事件组，用来通知其他任务“时间已经同步好了”
EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

// NTP 时间同步函数
static void obtain_time(void) {
    ESP_LOGI(TAG, "正在从 NTP 服务器获取时间...");
    
    // 初始化 SNTP 服务，使用阿里云的 NTP 服务器（国内速度快）
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    sntp_init();

    // 设置时区为中国标准时间 (UTC+8)
    setenv("TZ", "CST-8", 1);
    tzset();
}


// 核心：Wi-Fi 事件处理回调函数
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    // 1. Wi-Fi 启动成功，开始连接
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    // 2. 连接断开或失败
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect(); // 重试连接
            s_retry_num++;
            ESP_LOGI(TAG, "重试连接 Wi-Fi... (%d/%d)", s_retry_num, MAX_RETRY);
        } else {
            ESP_LOGE(TAG, "Wi-Fi 连接失败，达到最大重试次数！");
        }
    } 
    // 3. 成功获取 IP 地址（说明真正连上路由器了）
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "成功获取 IP 地址: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0; // 重置重试次数
        // 这里可以触发 OTA 任务！
    }
}

void wifi_manager_init(void)
{
    // 1. 初始化 NVS（Wi-Fi 需要用它来保存配置）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 初始化网络接口和默认事件循环
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 3. 初始化 Wi-Fi 驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 4. 注册事件回调函数
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

    // 5. 配置 Wi-Fi 账号密码
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 6. 启动 Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi 初始化完成，正在尝试连接 %s...", WIFI_SSID);
}