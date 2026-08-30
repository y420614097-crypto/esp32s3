#include "lcd_display.h"
#include "lvgl.h"
#include "drivers/lcd_gc9a01.h"
#include "esp_log.h"
#include <time.h>
#include "esp_heap_caps.h"  // 【新增】提供 heap_caps_malloc 和 MALLOC_CAP_DMA

static const char *TAG = "LCD_DISPLAY";

// 在 LVGL v9 中，指针是独立的 lv_obj_t 对象
static lv_obj_t *g_scale = NULL; 
static lv_obj_t *ind_hour = NULL, *ind_min = NULL, *ind_sec = NULL;


// LVGL v9 核心初始化函数
void lcd_lvgl_init(esp_lcd_panel_handle_t panel) {
    // 1. 初始化 LVGL 核心
    lv_init();

    // 2. 【核心修复】使用固定行数分配显存，避免计算错误
    // 建议至少分配屏幕高度的 1/4 到 1/3，对于 240x240 屏幕，60-80 行是安全值
    #define LVGL_BUF_LINES 60 
    static lv_color_t *buf1 = NULL;
    
    size_t buf_size = LCD_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t);
    
    // 尝试分配 DMA 内存
    buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    if (buf1 == NULL) {
        ESP_LOGW(TAG, "DMA malloc failed, trying internal RAM");
        // 如果 DMA 内存不足，尝试内部 RAM（速度稍慢但稳定）
        buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL);
    }
    
    if (buf1 == NULL) {
        ESP_LOGE(TAG, "CRITICAL: Failed to allocate LVGL buffer!");
        while(1); // 阻塞，防止后续崩溃
    }

    // 3. 创建 LVGL v9 显示驱动
    lv_display_t *disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    // 【如果画面变正了但是位置偏了，加这两行】
    lv_display_set_offset(disp, 0, 20); // 这里的数字根据实际偏移调整，比如 (20, 0)
    
    // 绑定刷新回调
    extern void lcd_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
    lv_display_set_flush_cb(disp, lcd_flush);
    
    // 【核心修复】单缓冲模式：第二个参数必须为 NULL
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    // lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_FULL);
    
    ESP_LOGI(TAG, "LVGL v9 init done. Buffer: %d lines (%d bytes)", LVGL_BUF_LINES, (int)buf_size);
}

// 1. 创建指针式表盘
static void create_analog_dial(lv_obj_t *parent) {
    g_scale = lv_scale_create(parent);
    lv_obj_center(g_scale);
    lv_obj_set_size(g_scale, 220, 220);

    // 设置为圆形内圈模式
    lv_scale_set_mode(g_scale, LV_SCALE_MODE_ROUND_INNER); 
    lv_scale_set_total_tick_count(g_scale, 60);
    lv_scale_set_major_tick_every(g_scale, 5);
    lv_scale_set_angle_range(g_scale, 360);
    lv_scale_set_rotation(g_scale, 0); // 0度在正上方
    
    // 【核心修正 1】：使用官方 v9 API 添加线条指针
    // 函数原型: lv_scale_set_line_needle_value(scale, needle_line, needle_length, value)
    // 注意：这个函数既用于创建/绑定指针，也用于设置初始值！
    
    // 创建时针 (长度40，初始值0)
    ind_hour = lv_line_create(g_scale); 
    lv_scale_set_line_needle_value(g_scale, ind_hour, 40, 0);
    lv_obj_set_style_line_color(ind_hour, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);
    lv_obj_set_style_line_width(ind_hour, 4, 0);

    // 创建分针 (长度60，初始值0)
    ind_min = lv_line_create(g_scale);
    lv_scale_set_line_needle_value(g_scale, ind_min, 60, 0);
    lv_obj_set_style_line_color(ind_min, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_line_width(ind_min, 3, 0);

    // 创建秒针 (长度80，初始值0)
    ind_sec = lv_line_create(g_scale);
    lv_scale_set_line_needle_value(g_scale, ind_sec, 80, 0);
    lv_obj_set_style_line_color(ind_sec, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_line_width(ind_sec, 2, 0);
}

// 2. 定时器回调：更新表盘指针
static void update_clock_hands(lv_timer_t *t) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int32_t sec_angle = timeinfo.tm_sec * 6; 
    int32_t min_angle = timeinfo.tm_min * 6 + timeinfo.tm_sec / 10;
    int32_t hour_angle = (timeinfo.tm_hour % 12) * 30 + timeinfo.tm_min / 2;

    // 【核心修正 2】：使用官方 v9 API 更新指针角度
    if (g_scale != NULL) {
        lv_scale_set_line_needle_value(g_scale, ind_sec, 80, sec_angle);
        lv_scale_set_line_needle_value(g_scale, ind_min, 60, min_angle);
        lv_scale_set_line_needle_value(g_scale, ind_hour, 40, hour_angle);
    }
}



// 3. 对外暴露的接口
void lcd_create_watch_ui(void) {
    ESP_LOGI(TAG, "正在创建手表 UI...");
    lv_obj_t *scr = lv_screen_active();

    lv_obj_t *dial_container = lv_obj_create(scr);
    lv_obj_set_size(dial_container, 240, 240);
    lv_obj_set_style_border_width(dial_container, 0, 0);
    lv_obj_set_style_bg_opa(dial_container, LV_OPA_TRANSP, 0); 
    // 把容器居中到屏幕，避免表盘出现轻微偏移
    lv_obj_center(dial_container);
    
    create_analog_dial(dial_container);
    lv_timer_create(update_clock_hands, 1000, NULL);
}