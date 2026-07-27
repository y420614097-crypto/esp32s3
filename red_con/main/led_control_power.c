#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "driver/ledc.h"
#include "esp_task_wdt.h" // 别忘了在文件顶部加上这个头文件

//定义引脚
#define LED_PIN 42
//选择定时器
#define LEDC_TIMER LEDC_TIMER_0
//选择模式，低速模式省电，切人眼观察不出来
#define LEDC_MODE LEDC_LOW_SPEED_MODE
//直观显示输出引脚
#define LED_OPTPUT_PIN LED_PIN
//ESP32 内部有多个通道（Channel 0 到 Channel 7），这里选择通道 0
#define LEDC_CHANNLE LEDC_CHANNEL_0
//设置占空比分辨率
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT
//设置频率为 5000Hz
#define LEDC_FREQUENCY  5000


void app_main(void)
{
    // esp_task_wdt_add(NULL);  // NULL 代表当前任务
    //1.配置定时器
    ledc_timer_config_t ledc_timer={
        .speed_mode=LEDC_MODE,
        .timer_num=LEDC_TIMER,
        .duty_resolution=LEDC_DUTY_RES,
        .freq_hz=LEDC_FREQUENCY,    
        .clk_cfg=LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    //2.配置通道
    ledc_channel_config_t ledc_channel={
        .speed_mode=LEDC_MODE,
        .channel=LEDC_CHANNLE,
        .timer_sel=LEDC_TIMER,
        .intr_type=LEDC_INTR_DISABLE,
        .gpio_num=LED_OPTPUT_PIN,
        .duty=0,
        .hpoint=0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    //实现占空比从 0 到 8191 的渐变
    while(1){
        // esp_task_wdt_reset(); // 手动喂狗，重置看门狗计时器
        //变量过程
        for(int duty=0;duty<8191;duty+=100){
            ledc_set_duty(LEDC_MODE,LEDC_CHANNLE,duty); 
            ledc_update_duty(LEDC_MODE,LEDC_CHANNLE);
            vTaskDelay(30 / portTICK_PERIOD_MS);
        }
        //变暗过程
        for(int duty=8191;duty>0;duty-=100){
            ledc_set_duty(LEDC_MODE,LEDC_CHANNLE,duty); 
            ledc_update_duty(LEDC_MODE,LEDC_CHANNLE);
            vTaskDelay(30 / portTICK_PERIOD_MS);
        }
    }
}