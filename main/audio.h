#pragma once
#include <stdint.h>
#include <driver/gpio.h>

#define AUDIO_I2S_MIC_GPIO_WS GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16
#define VOLUME_UP_GPIO   GPIO_NUM_40    // 音量增加按键GPIO
#define VOLUME_DOWN_GPIO GPIO_NUM_39    // 音量减小按键GPIO

typedef struct
{
    int len;
    const char *data;
} opus_packet_t;

void audio_task(void *pvParameter);

// 音量控制接口
void set_output_volume(uint8_t volume);  // 设置输出音量
uint8_t get_output_volume(void);         // 获取当前音量
void save_volume_settings(void);         // 保存音量设置到NVS
void load_volume_settings(void);         // 从NVS加载音量设置
