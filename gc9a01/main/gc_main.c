#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "app/lcd_display.h"
#include "drivers/lcd_gc9a01.h"
#include "drivers/button_ctrl.h"
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

// 按键回调：在按键任务上下文执行，只设置 UI 请求标志（线程安全）
static void on_button_pressed(button_id_t btn) {
    switch (btn) {
    case BUTTON_ID_TIME_SWITCH:
        // 按钮1：模拟表盘 <-> 数字时间
        lcd_ui_request_toggle_clock();
        break;
    case BUTTON_ID_MENU:
        // 按钮2：打开/关闭菜单
        lcd_ui_request_toggle_menu();
        break;
    default:
        break;
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

    // 4. 初始化 LVGL 并创建三视图 UI（模拟表盘/数字时间/菜单）
    lcd_lvgl_init(panel);
    lcd_create_watch_ui();

    // 5. 初始化按键：GPIO47=切换表盘/数字，GPIO4=菜单
    button_ctrl_init(on_button_pressed);

    // 6. 启动 LVGL 刷新任务
    xTaskCreate(task_text_display, "Lvgl_Task", 8192, (void *)panel, 5, NULL);
}