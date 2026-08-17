#ifndef LCD_GC9A01_H
#define LCD_GC9A01_H

#include "esp_lcd_panel_ops.h"

// 定义屏幕分辨率常量，方便后续画图使用
#define LCD_WIDTH  240
#define LCD_HEIGHT 240

/**
 * @brief 初始化 GC9A01 屏幕
 * 
 * @return esp_lcd_panel_handle_t 返回屏幕句柄，用于后续画图
 */
esp_lcd_panel_handle_t lcd_gc9a01_init(void);

#endif