#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#define PIN_NUM_MISO 16
#define PIN_NUM_MOSI 8
#define PIN_NUM_CLK 18
#define PIN_NUM_CS 9
#define PIN_NUM_DC 3
#define PIN_NUM_RST 46
#define PIN_NUM_BL 17

static const char *TAG = "ILI9341";
static spi_device_handle_t ili_handle;

static esp_err_t ili9341_send_command(uint8_t cmd, const void *data, int len)
{
    gpio_set_level(PIN_NUM_DC, 0);

    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 8,
        .tx_data = {cmd},
    };
    esp_err_t ret = spi_device_polling_transmit(ili_handle, &trans);
    if (ret != ESP_OK) {
        return ret;
    }

    if (data && len > 0) {
        gpio_set_level(PIN_NUM_DC, 1);
        spi_transaction_t data_trans = {
            .length = len * 8,
            .tx_buffer = data,
        };
        ret = spi_device_polling_transmit(ili_handle, &data_trans);
    }
    return ret;
}

static esp_err_t ili9341_reset(void)
{
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}

static esp_err_t ili9341_init(void)
{
    esp_err_t ret;

    ret = ili9341_send_command(0x01, NULL, 0); // SWRESET
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(150));

    ret = ili9341_send_command(0x11, NULL, 0); // SLPOUT
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(120));

    uint8_t data;

    data = 0x55; // 16-bit/pixel
    ret = ili9341_send_command(0x3A, &data, 1); // COLMOD
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));

    data = 0x48; // MX, BGR, MH. 调整显示方向和颜色顺序
    ret = ili9341_send_command(0x36, &data, 1); // MADCTL
    if (ret != ESP_OK) return ret;

    data = 0x01; // Normal display mode
    ret = ili9341_send_command(0x13, &data, 0); // NORON (no data)
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));

    ret = ili9341_send_command(0x29, NULL, 0); // DISPON
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}

static esp_err_t ili9341_fill_screen(uint16_t color)
{
    esp_err_t ret;
    uint8_t data[4];
    data[0] = 0x00;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 0xEF;
    ret = ili9341_send_command(0x2A, data, 4); // CASET
    if (ret != ESP_OK) return ret;

    data[0] = 0x00;
    data[1] = 0x00;
    data[2] = 0x01;
    data[3] = 0x3F;
    ret = ili9341_send_command(0x2B, data, 4); // RASET
    if (ret != ESP_OK) return ret;

    ret = ili9341_send_command(0x2C, NULL, 0); // RAMWR
    if (ret != ESP_OK) return ret;

    size_t pixels = 240 * 320;
    size_t buffer_size = 64;
    uint16_t buffer[buffer_size];
    for (size_t i = 0; i < buffer_size; i++) {
        buffer[i] = (color >> 8) | (color << 8);
    }

    gpio_set_level(PIN_NUM_DC, 1);
    spi_transaction_t trans = {
        .length = buffer_size * 16,
        .tx_buffer = buffer,
    };

    while (pixels > 0) {
        size_t chunk = pixels > buffer_size ? buffer_size : pixels;
        trans.length = chunk * 16;
        ret = spi_device_polling_transmit(ili_handle, &trans);
        if (ret != ESP_OK) return ret;
        pixels -= chunk;
    }
    return ESP_OK;
}

void app_main(void)
{
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &ili_handle));

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_RST) | (1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    ESP_LOGI(TAG, "Reset display");
    ESP_ERROR_CHECK(ili9341_reset());
    ESP_LOGI(TAG, "Init display");
    ESP_ERROR_CHECK(ili9341_init());

    gpio_set_level(PIN_NUM_BL, 1);

    while (1) {
        ESP_LOGI(TAG, "Fill red");
        ESP_ERROR_CHECK(ili9341_fill_screen(0xF800));
        vTaskDelay(pdMS_TO_TICKS(2000));

        ESP_LOGI(TAG, "Fill green");
        ESP_ERROR_CHECK(ili9341_fill_screen(0x07E0));
        vTaskDelay(pdMS_TO_TICKS(2000));

        ESP_LOGI(TAG, "Fill blue");
        ESP_ERROR_CHECK(ili9341_fill_screen(0x001F));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
