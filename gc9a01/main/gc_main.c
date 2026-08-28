#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h" 
#include "esp_log.h"      
#include "app/lcd_display.h" 
#include "drivers/lcd_gc9a01.h"
#include "app/wifi_manager.h"
#include "lvgl.h" 
#include "app/time_sync.h"
#include <time.h> 

static const char *TAG = "MAIN_TASK"; 

// --- LVGL 显示任务（LVGL 的心脏，保持不变）---
void task_text_display(void *pvParameters) {
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)pvParameters;
    (void)panel; 
    ESP_LOGI(TAG, "显示任务已启动，进入刷新循环");
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void app_main(void) {
    // 1. 启动 Wi-Fi 和时间同步（后台非阻塞运行）
    wifi_manager_init();
    initialize_sntp();
    
    // 2. 初始化屏幕
    esp_lcd_panel_handle_t panel = lcd_gc9a01_init();
    
    // 3. 临时增加看门狗超时（保留你的防崩溃逻辑）
    ESP_LOGI(TAG, "初始化 LVGL，临时增加 WDT 超时至 10s");
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = 10000,
        .idle_core_mask = 0,
        .trigger_panic = false,
    };
    esp_err_t wrc = esp_task_wdt_reconfigure(&wdt_cfg);
    if (wrc != ESP_OK) {
        ESP_LOGW(TAG, "esp_task_wdt_init returned %d", wrc);
    }
    
    // 4. 初始化 LVGL 并创建纯指针表盘
    lcd_lvgl_init(panel);
    lcd_create_watch_ui(); 
    
    // 【已删除】：lcd_touch_init(); （屏幕无触摸功能）
    
    // 5. 启动 LVGL 刷新任务
    xTaskCreate(task_text_display, "Lvgl_Task", 8192, (void *)panel, 5, NULL);
    
    // 【已删除】：time_display_task 任务（纯指针表盘由 LVGL 内部定时器自动更新，无需外部搬运时间）
}