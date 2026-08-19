#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#define WIFI_SSID      "1102"    // 替换成你家的WiFi名称
#define WIFI_PASSWORD  "86279190"    // 替换成你家的WiFi密码

// Wi-Fi 事件回调函数（核心机制：异步处理）
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    // 1. 当 Wi-Fi 启动后，自动发起连接
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    // 2. 如果连接断开，自动尝试重连
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        printf("Wi-Fi 断开，正在重连...\n");
        esp_wifi_connect();
    } 
    // 3. 成功获取到 IP 地址，说明真正连上网络了！
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        printf("🎉 恭喜！Wi-Fi 连接成功！\n");
        printf("🌐 分配的 IP 地址: " IPSTR "\n", IP2STR(&event->ip_info.ip));
        
        // 【你的水钟可以在这里启动 NTP 网络对时！】
    }
}

void wifi_init_sta(void)
{
    // 1. 初始化底层存储（Wi-Fi 依赖 NVS 保存配置）
    ESP_ERROR_CHECK(nvs_flash_init());
    // 2. 初始化 TCP/IP 协议栈和默认事件循环
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 3. 初始化 Wi-Fi 驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 4. 注册事件回调函数
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // 5. 配置 Wi-Fi 账号密码
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    // 6. 设置为 Station 模式（像手机一样连路由器），并启动
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    printf("📡 Wi-Fi 正在连接 %s ...\n", WIFI_SSID);
}

void app_main(void)
{
    wifi_init_sta();

}