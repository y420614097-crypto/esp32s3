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

// 时间搬运工任务
void time_display_task(void *pvParameter) {
    // 【修复 1】：正确的 5 秒超时等待写法（去掉了多余的参数）
    EventBits_t bits = xEventGroupWaitBits(
        time_event_group, 
        TIME_SYNCED_BIT, 
        pdFALSE, 
        pdTRUE, 
        pdMS_TO_TICKS(5000)  // 5秒超时
    );

    if(bits & TIME_SYNCED_BIT) {
        ESP_LOGI(TAG, "时间同步成功，开始显示时间！");
    } else {
        ESP_LOGW(TAG, "等待时间同步超时，使用本地 RTC 时间！");
    }

    char time_str[32];
    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // 【修复 2】：将单引号改为双引号
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
        
        lcd_update_time(time_str);
        vTaskDelay(pdMS_TO_TICKS(1000)); 
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
    
    // 4. 初始化 LVGL 并创建表盘
    lcd_lvgl_init(panel);
    lcd_create_watch_ui(); 
    
    // 【修复 3】：启动触摸初始化（如果你已经写了 lcd_touch_init）
    // 如果你还没写，可以先注释掉这行，用定时器测试
    lcd_touch_init(); 
    
    // 5. 启动 LVGL 刷新任务
    xTaskCreate(task_text_display, "Lvgl_Task", 8192, (void *)panel, 5, NULL);
    
    // 6. 启动时间更新任务
    xTaskCreate(time_display_task, "Time_Task", 4096, NULL, 5, NULL); 
    
    // 【删除了】：auto_switch_timer_cb 和 lv_timer_create(auto_switch_timer_cb...)
    // 因为现在由你在 lcd_display.c 中绑定的 LV_EVENT_GESTURE 手势事件来控制切换了！
}