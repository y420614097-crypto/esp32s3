#include "lcd_display.h"
#include "lvgl.h"
#include "drivers/lcd_gc9a01.h"
#include "esp_log.h"
#include <time.h>
#include "esp_heap_caps.h"  // 提供 heap_caps_malloc 和 MALLOC_CAP_DMA

static const char *TAG = "LCD_DISPLAY";

/* ==================== 视图管理 ==================== */
typedef enum {
    VIEW_ANALOG = 0,    // 模拟指针表盘
    VIEW_DIGITAL,       // 数字时间
    VIEW_MENU,          // 表盘菜单
} ui_view_t;

// UI 视图对象（三个视图常驻，切换时隐藏/显示，避免重建开销）
static lv_obj_t *view_analog  = NULL;
static lv_obj_t *view_digital = NULL;
static lv_obj_t *view_menu    = NULL;
static ui_view_t current_view = VIEW_ANALOG;
static ui_view_t last_clock_view = VIEW_ANALOG;  // 从菜单返回时回到的时间视图

// 跨线程请求标志：按键任务写，LVGL 定时器读并消费
#define UI_REQ_NONE          0
#define UI_REQ_TOGGLE_CLOCK  1
#define UI_REQ_TOGGLE_MENU   2
static volatile int g_ui_request = UI_REQ_NONE;

// 模拟表盘控件（LVGL v9 中指针是独立的 lv_obj_t 对象）
static lv_obj_t *g_scale = NULL;
static lv_obj_t *ind_hour = NULL, *ind_min = NULL, *ind_sec = NULL;

// 数字时间控件
static lv_obj_t *lbl_time = NULL;
static lv_obj_t *lbl_date = NULL;

/* ==================== LVGL 核心初始化 ==================== */
void lcd_lvgl_init(esp_lcd_panel_handle_t panel) {
    (void)panel;  // panel 句柄已在 lcd_gc9a01_init 中保存，这里不用
    // 1. 初始化 LVGL 核心
    lv_init();

    // 2. 使用固定行数分配显存，避免计算错误
    #define LVGL_BUF_LINES 80
    static lv_color_t *buf1 = NULL;

    size_t buf_size = LCD_WIDTH * LVGL_BUF_LINES * sizeof(lv_color_t);

    // 尝试分配 DMA 内存
    buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    if (buf1 == NULL) {
        ESP_LOGW(TAG, "DMA malloc failed, trying internal RAM");
        buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL);
    }

    if (buf1 == NULL) {
        ESP_LOGE(TAG, "CRITICAL: Failed to allocate LVGL buffer!");
        while(1);
    }

    // 3. 创建 LVGL v9 显示驱动
    lv_display_t *disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_offset(disp, 0, 0);

    // 绑定刷新回调
    extern void lcd_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);
    lv_display_set_flush_cb(disp, lcd_flush);

    // 单缓冲模式
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "LVGL v9 init done. Buffer: %d lines (%d bytes)", LVGL_BUF_LINES, (int)buf_size);
}

/* ==================== 视图1：模拟指针表盘 ==================== */
static void create_analog_dial(lv_obj_t *parent) {
    g_scale = lv_scale_create(parent);
    lv_obj_set_size(g_scale, 240, 240);
    lv_obj_center(g_scale);

    // 设置为圆形内圈模式
    lv_scale_set_mode(g_scale, LV_SCALE_MODE_ROUND_INNER);

    // 显式设为圆形并裁剪角，避免容器/边界裁切导致显示成半圆
    lv_obj_set_style_radius(g_scale, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(g_scale, true, 0);
    lv_obj_set_style_bg_opa(g_scale, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_scale, 0, 0);
    // 清除默认 padding（默认 16px 会导致 scale 内容被压缩）
    lv_obj_set_style_pad_all(g_scale, 0, 0);

    // 关闭文字标签（不需要数字）
    lv_scale_set_label_show(g_scale, false);

    lv_scale_set_total_tick_count(g_scale, 60);
    lv_scale_set_major_tick_every(g_scale, 5);

    // 显式设置 tick 长度，默认为 0 会导致看不到刻度
    // LV_PART_ITEMS = minor tick，LV_PART_INDICATOR = major tick
    lv_obj_set_style_length(g_scale, 6, LV_PART_ITEMS);
    lv_obj_set_style_length(g_scale, 12, LV_PART_INDICATOR);
    // tick 颜色
    lv_obj_set_style_line_color(g_scale, lv_palette_main(LV_PALETTE_GREY), LV_PART_ITEMS);
    lv_obj_set_style_line_color(g_scale, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_line_width(g_scale, 1, LV_PART_ITEMS);
    lv_obj_set_style_line_width(g_scale, 2, LV_PART_INDICATOR);
    // 主圆弧宽度
    lv_obj_set_style_arc_width(g_scale, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_color(g_scale, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);

    // 360 度全圆周
    lv_scale_set_angle_range(g_scale, 360);
    // LVGL 中 0 度在 3 点钟方向，需要 +270 度让 0 度（最小值）指向 12 点钟
    lv_scale_set_rotation(g_scale, 270);
    // range 设为 0~60，方便指针值直接用时间分量
    lv_scale_set_range(g_scale, 0, 60);

    // 创建指针：长度不能超过 scale 半径（240/2=120）
    ind_hour = lv_line_create(g_scale);
    lv_scale_set_line_needle_value(g_scale, ind_hour, 60, 0);
    lv_obj_set_style_line_color(ind_hour, lv_palette_main(LV_PALETTE_BLUE_GREY), 0);
    lv_obj_set_style_line_width(ind_hour, 5, 0);
    lv_obj_set_style_line_rounded(ind_hour, true, 0);
    lv_obj_set_style_line_opa(ind_hour, LV_OPA_COVER, 0);

    ind_min = lv_line_create(g_scale);
    lv_scale_set_line_needle_value(g_scale, ind_min, 85, 0);
    lv_obj_set_style_line_color(ind_min, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_line_width(ind_min, 3, 0);
    lv_obj_set_style_line_rounded(ind_min, true, 0);
    lv_obj_set_style_line_opa(ind_min, LV_OPA_COVER, 0);

    ind_sec = lv_line_create(g_scale);
    lv_scale_set_line_needle_value(g_scale, ind_sec, 100, 0);
    lv_obj_set_style_line_color(ind_sec, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_line_width(ind_sec, 2, 0);
    lv_obj_set_style_line_rounded(ind_sec, true, 0);
    lv_obj_set_style_line_opa(ind_sec, LV_OPA_COVER, 0);
}

// 指针更新定时器：把时间映射到 scale 的 0~60 值域
static void update_clock_hands(lv_timer_t *t) {
    (void)t;
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // scale range = 0~60，value 直接用 0~60 范围
    int32_t sec_val  = timeinfo.tm_sec;
    int32_t min_val  = timeinfo.tm_min;
    // 时针：12 小时 = 60 刻度，每小时 5 格，每分钟约 1/12 格
    int32_t hour_val = (timeinfo.tm_hour % 12) * 5 + timeinfo.tm_min / 12;

    if (g_scale != NULL) {
        lv_scale_set_line_needle_value(g_scale, ind_sec,  100, sec_val);
        lv_scale_set_line_needle_value(g_scale, ind_min,   85, min_val);
        lv_scale_set_line_needle_value(g_scale, ind_hour,  60, hour_val);
    }
}

/* ==================== 视图2：数字时间 ==================== */
static void create_digital_clock(lv_obj_t *parent) {
    // 时间标签（大字体，居中）
    lbl_time = lv_label_create(parent);
    lv_label_set_text(lbl_time, "--:--:--");
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_time, lv_color_white(), 0);
    lv_obj_set_style_text_align(lbl_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_time);
    lv_obj_set_y(lbl_time, -25);

    // 日期标签（英文缩写，默认字体不含中文）
    lbl_date = lv_label_create(parent);
    lv_label_set_text(lbl_date, "----/--/--");
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_date, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_text_align(lbl_date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(lbl_date, lbl_time, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
}

// 数字时间刷新定时器（1 秒）：时间来自 WiFi SNTP 同步的系统时钟
static void update_digital_clock(lv_timer_t *t) {
    (void)t;
    if (current_view != VIEW_DIGITAL) return;  // 只在数字视图可见时刷新

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    static const char *wday_en[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    lv_label_set_text_fmt(lbl_time, "%02d:%02d:%02d",
                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    lv_label_set_text_fmt(lbl_date, "%04d/%02d/%02d %s",
                          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                          wday_en[timeinfo.tm_wday]);
}

/* ==================== 视图3：表盘菜单（占位，后续添加内容） ==================== */
static void create_menu(lv_obj_t *parent) {
    // 菜单标题
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Menu");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
    // 标题下划线
    lv_obj_set_style_border_width(title, 0, 0);

    // 菜单占位提示（后续在此容器内添加菜单项）
    lv_obj_t *placeholder = lv_label_create(parent);
    lv_label_set_text(placeholder, "(empty)");
    lv_obj_set_style_text_font(placeholder, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(placeholder, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_center(placeholder);
}

/* ==================== 视图切换 ==================== */
static void show_view(ui_view_t target) {
    lv_obj_t *views[] = {view_analog, view_digital, view_menu};
    for (int i = 0; i < 3; i++) {
        if (i == (int)target) {
            lv_obj_remove_flag(views[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(views[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    current_view = target;
    ESP_LOGI(TAG, "switch to view: %s",
             target == VIEW_ANALOG ? "ANALOG" : (target == VIEW_DIGITAL ? "DIGITAL" : "MENU"));
}

// UI 请求处理定时器（100ms）：在 LVGL 线程中安全执行视图切换
static void ui_request_handler(lv_timer_t *t) {
    (void)t;
    int req = g_ui_request;
    if (req == UI_REQ_NONE) return;
    g_ui_request = UI_REQ_NONE;  // 立即消费，防止重复处理

    if (req == UI_REQ_TOGGLE_CLOCK) {
        if (current_view == VIEW_MENU) {
            // 菜单里按切换键：先退出菜单回到时间视图
            show_view(last_clock_view);
        } else {
            ui_view_t target = (current_view == VIEW_ANALOG) ? VIEW_DIGITAL : VIEW_ANALOG;
            last_clock_view = target;
            show_view(target);
        }
    } else if (req == UI_REQ_TOGGLE_MENU) {
        if (current_view == VIEW_MENU) {
            show_view(last_clock_view);          // 再按一次：退出菜单
        } else {
            last_clock_view = current_view;      // 记住来源视图
            show_view(VIEW_MENU);
        }
    }
}

/* ==================== 对外接口（跨线程调用安全） ==================== */
void lcd_ui_request_toggle_clock(void) {
    g_ui_request = UI_REQ_TOGGLE_CLOCK;
}

void lcd_ui_request_toggle_menu(void) {
    g_ui_request = UI_REQ_TOGGLE_MENU;
}

/* ==================== 创建全部 UI ==================== */
void lcd_create_watch_ui(void) {
    ESP_LOGI(TAG, "正在创建手表 UI（三视图）...");
    lv_obj_t *scr = lv_screen_active();

    // 清理屏幕默认样式，避免默认 padding 影响布局
    lv_obj_set_style_bg_opa(scr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);

    // ---- 视图1：模拟表盘 ----
    view_analog = lv_obj_create(scr);
    lv_obj_set_size(view_analog, 240, 240);
    lv_obj_set_style_border_width(view_analog, 0, 0);
    lv_obj_set_style_bg_opa(view_analog, LV_OPA_TRANSP, 0);
    // 容器清 padding，否则 scale 子对象会被压到内容区
    lv_obj_set_style_pad_all(view_analog, 0, 0);
    lv_obj_set_style_radius(view_analog, 0, 0);
    lv_obj_remove_flag(view_analog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(view_analog);
    create_analog_dial(view_analog);

    // ---- 视图2：数字时间 ----
    view_digital = lv_obj_create(scr);
    lv_obj_set_size(view_digital, 240, 240);
    lv_obj_set_style_border_width(view_digital, 0, 0);
    lv_obj_set_style_bg_opa(view_digital, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(view_digital, 0, 0);
    lv_obj_set_style_radius(view_digital, 0, 0);
    lv_obj_remove_flag(view_digital, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(view_digital);
    create_digital_clock(view_digital);

    // ---- 视图3：菜单 ----
    view_menu = lv_obj_create(scr);
    lv_obj_set_size(view_menu, 240, 240);
    lv_obj_set_style_border_width(view_menu, 0, 0);
    lv_obj_set_style_bg_opa(view_menu, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(view_menu, 0, 0);
    lv_obj_set_style_radius(view_menu, 0, 0);
    lv_obj_remove_flag(view_menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(view_menu);
    create_menu(view_menu);

    // 初始状态：只显示模拟表盘
    lv_obj_add_flag(view_digital, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_menu, LV_OBJ_FLAG_HIDDEN);
    current_view = VIEW_ANALOG;
    last_clock_view = VIEW_ANALOG;

    // 立即刷新一次指针，避免启动时停在 0 度
    update_clock_hands(NULL);

    // 注册定时器：指针 1s；数字钟 1s；UI 请求轮询 100ms
    lv_timer_create(update_clock_hands, 1000, NULL);
    lv_timer_create(update_digital_clock, 1000, NULL);
    lv_timer_create(ui_request_handler, 100, NULL);
}
