#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h" 
#include "esp_log.h"      // 【新增】必须包含这个才能使用 ESP_LOGI
#include "app/lcd_display.h" 
#include "drivers/lcd_gc9a01.h"
#include "app/wifi_manager.h" 
#include "lvgl.h" 

// 【新增】定义 TAG，否则编译器不知道 TAG 是什么
static const char *TAG = "MAIN_TASK"; 

// --- LVGL 显示任务 ---
void task_text_display(void *pvParameters) {
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)pvParameters;
    (void)panel; // 避免未使用变量警告
    ESP_LOGI(TAG, "显示任务已启动，进入刷新循环");
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void app_main(void) {
    //wifi组件启动
    wifi_manager_init()
    
    esp_lcd_panel_handle_t panel = lcd_gc9a01_init();
    ESP_LOGI(TAG, "在 app_main 中初始化 LVGL，临时增加 WDT 超时至 10s");
    // 临时把 Task WDT 超时时间调大，防止 lv_init 在某些设备上较慢触发看门狗
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = 10000,
        .idle_core_mask = 0,
        .trigger_panic = false,
    };
    esp_err_t wrc = esp_task_wdt_reconfigure(&wdt_cfg);
    if (wrc != ESP_OK) {
        ESP_LOGW(TAG, "esp_task_wdt_init returned %d", wrc);
    }
    lcd_lvgl_init(panel);
    lcd_welcome_text1(panel);

    xTaskCreate(task_text_display, "TextTask", 8192, (void *)panel, 5, NULL);
}