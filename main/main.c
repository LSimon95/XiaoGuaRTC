/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "config.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "iot_button.h"

#include <nvs.h>
#include <nvs_flash.h>

#include "system_info.h"
#include "network.h"

#include "chat.h"
#include <math.h>

static const char *TAG = "MAIN";

static led_strip_handle_t led_strip;
extern chat_config_t g_chat_config;
extern bool g_chat_server_connected;

void configure_led(void)
{
  /* LED strip initialization with the GPIO and pixels number*/
  led_strip_config_t strip_config = {
      .strip_gpio_num = 48,
      .max_leds = 1, // at least one LED on board
  };

  led_strip_rmt_config_t rmt_config = {
      .resolution_hz = 10 * 1000 * 1000, // 10MHz
      .flags.with_dma = false,
  };
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

  /* Set all LED off to clear all pixels */
  led_strip_clear(led_strip);
  led_strip_set_pixel(led_strip, 0, LED_INDICATOR_BRGHTNSS, 0, 0);
  led_strip_refresh(led_strip);
}

void reset_button_long_press_cb(void *button_handle, void *usr_data)
{
  ESP_LOGI(TAG, "Resetting the device");
  nvs_flash_erase();
  ESP_LOGI(TAG, "Restarting the ESP32 in 3 second");
  vTaskDelay(pdMS_TO_TICKS(3000));
  esp_restart();
}

void configure_control()
{
  button_config_t button_reset = {
      .type = BUTTON_TYPE_GPIO,
      .long_press_time = 3000,
      .short_press_time = 50,
      .gpio_button_config = {
          .gpio_num = RESET_BUTTON_GPIO,
          .active_level = 0,
      },
  };

  button_handle_t btn_handle = iot_button_create(&button_reset);
  if (btn_handle == NULL)
    ESP_LOGE(TAG, "Failed to create button");

  iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_START, reset_button_long_press_cb, NULL);
}

void app_main(void)
{
  /* Initialize the default event loop */
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  /* Initialize NVS flash for WiFi configuration */
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  /* Configure the peripheral according to the LED type */
  configure_led();

  /* Configure the control button */
  configure_control();

  /* Initialize network */
  bool connected = configure_network();

  /* Start chat task */
  if (connected)
    xTaskCreate(chat_task, "chat_task", 4096, NULL, PRIORITY_CHAT_TASK, NULL);

  int led_flash_cnt = 0;

  while (1)
  {
    // ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
    // blink_led();
    // /* Toggle the LED state */
    // s_led_state = !s_led_state;
    // if (configUSE_TRACE_FACILITY == 1) LogRealTimeStats();
    // ESP_LOGI(TAG, "Chat state: %d", g_chat_config.chat_state);

    vTaskDelay(pdMS_TO_TICKS(100));
    // LED indication
    switch (g_chat_config.chat_state)
    {
    // Set red
    case CHAT_STATE_WAIT_REGIST:
      if (g_chat_server_connected)
      {
        led_strip_set_pixel(led_strip, 0, LED_INDICATOR_BRGHTNSS, 0, 0);
        led_strip_refresh(led_strip);
      }
      else
      {
        if (led_flash_cnt % 2 == 0)
          led_strip_set_pixel(led_strip, 0, LED_INDICATOR_BRGHTNSS, 0, 0);
        else
          led_strip_set_pixel(led_strip, 0, 0, 0, 0);
        led_strip_refresh(led_strip);
      }

      break;

    // Set yellow
    case CHAT_STATE_IDLE:
      led_strip_set_pixel(led_strip, 0, LED_INDICATOR_BRGHTNSS, LED_INDICATOR_BRGHTNSS, 0);
      led_strip_refresh(led_strip);
      break;

    // Set flashing blue
    case CHAT_STATE_CONNECTING:
      if (led_flash_cnt % 2 == 0)
        led_strip_set_pixel(led_strip, 0, 0, 0, LED_INDICATOR_BRGHTNSS);
      else
        led_strip_set_pixel(led_strip, 0, 0, 0, 0);
      led_strip_refresh(led_strip);
      break;

    // Set breathe blink green
    case CHAT_STATE_CONNECTED_WAIT_WS_TURN:
      int brightness = (int)(LED_INDICATOR_BRGHTNSS * sin(M_PI * (led_flash_cnt / 10.0)));
      if (brightness < 0)
        brightness = -brightness;
      led_strip_set_pixel(led_strip, 0, 0, brightness, 0);
      led_strip_refresh(led_strip);
      break;

    // Set green
    case CHAT_STATE_CONNECTED:
      led_strip_set_pixel(led_strip, 0, 0, LED_INDICATOR_BRGHTNSS, 0);
      led_strip_refresh(led_strip);
      break;

    default:
      break;
    }
    led_flash_cnt++;
  }
}
