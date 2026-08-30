#include "lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"  // <--- 加上这一行
// #include "esp_lcd_touch_cst816s.h" // 引入触摸驱动
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// --- 硬件引脚配置 (只在这个文件里修改) ---
#define PIN_NUM_RST     9
#define PIN_NUM_CS      46
#define PIN_NUM_DC      3
#define PIN_NUM_MOSI    8
#define PIN_NUM_CLK     18
#define PIN_NUM_BK_LIGHT 5 // 背光引脚

// static lv_indev_t *g_touch_indev = NULL; // 全局触摸输入设备句柄
static const char *TAG = "lcd_gc9a01";
// 全局 panel 句柄（需要在 lcd_gc9a01_init 中赋值）
static esp_lcd_panel_handle_t g_panel_handle = NULL;

// // 1. 触摸数据读取回调（LVGL 会定期调用这个函数获取坐标）
// static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
//     uint16_t touch_x, touch_y;
//     uint8_t touch_cnt = 0;
    
//     // 从底层驱动读取触摸状态
//     esp_lcd_touch_read_data(g_touch_handle); 
    
//     // 获取第一个触摸点的坐标
//     bool touched = esp_lcd_touch_get_coordinates(g_touch_handle, &touch_x, &touch_y, NULL, &touch_cnt, 1);
    
//     if (touched && touch_cnt > 0) {
//         data->point.x = touch_x;
//         data->point.y = touch_y;
//         data->state = LV_INDEV_STATE_PRESSED;
//     } else {
//         data->state = LV_INDEV_STATE_RELEASED;
//     }
// }

void lcd_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    if (g_panel_handle == NULL) return;

    // 1. 使用 Panel 句柄绘制位图（内部自动处理 DMA 和 SPI 传输）
    esp_lcd_panel_draw_bitmap(g_panel_handle,
        area->x1, area->y1, area->x2 + 1, area->y2 + 1,
        px_map);

    // 2. 【关键】通知 LVGL 本帧刷新完成，允许开始下一帧
    lv_display_flush_ready(disp);
}

esp_lcd_panel_handle_t lcd_gc9a01_init(void) {
    esp_lcd_panel_handle_t panel_handle = NULL;

    // 1. 初始化背光 (可选)
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(PIN_NUM_BK_LIGHT, 1); // 点亮背光

    // 2. 配置 SPI 总线
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_CLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1, // GC9A01 通常不需要 MISO
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32 * 1024,  // 32KB，ESP32-S3 SPI DMA 安全上限
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 3. 配置 LCD IO 句柄
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        // 降低时钟以降低信号完整性问题（先测试 20MHz）
        .pclk_hz = 10 * 1000 * 1000, // 20MHz 时钟
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    // 4. 安装 GC9A01 驱动
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        // 很多 GC9A01 模块实际是 BGR 排列，颜色异常时先切换为 BGR
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, // 如果颜色异常，改为 LCD_RGB_ELEMENT_ORDER_BGR
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));

        // 5. 执行标准初始化流程
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    // 【关键修复】尝试修正 GC9A01 的扫描方向和坐标系
    // GC9A01 的默认设置往往和 LVGL 的默认坐标系（左上角为原点）不一致
    
    // // 方案 A：最常见的圆形屏配置（交换 XY 轴 + X 轴镜像）
    // esp_lcd_panel_swap_xy(panel_handle, true);  
    // esp_lcd_panel_mirror(panel_handle, true, false); 

    // // 如果方案 A 不行，请注释掉上面两行，尝试方案 B：
    // esp_lcd_panel_swap_xy(panel_handle, false); 
    // esp_lcd_panel_mirror(panel_handle, false, true);

    // 如果方案 B 也不行，尝试方案 C（完全不镜像，只交换）：
    esp_lcd_panel_swap_xy(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, false, false); 
    
    // 颜色与反色配置（先保持你之前的 true/true 组合）
    esp_lcd_panel_invert_color(panel_handle, true);
    
    // 【核心修复 1】：手动打开屏幕显示（不加这句，屏幕永远是黑的！）
    esp_lcd_panel_disp_on_off(panel_handle, true); 
    // 改后（代码实际用的是 RGB）：
    ESP_LOGI(TAG, "lcd: pclk=%d, rgb_order=RGB, invert_color=1", io_config.pclk_hz);
    
    // 【核心修复 2】：保存全局句柄，供 lcd_flush 使用
    g_panel_handle = panel_handle;  
    
    // 【核心修复 3】：把 return 放到函数的最后一行！
    return panel_handle;

    // // 【新增】初始化 CST816S 触摸
    // esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    // esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    // ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)1, &tp_io_config, &tp_io_handle));
    
    // esp_lcd_touch_handle_t tp_handle = NULL;
    // esp_lcd_touch_config_t tp_cfg = {
    //     .x_max = 240,
    //     .y_max = 240,
    //     .rst_gpio_num = GPIO_NUM_NC, // 根据你的硬件修改
    //     .int_gpio_num = GPIO_NUM_NC, // 根据你的硬件修改
    // };
    // ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp_handle));
    // g_touch_handle = tp_handle; // 保存句柄

    // // 【新增】将触摸注册到 LVGL
    // g_touch_indev = lv_indev_create();
    // lv_indev_set_type(g_touch_indev, LV_INDEV_TYPE_POINTER);
    // lv_indev_set_read_cb(g_touch_indev, touchpad_read);
}
