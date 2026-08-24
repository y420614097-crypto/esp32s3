#include "lcd_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "LCD_DISPLAY";
// 全局变量
static lv_obj_t *g_time_label = NULL;

// 更新时间的接口
void lcd_update_time(const char *time_str) {
    if (g_time_label != NULL) {
        lv_label_set_text(g_time_label, time_str);
    }
}

// ================= LVGL v9 刷新回调函数 =================
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{

    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    
    // 注意：area->x2 和 y2 是包含在内的（inclusive），esp_lcd_panel_draw_bitmap 也接受包含的结束坐标，
    // 因此不应再对 x2/y2 额外 +1，否则会导致绘制坐标越界或不显示。
    int width = area->x2 - area->x1 + 1;
    int height = area->y2 - area->y1 + 1;
    (void)width; (void)height;

    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2, area->y2, px_map);
    
    // 告诉 LVGL 刷新完成
    lv_display_flush_ready(disp);
}

// ================= LVGL 核心初始化 =================
void lcd_lvgl_init(esp_lcd_panel_handle_t panel)
{
    ESP_LOGI(TAG, "正在初始化 LVGL v9...");
    lv_init();

    lv_display_t *disp = lv_display_create(240, 240);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_user_data(disp, panel);
    
    // 【关键修复】：退回使用内部 DMA 内存，大小设为屏幕的 1/10
    // 240 * 240 / 10 * 2 = 11520 字节，内部 RAM 绝对够用！
    uint32_t buf_size = 240 * 240 / 10 * sizeof(lv_color_t);
    lv_color_t *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    
    if (!buf1) {
        ESP_LOGE(TAG, "LVGL 显存分配失败！");
        return;
    }
    memset(buf1, 0x00, buf_size); // 清零显存，防止花屏

    // 退回局部刷新模式
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    ESP_LOGI(TAG, "LVGL v9 初始化成功，显存大小: %d bytes", buf_size);
}

// ================= 欢迎界面 1（只负责创建控件）=================
void lcd_welcome_text(esp_lcd_panel_handle_t panel)
{
    ESP_LOGI(TAG, "开始绘制 LVGL 欢迎界面 1...");
    lv_obj_t *scr = lv_screen_active();
    
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Hello, ESP32!\nWelcome!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0); 

    // 创建时间标签
    g_time_label = lv_label_create(scr);
    lv_label_set_text(g_time_label, "Syncing Time...");
    lv_obj_align(g_time_label, LV_ALIGN_CENTER, 0, 30); // 放在下方
    lv_obj_set_style_text_color(g_time_label, lv_color_hex(0x00FF00), 0);
}

void lcd_welcome_text1(esp_lcd_panel_handle_t panel)
{
    ESP_LOGI(TAG, "开始绘制 LVGL 欢迎界面 2...");
    lv_obj_t *scr = lv_screen_active();

    // 创建全屏背景
    lv_obj_t *bg = lv_obj_create(scr);
    lv_obj_set_size(bg, 240, 240);
    
    // 【测试用】强制设置为纯红色 (0xFF0000)
    // 如果颜色反转正确，你应该看到鲜艳的红色
    lv_obj_set_style_bg_color(bg, lv_color_hex(0xFF0000), 0); 
    lv_obj_set_style_border_width(bg, 0, 0);
    lv_obj_align(bg, LV_ALIGN_CENTER, 0, 0);

    // 创建文字
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Hello ESP32!\nFont OK!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    
    // 设置文字为白色，确保在红色背景上可见
    lv_obj_set_style_text_color(label, lv_color_white(), 0); 
}