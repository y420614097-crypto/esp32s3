#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "esp_lcd_panel_ops.h"

// 声明函数1：跑马灯颜色测试
void lcd_run_color_test(esp_lcd_panel_handle_t panel);

// 声明函数2：LVGL v9 核心初始化（必须调用）
void lcd_lvgl_init(esp_lcd_panel_handle_t panel);

// 声明函数3：显示欢迎界面 1
void lcd_welcome_text(esp_lcd_panel_handle_t panel);

// 声明函数4：显示欢迎界面 2
void lcd_welcome_text1(esp_lcd_panel_handle_t panel);

#endif