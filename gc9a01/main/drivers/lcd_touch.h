#include "esp_lcd_touch_cst816s.h"
#include "driver/i2c.h" // 必须引入 I2C 驱动头文件


// 触摸相关的句柄
static esp_lcd_touch_handle_t g_touch_handle = NULL;
static lv_indev_t *g_touch_indev = NULL;

// LVGL 读取触摸坐标的回调函数
static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    uint16_t touch_x, touch_y;
    uint8_t touch_cnt = 0;
    
    esp_lcd_touch_read_data(g_touch_handle); 
    bool touched = esp_lcd_touch_get_coordinates(g_touch_handle, &touch_x, &touch_y, NULL, &touch_cnt, 1);
    
    if (touched && touch_cnt > 0) {
        data->point.x = touch_x;
        data->point.y = touch_y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// 触摸初始化函数
void lcd_touch_init(void) {
    ESP_LOGI(TAG, "正在初始化 CST816S 触摸...");

    // 1. 初始化 I2C 总线（使用你定义的 SDA 和 SCL 引脚）
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_NUM_TOUCH_SDA,  // 设置 SDA 引脚
        .scl_io_num = PIN_NUM_TOUCH_SCL,  // 设置 SCL 引脚
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,       // 400kHz 快速模式
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    // 2. 初始化 CST816S 触摸芯片
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_NUM_0, &tp_io_config, &tp_io_handle));
    
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 240,
        .y_max = 240,
        .rst_gpio_num = GPIO_NUM_NC, // 如果没接复位引脚就填 NC
        .int_gpio_num = GPIO_NUM_NC, // 如果没接中断引脚就填 NC
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &g_touch_handle));

    // 3. 将触摸注册到 LVGL
    g_touch_indev = lv_indev_create();
    lv_indev_set_type(g_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(g_touch_indev, touchpad_read);

    ESP_LOGI(TAG, "触摸初始化成功！");
}