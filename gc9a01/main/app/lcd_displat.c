#include "lcd_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "LCD_DISPLAY";

// ================= 跑马灯任务 =================
void lcd_run_color_test(esp_lcd_panel_handle_t panel)
{
    // 【修复3】使用静态数组或全局变量，防止栈溢出
    // 注意：如果内存不够，可以减小缓冲区大小，分块刷新
    static uint16_t color_buf[240 * 240]; 
    uint16_t test_colors[] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000};
    int color_count = sizeof(test_colors) / sizeof(test_colors[0]);

    while (1) {
        for (int i = 0; i < color_count; i++) {
            for (int j = 0; j < 240 * 240; j++) {
                color_buf[j] = test_colors[i];
            }
            esp_lcd_panel_draw_bitmap(panel, 0, 0, 240, 240, color_buf);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// ================= LVGL 刷新回调函数（核心！）=================
// 当 LVGL 画好一帧后，会调用这个函数把数据发给屏幕
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t) drv->user_data;
    
    // 将 LVGL 的坐标和颜色数据传给底层驱动
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
    
    // 告诉 LVGL 刷新完成
    lv_disp_flush_ready(drv);
}

// ================= 欢迎界面任务 =================
void lcd_welcome_text(esp_lcd_panel_handle_t panel)
{
    ESP_LOGI(TAG, "开始绘制 LVGL 欢迎界面...");

    // 1. 初始化 LVGL
    lv_init();

    // 2. 【修复2】配置 LVGL 的显示驱动，绑定底层屏幕句柄
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[240 * 40]; // LVGL 局部刷新缓冲，40行足够
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 240 * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = panel; // 把屏幕句柄传给回调函数
    lv_disp_drv_register(&disp_drv);

    // 3. 创建文本标签
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello, ESP32!\nWelcome!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);

    // 4. LVGL 心跳循环
    while(1) {
        lv_timer_handler(); 
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void lcd_welcome_text1(esp_lcd_panel_handle_t panel)
{
    lv_init();

    // 1. 配置 LVGL 显示驱动（和之前一样）
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[240 * 40];
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 240 * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = panel;
    lv_disp_drv_register(&disp_drv);

    // 2. 创建背景对象
    lv_obj_t *bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bg, 240, 240);
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x0000FF), 0); // 初始蓝色
    lv_obj_set_style_border_width(bg, 0, 0);
    lv_obj_align(bg, LV_ALIGN_CENTER, 0, 0);

    // 3. 创建文本对象（在背景之上）
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello, ESP32!\nWelcome!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0); // 白色字体
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);

    // 4. 颜色数组和索引
    uint32_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000};
    int color_count = 5;
    int color_index = 0;

    // 5. 统一的心跳循环
    while(1) {
        // 每隔 1 秒切换一次背景颜色
        lv_obj_set_style_bg_color(bg, lv_color_hex(colors[color_index]), 0);
        color_index = (color_index + 1) % color_count;

        // LVGL 处理刷新（文字和背景会一起被画出来）
        lv_timer_handler(); 
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1秒换一次颜色
    }
}