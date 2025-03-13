#pragma once

#include <soc/gpio_num.h>

#define RESET_BUTTON_GPIO        GPIO_NUM_0

#define BASE_URL "https://zideai.com"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define PRIORITY_AUDIO_TASK 5
#define PRIORITY_CHAT_TASK 1
#define PRIORITY_AFE_TASK 1
#define PRIORITY_PC_TASK 1
#define PRIORITY_WS_TASK 6
#define PRIORITY_MIC_TASK 5
#define PRIORITY_AUDIO_DELIVER_TASK 6
#define PRIORITY_EMO_TASK 1

#define LED_INDICATOR_BRGHTNSS 5

#define OUT_VOLUME 100

#define DISPLAY_I2C_BUS_PORT I2C_NUM_0
#define EXAMPLE_PIN_NUM_SDA GPIO_NUM_41
#define EXAMPLE_PIN_NUM_SCL GPIO_NUM_42