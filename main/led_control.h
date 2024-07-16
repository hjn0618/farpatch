// 新增的RMT LED控制相关头文件
#include <string.h>
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"

void set_led_color(uint8_t r, uint8_t g, uint8_t b);
void init_led_control(void);
