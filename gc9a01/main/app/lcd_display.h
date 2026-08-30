#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "esp_lcd_panel_ops.h"


// 声明函数2：LVGL v9 核心初始化（必须调用）
void lcd_lvgl_init(esp_lcd_panel_handle_t panel);

// 【新增】声明函数5：创建纯指针式表盘 UI
void lcd_create_watch_ui(void);

#endif