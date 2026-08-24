#include "time_sync.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h" // 依然需要包含，因为 esp_netif_sntp_init 在这里声明

static const char *TAG = "TIME_SYNC";

// 时间同步事件组
EventGroupHandle_t time_event_group;
const int TIME_SYNCED_BIT = BIT0;

// 时间同步回调函数
static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "时间同步成功，当前时间戳: %ld", tv->tv_sec);
    xEventGroupSetBits(time_event_group, TIME_SYNCED_BIT);
}

void initialize_sntp(void)
{
    ESP_LOGI(TAG, "正在初始化 SNTP...");
    time_event_group = xEventGroupCreate();

    //1.设置中国时区
    setenv("TZ", "CST-8", 1);
    tzset();

    // 1. 使用官方宏生成默认配置（包含正确的字段和默认值）
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");

    // 2. 设置时间同步成功的回调函数
    config.sync_cb = time_sync_notification_cb; // 把回调函数放进配置里

    // 3. 启动 SNTP 服务
    esp_netif_sntp_init(&config);
}