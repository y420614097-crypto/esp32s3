
// ============================================================
// ESP32 + GC9A01 + Wi-Fi + NTP + 实时时间显示 完整代码
// 标注说明：
//   【新增】 = 从"显示时间代码"合并过来的新内容
//   【修改】 = 在原代码基础上做了调整
// ============================================================

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_gc9a01.h"
#include <stdint.h>

// ============================================================
// 【新增】第1步：引入 FreeRTOS 任务头文件
// 原因：显示时间需要一个独立的后台任务循环刷新屏幕
// ============================================================
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WIFI_SSID      "1102"
#define WIFI_PASSWORD  "86279190"

// ================= 屏幕引脚配置 =================
#define PIN_NUM_RST   9
#define PIN_NUM_CS    46
#define PIN_NUM_DC    3
#define PIN_NUM_MOSI  8
#define PIN_NUM_SCLK  18

#define LCD_H_RES     240
#define LCD_V_RES     240
#define SPI_FREQ_HZ   (40 * 1000 * 1000) // 40MHz

// 全局屏幕句柄，后续给 LVGL 用
esp_lcd_panel_handle_t panel_handle = NULL;

// ============================================================
// 【新增】第2步：添加"上一次显示的时间"变量
// 原因：只有时间真正变化时才刷新屏幕，防止闪烁
// ============================================================
static char last_time_str[9] = {0};

// 极简数字字体：只包含 '0'-'9' 和 ':'，每字符 8x8 像素，按行存储（8 字节）
static const uint8_t font_digits[][8] = {
    {0x3C,0x66,0x6E,0x7E,0x76,0x66,0x3C,0x00}, // 0
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 1
    {0x3C,0x66,0x06,0x0C,0x30,0x60,0x7E,0x00}, // 2
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // 3
    {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x1E,0x00}, // 4
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // 5
    {0x3C,0x60,0x60,0x7C,0x66,0x66,0x3C,0x00}, // 6
    {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00}, // 7
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // 8
    {0x3C,0x66,0x66,0x3E,0x06,0x06,0x3C,0x00}, // 9
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // :
};

// 简单的像素绘制：每像素通过 esp_lcd_panel_fill 填充 1x1 矩形
void esp_lcd_text_draw_string(esp_lcd_panel_handle_t panel, const char *str, int x, int y, uint16_t color)
{
    int cursor_x = x;
    while (*str) {
        char c = *str++;
        const uint8_t *bmp = NULL;
        if (c >= '0' && c <= '9') bmp = font_digits[c - '0'];
        else if (c == ':') bmp = font_digits[10];
        else { // 不支持的字符，绘制空格
            cursor_x += 8; continue;
        }
        for (int row = 0; row < 8; row++) {
            uint8_t bits = bmp[row];
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80 >> col)) {
                    esp_lcd_panel_fill(panel, cursor_x + col, y + row, cursor_x + col, y + row, color);
                }
            }
        }
        cursor_x += 8; // 每字符宽度 8
    }
}

// ================= 极简屏幕初始化函数 =================
void init_gc9a01_display(void) {
    // 1. 初始化 SPI 总线
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,          // 屏幕不需要 MISO
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 2. 创建屏幕 IO 句柄
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = SPI_FREQ_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    // 3. 创建 GC9A01 面板句柄
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));

    // 4. 初始化并点亮屏幕
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, true); // GC9A01 通常需要颜色反转
    esp_lcd_panel_disp_on_off(panel_handle, true);  // 开启显示

    // ============================================================
    // 【新增】第3步：屏幕初始化后先清空为黑色背景
    // 原因：避免屏幕上残留随机噪点，让时间显示更干净
    // ============================================================
    esp_lcd_panel_fill(panel_handle, 0, 0, 239, 239, 0x0000);

    printf("️ GC9A01 屏幕初始化成功！\n");
}

// ============================================================
// 【新增】第4步：创建屏幕刷新任务（核心！）
// 这是一个独立的 FreeRTOS 任务，循环获取当前时间并刷新到屏幕
// ============================================================
void display_task(void *pvParameters) {
    char time_str[9];
    struct tm timeinfo;

    // 240x240 屏幕居中显示 "HH:MM:SS"（默认字体 16x26 像素）
    // 时间字符串宽 8 个字符 × 16 = 128 像素
    // X 居中: (240 - 128) / 2 = 56
    // Y 居中: (240 - 26) / 2 = 107
    int x_pos = 56;
    int y_pos = 107;

    while (1) {
        // 1. 获取当前本地时间
        time_t now;
        time(&now);
        localtime_r(&now, &timeinfo);

        // 2. 格式化为 "HH:MM:SS"
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);

        // 3. 【关键】只有时间变化时才刷新屏幕，避免闪烁
        if (strcmp(time_str, last_time_str) != 0) {
            // 先擦除旧时间区域（填充黑色）
            esp_lcd_panel_fill(panel_handle, x_pos, y_pos, x_pos + 127, y_pos + 25, 0x0000);

            // 绘制新时间（白色字体 0xFFFF）
            esp_lcd_text_draw_string(panel_handle, time_str, x_pos, y_pos, 0xFFFF);

            // 记录本次时间，下次对比用
            strcpy(last_time_str, time_str);
        }

        // 每 500ms 检查一次，保证秒数切换及时
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ================= Wi-Fi 与 NTP 逻辑 =================
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        printf("Wi-Fi 断开，正在重连...\n");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        printf(" Wi-Fi 连接成功！IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));

        printf(" 正在同步网络时间...\n");
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "ntp.aliyun.com");
        esp_sntp_init();
    }
}

void time_sync_notification_cb(struct timeval *tv) {
    printf("✅ 网络时间同步成功！\n");
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    printf(" 当前北京时间: %s\n", strftime_buf);
}

void wifi_init_sta(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    wifi_config_t wifi_config = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASSWORD } };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    printf(" Wi-Fi 正在连接 %s ...\n", WIFI_SSID);
}

// ================= 主程序 =================
void app_main(void) {
    // 1. 设置时区为北京时间 (东八区 UTC+8)
    setenv("TZ", "CST-8", 1);
    tzset();
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    // 2. 初始化屏幕
    init_gc9a01_display();

    // 3. 启动 Wi-Fi
    wifi_init_sta();

    // ============================================================
    // 【新增】第5步：创建显示任务
    // 在后台独立运行，不阻塞 Wi-Fi 和 NTP
    // 栈大小 4096 字节，优先级 5
    // ============================================================
    xTaskCreate(display_task, "display_task", 4096, NULL, 5, NULL);
}
