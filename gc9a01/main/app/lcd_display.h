#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "esp_lcd_panel_ops.h"

// 声明函数2：LVGL v9 核心初始化（必须调用）
void lcd_lvgl_init(esp_lcd_panel_handle_t panel);

// 创建全部 UI 视图（模拟表盘 / 数字时间 / 菜单）
void lcd_create_watch_ui(void);

// 【线程安全】请求切换 模拟表盘 <-> 数字时间（可在按键任务等上下文调用）
// 内部只设置标志位，实际切换由 LVGL 定时器执行
void lcd_ui_request_toggle_clock(void);

// 【线程安全】请求 打开/关闭 菜单视图（可在按键任务等上下文调用）
void lcd_ui_request_toggle_menu(void);

#endif
