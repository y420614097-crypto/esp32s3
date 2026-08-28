#include "lcd_display.h"
#include "lvgl.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "LCD_DISPLAY";

// 指针式表盘相关的变量
static lv_obj_t *g_meter = NULL;
static lv_meter_indicator_t *ind_hour, *ind_min, *ind_sec;

// ================= 内部辅助函数 =================

// 1. 创建指针式表盘
static void create_analog_dial(lv_obj_t *parent) {
    g_meter = lv_meter_create(parent);
    lv_obj_center(g_meter);
    lv_obj_set_size(g_meter, 220, 220);

    // 设置刻度盘样式
    lv_meter_scale_t *scale = lv_meter_add_scale(g_meter);
    lv_meter_set_scale_ticks(g_meter, scale, 60, 1, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(g_meter, scale, 5, 2, 20, lv_color_black(), 10);
    lv_meter_set_scale_range(g_meter, scale, 0, 360, 360, 0);

    // 添加时针、分针、秒针
    ind_hour = lv_meter_add_needle_line(g_meter, scale, 4, lv_palette_main(LV_PALETTE_BLUE_GREY), -20);
    ind_min = lv_meter_add_needle_line(g_meter, scale, 3, lv_palette_main(LV_PALETTE_BLUE), -10);
    ind_sec = lv_meter_add_needle_line(g_meter, scale, 2, lv_palette_main(LV_PALETTE_RED), 0);
}

// 2. 定时器回调：更新表盘指针
static void update_clock_hands(lv_timer_t *t) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // 计算角度
    int32_t sec_angle = timeinfo.tm_sec * 6; 
    int32_t min_angle = timeinfo.tm_min * 6 + timeinfo.tm_sec / 10;
    int32_t hour_angle = (timeinfo.tm_hour % 12) * 30 + timeinfo.tm_min / 2;

    if (g_meter != NULL) {
        lv_meter_set_indicator_value(g_meter, ind_sec, sec_angle);
        lv_meter_set_indicator_value(g_meter, ind_min, min_angle);
        lv_meter_set_indicator_value(g_meter, ind_hour, hour_angle);
    }
}

// ================= 对外暴露的接口 =================

void lcd_create_watch_ui(void) {
    ESP_LOGI(TAG, "正在创建手表 UI...");
    lv_obj_t *scr = lv_screen_active();

    // 直接在屏幕上创建表盘容器
    lv_obj_t *dial_container = lv_obj_create(scr);
    lv_obj_set_size(dial_container, 240, 240);
    lv_obj_set_style_border_width(dial_container, 0, 0);
    lv_obj_set_style_bg_opa(dial_container, LV_OPA_TRANSP, 0); 
    
    // 创建并居中表盘
    create_analog_dial(dial_container);

    // 启动时间更新定时器 (每秒执行一次)
    lv_timer_create(update_clock_hands, 1000, NULL);
}