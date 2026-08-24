#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h" 
#include "esp_log.h"      // 【新增】必须包含这个才能使用 ESP_LOGI
#include "app/lcd_display.h" 
#include "drivers/lcd_gc9a01.h"
#include "app/wifi_manager.h"
#include "lvgl.h" 
#include "app/time_sync.h"
#include <time.h> // 1. 引入 time.h 库，用于处理时间

// 【新增】定义 TAG，否则编译器不知道 TAG 是什么
static const char *TAG = "MAIN_TASK"; 


//
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

// 时间搬运工任务
void time_display_task(void *pvParameter) {
    // 等待时间同步成功
    EventBits_t bits = xEventGroupWaitBits(time_event_group, TIME_SYNCED_BIT, pdFALSE, pdTRUE, portMAX_DELAY,pdMS_TO_TICKS(5000));
    if(bits & TIME_SYNCED_BIT) {
        ESP_LOGI(TAG, "时间同步成功，开始显示时间！");
    } else {
        ESP_LOGW(TAG, "等待时间同步超时，继续尝试显示时间！");
    }

    char time_str[32];
    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(time_str, sizeof(time_str), '%H:%M:%S', &timeinfo);
        
        // 把时间塞给 LVGL（LVGL 内部有锁，这里调用是安全的）
        lcd_update_time(time_str);
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1秒更新一次
    }
}

void app_main(void) {
    //wifi组件启动
    wifi_manager_init();
    // 启动时间同步任务
    initialize_sntp();
    // 等待时间同步完成
    xEventGroupWaitBits(time_event_group, TIME_SYNCED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "时间已就绪，可以开始显示时间了！");
    
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

    xTaskCreate(task_text_display, "Lvgl_Task", 8192, (void *)panel, 5, NULL);
    xTaskCreate(time_display_task, "Time_Task", 4096, null, 5, NULL);
}