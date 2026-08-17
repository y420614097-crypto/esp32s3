#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "esp_lcd_panel_ops.h"

// 声明函数1：跑马灯颜色测试
void lcd_run_color_test(esp_lcd_panel_handle_t panel);

// 声明函数2：显示一个静态的欢迎界面
void lcd_welcome_text(esp_lcd_panel_handle_t panel);

// 声明函数3：带欢迎文本的跑马灯
void lcd_welcome_text1(esp_lcd_panel_handle_t panel);


#endif