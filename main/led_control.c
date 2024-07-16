#include "led_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz 分辨率
#define RMT_LED_STRIP_GPIO_NUM      38
#define EXAMPLE_LED_NUMBERS         1

static uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];
static rmt_channel_handle_t led_chan = NULL;
static rmt_encoder_handle_t led_encoder = NULL;

static uint8_t cache_red = 0;
static uint8_t cache_green = 0;
static uint8_t cache_blue = 0;
static bool is_led_init = false;
static QueueHandle_t led_color_queue;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} led_color_t;

static const char *TAG = "LED_Control";

// LED 控制任务
void led_control_task(void *pvParameters)
{
    led_color_t color;

    while (1) {
        if (xQueueReceive(led_color_queue, &color, portMAX_DELAY)) {
            ESP_LOGI(TAG, "接收到颜色: R=%d, G=%d, B=%d", color.red, color.green, color.blue);

            cache_red = color.red;
            cache_green = color.green;
            cache_blue = color.blue;

            for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {
                led_strip_pixels[i * 3 + 0] = color.green;
                led_strip_pixels[i * 3 + 1] = color.red;
                led_strip_pixels[i * 3 + 2] = color.blue;
            }

            rmt_transmit_config_t tx_config = {
                .loop_count = 0,
            };

            rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config);
            rmt_tx_wait_all_done(led_chan, portMAX_DELAY);

            ESP_LOGI(TAG, "LED 灯条颜色更新完成");
        }
    }
}

// 设置LED颜色
void set_led_color(uint8_t red, uint8_t green, uint8_t blue)
{
    if (red == cache_red && green == cache_green && blue == cache_blue) {
        return;
    }

    led_color_t color = { .red = red, .green = green, .blue = blue };
    xQueueSend(led_color_queue, &color, portMAX_DELAY);
}

// 初始化LED控制
void init_led_control(void)
{
    if (is_led_init) {
        return;
    }

    is_led_init = true;

    led_color_queue = xQueueCreate(10, sizeof(led_color_t));
    if (led_color_queue == NULL) {
        ESP_LOGE(TAG, "创建队列失败");
        return;
    }

    ESP_LOGI(TAG, "创建 RMT TX 频道");
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    ESP_LOGI(TAG, "安装 LED 灯条编码器");
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_LOGI(TAG, "启用 RMT TX 频道");
    ESP_ERROR_CHECK(rmt_enable(led_chan));

    xTaskCreate(led_control_task, "led_control_task", 2048, NULL, 10, NULL);
}
