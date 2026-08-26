#include "lcd_display.h"
#include "lvgl.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "LCD_DISPLAY";

// 定义两个全局的容器对象，分别代表“表盘界面”和“数字界面”
static lv_obj_t *dial_container = NULL;
static lv_obj_t *digital_container = NULL;

// 记录当前处于哪个界面 (0: 表盘, 1: 数字)
static uint8_t current_ui = 0; 

// 指针式表盘相关的变量
static lv_obj_t *g_meter = NULL;
static lv_meter_indicator_t *ind_hour, *ind_min, *ind_sec;

// ================= 内部辅助函数 =================

// 1. 创建指针式表盘
static void create_analog_dial(lv_obj_t *parent) {
    g_meter = lv_meter_create(parent);
    lv_obj_center(g_meter);
    lv_obj_set_size(g_meter, 220, 220);

    lv_meter_scale_t *scale = lv_meter_add_scale(g_meter);
    lv_meter_set_scale_ticks(g_meter, scale, 60, 1, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(g_meter, scale, 5, 2, 20, lv_color_black(), 10);
    lv_meter_set_scale_range(g_meter, scale, 0, 360, 360, 0);

    ind_hour = lv_meter_add_needle_line(g_meter, scale, 4, lv_palette_main(LV_PALETTE_BLUE_GREY), -20);
    ind_min = lv_meter_add_needle_line(g_meter, scale, 3, lv_palette_main(LV_PALETTE_BLUE), -10);
    ind_sec = lv_meter_add_needle_line(g_meter, scale, 2, lv_palette_main(LV_PALETTE_RED), 0);
}

// 2. 创建数字时间界面
static void create_digital_clock(lv_obj_t *parent) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "00:00:00");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0); 
}

// 3. 定时器回调：更新表盘指针
static void update_clock_hands(lv_timer_t *t) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int32_t sec_angle = timeinfo.tm_sec * 6; 
    int32_t min_angle = timeinfo.tm_min * 6 + timeinfo.tm_sec / 10;
    int32_t hour_angle = (timeinfo.tm_hour % 12) * 30 + timeinfo.tm_min / 2;

    // 只有在表盘界面可见时才更新指针，节省 CPU 资源
    if (current_ui == 0 && g_meter != NULL) {
        lv_meter_set_indicator_value(g_meter, ind_sec, sec_angle);
        lv_meter_set_indicator_value(g_meter, ind_min, min_angle);
        lv_meter_set_indicator_value(g_meter, ind_hour, hour_angle);
    }
}

// 4. 【新增】手势回调函数（必须放在 lcd_create_watch_ui 前面）
static void watch_gesture_cb(lv_event_t *e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    // 向左或向右滑动时，触发切换
    if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) {
        lcd_toggle_watch_face(); 
    }
}

// ================= 对外暴露的接口 =================

// 初始化主界面
void lcd_create_watch_ui(void) {
    ESP_LOGI(TAG, "正在创建手表 UI...");
    lv_obj_t *scr = lv_screen_active();

    // 1. 创建表盘容器并添加控件
    dial_container = lv_obj_create(scr);
    lv_obj_set_size(dial_container, 240, 240);
    lv_obj_set_style_border_width(dial_container, 0, 0);
    lv_obj_set_style_bg_opa(dial_container, LV_OPA_TRANSP, 0); 
    create_analog_dial(dial_container);

    // 2. 创建数字容器并添加控件
    digital_container = lv_obj_create(scr);
    lv_obj_set_size(digital_container, 240, 240);
    lv_obj_set_style_border_width(digital_container, 0, 0);
    lv_obj_set_style_bg_opa(digital_container, LV_OPA_TRANSP, 0); 
    create_digital_clock(digital_container);

    // 3. 默认显示表盘，隐藏数字界面
    lv_obj_clear_flag(dial_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(digital_container, LV_OBJ_FLAG_HIDDEN);

    // 4. 【新增】为整个屏幕绑定滑动事件
    lv_obj_add_flag(scr, LV_OBJ_FLAG_SCROLLABLE); // 允许响应滑动
    lv_obj_add_event_cb(scr, watch_gesture_cb, LV_EVENT_GESTURE, NULL);

    // 5. 启动时间更新定时器 (每秒执行一次)
    lv_timer_create(update_clock_hands, 1000, NULL);
}

// 切换界面的核心函数
void lcd_toggle_watch_face(void) {
    if (current_ui == 0) {
        // 切换到数字界面
        lv_obj_add_flag(dial_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(digital_container, LV_OBJ_FLAG_HIDDEN);
        current_ui = 1;
        ESP_LOGI(TAG, "切换到数字界面");
    } else {
        // 切换到表盘界面
        lv_obj_add_flag(digital_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(dial_container, LV_OBJ_FLAG_HIDDEN);
        current_ui = 0;
        ESP_LOGI(TAG, "切换到表盘界面");
    }
}