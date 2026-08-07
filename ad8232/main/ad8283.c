#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "ECG_MONITOR";

// --- 硬件配置 ---
#define ADC_UNIT          ADC_UNIT_1        
#define ADC_CHANNEL       ADC_CHANNEL_4     // GPIO1
#define ADC_ATTEN         ADC_ATTEN_DB_12   
#define ADC_WIDTH         ADC_BITWIDTH_12   

// --- 采样参数 ---
#define SAMPLE_DELAY_MS   4                 // 250Hz
#define WARM_UP_SECONDS   2                 

// 全局句柄
static adc_oneshot_unit_handle_t adc1_handle;

void app_main(void)
{
    ESP_LOGI(TAG, "AD8232 ECG Monitor Started");

    // 1. 初始化 ADC
    adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc1_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_WIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &chan_cfg));

    vTaskDelay(pdMS_TO_TICKS(WARM_UP_SECONDS * 1000));
    ESP_LOGI(TAG, "Start Sampling...");

    int raw_value = 0;
    
    // --- 核心算法变量 ---
    long long baseline_slow = 0; // 慢速基准（用于去基线漂移）
    long long baseline_fast = 0; // 快速基准（可选，用于平滑）
    int diff_value = 0;
    int sample_count = 0;

    while (1) {
        esp_err_t ret = adc_oneshot_read(adc1_handle, ADC_CHANNEL, &raw_value);
        
        if (ret == ESP_OK) {
            sample_count++;

            // 初始化
            if (sample_count == 1) {
                baseline_slow = raw_value;
            }



            00
            // 【关键修改】计算动态基准线
            // 使用 >> 4 (除以16) 而不是 >> 6 (除以64)
            // 这样基准线能更快地跟随呼吸造成的漂移，从而将其滤除
            baseline_slow = baseline_slow + ((long long)(raw_value - baseline_slow) >> 4);

            // 计算差值 (交流信号)
            diff_value = (int)(raw_value - baseline_slow);

            // 【可选】如果信号还是不稳，可以限制输出范围，防止画图炸裂
            // if (diff_value > 100) diff_value = 100;
            // if (diff_value < -100) diff_value = -100;

            // 打印纯数据
            printf("%d\n", diff_value);
            
        } else {
            ESP_LOGE(TAG, "ADC Read Failed");
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
    }
}