#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app/lcd_display.h" // 引入你自己的头文件
#include "drivers/lcd_gc9a01.h"

// --- 任务1的入口函数 ---
void task_color_test(void *pvParameters) {
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)pvParameters;
    lcd_run_color_test(panel); // 调用你在 lcd_display.c 里写的函数
}

// --- 任务2的入口函数 (比如你想同时让屏幕显示文字) ---
void task_text_display(void *pvParameters) {
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)pvParameters;
        lcd_welcome_text(panel);
}
// --- 任务3的入口函数 (比如你想同时让屏幕显示文字) ---
void task_text1_display(void *pvParameters) {
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)pvParameters;
        lcd_welcome_text1(panel);
}

void app_main(void) {
    // 1. 初始化硬件
    esp_lcd_panel_handle_t panel = lcd_gc9a01_init();

    // 2. 创建任务 A (颜色测试)
    xTaskCreate(task_color_test, "ColorTask", 4096, (void*)panel, 5, NULL);

    // 3. 创建任务 B (显示)
    xTaskCreate(task_text_display, "TextTask", 4096, (void*)panel, 5, NULL);

    // 3. 创建任务 c (其他逻辑)
    xTaskCreate(task_text1_display, "TextTask1", 4096, (void*)panel, 5, NULL);

    // app_main 到这里就结束了，但两个任务会在后台一直跑！
}