// XiaoGua RTC by zideai.com

#include "config.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "iot_button.h"
#include "audio.h"
#include "display.h"

#include <nvs.h>
#include <nvs_flash.h>

#include "system_info.h"
#include "network.h"

#include "chat.h"

static const char *TAG = "MAIN";
extern chat_config_t g_chat_config;

static void volume_up_click_cb(void *button_handle, void *usr_data);
static void volume_up_long_press_cb(void *button_handle, void *usr_data);
static void volume_down_click_cb(void *button_handle, void *usr_data);
static void volume_down_long_press_cb(void *button_handle, void *usr_data);

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

  button_config_t volume_up = {
      .type = BUTTON_TYPE_GPIO,
      .long_press_time = 1000,
      .short_press_time = 50,
      .gpio_button_config = {
          .gpio_num = VOLUME_UP_GPIO,
          .active_level = 0,
      },
  };

  button_config_t volume_down = {
      .type = BUTTON_TYPE_GPIO,
      .long_press_time = 1000,
      .short_press_time = 50,
      .gpio_button_config = {
          .gpio_num = VOLUME_DOWN_GPIO,
          .active_level = 0,
      },
  };

  button_handle_t btn_handle = iot_button_create(&button_reset);
  button_handle_t vol_up_handle = iot_button_create(&volume_up);
  button_handle_t vol_down_handle = iot_button_create(&volume_down);

  if (btn_handle == NULL)
    ESP_LOGE(TAG, "Failed to create button");

  iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_START, reset_button_long_press_cb, NULL);
  iot_button_register_cb(vol_up_handle, BUTTON_SINGLE_CLICK, volume_up_click_cb, NULL);
  iot_button_register_cb(vol_up_handle, BUTTON_LONG_PRESS_START, volume_up_long_press_cb, NULL);
  iot_button_register_cb(vol_down_handle, BUTTON_SINGLE_CLICK, volume_down_click_cb, NULL);
  iot_button_register_cb(vol_down_handle, BUTTON_LONG_PRESS_START, volume_down_long_press_cb, NULL);
}

static void volume_up_click_cb(void *button_handle, void *usr_data)
{
  uint8_t volume = get_output_volume();
  volume = (volume + 10 > 100) ? 100 : volume + 10;
  set_output_volume(volume);
  ESP_LOGI(TAG, "Volume up: %d", volume);
}

static void volume_up_long_press_cb(void *button_handle, void *usr_data)
{
  set_output_volume(100);
  ESP_LOGI(TAG, "Volume max");
}

static void volume_down_click_cb(void *button_handle, void *usr_data)
{
  uint8_t volume = get_output_volume();
  volume = (volume < 10) ? 0 : volume - 10;
  set_output_volume(volume);
  ESP_LOGI(TAG, "Volume down: %d", volume);
}

static void volume_down_long_press_cb(void *button_handle, void *usr_data)
{
  set_output_volume(0);
  ESP_LOGI(TAG, "Volume min");
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
  configure_display();

  /* Configure the control button */
  configure_control();

  /* Initialize network */
  bool connected = configure_network();

  /* Start chat task */
  if (connected)
    xTaskCreate(chat_task, "chat_task", 4096, NULL, PRIORITY_CHAT_TASK, NULL);

  while (1)
  {
    // ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
    // blink_led();
    // /* Toggle the LED state */
    // s_led_state = !s_led_state;
    // if (configUSE_TRACE_FACILITY == 1) LogRealTimeStats();
    // ESP_LOGI(TAG, "Chat state: %d", g_chat_config.chat_state);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
