#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include "esp_event.h"

// 声明一个事件组句柄，让其他文件也能访问
extern EventGroupHandle_t time_event_group;
// 声明一个比特位，代表“时间同步成功”
extern const int TIME_SYNCED_BIT;

// 声明初始化函数
void initialize_sntp(void);
#endif