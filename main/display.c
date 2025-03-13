#include "display.h"
#include "config.h"
#include <math.h>

#include <esp_log.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <esp_lcd_io_i2c.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include "driver/gpio.h"
#include <led_strip.h>

#include "chat.h"

#define TAG "DISPLAY"

static led_strip_handle_t led_strip;
esp_lcd_panel_io_handle_t g_panel_io_handle = NULL;
esp_lcd_panel_handle_t g_panel = NULL;

extern bool g_chat_server_connected;

extern chat_config_t g_chat_config;

extern const uint8_t ebm_no_wifi[] asm("_binary_no_wifi_ebm_start");
extern const uint8_t ebm_regist[] asm("_binary_regist_ebm_start");
extern const uint8_t ebm_emo_neutral[] asm("_binary_emo_neutral_ebm_start");
extern const uint8_t ebm_emo_wink[] asm("_binary_emo_wink_ebm_start");
extern const uint8_t ebm_emo_happy1[] asm("_binary_emo_happy1_ebm_start");
extern const uint8_t ebm_emo_happy2[] asm("_binary_emo_happy2_ebm_start");
extern const uint8_t ebm_emo_sad1[] asm("_binary_emo_sad1_ebm_start");
extern const uint8_t ebm_emo_sad2[] asm("_binary_emo_sad2_ebm_start");
extern const uint8_t ebm_emo_angry1[] asm("_binary_emo_angry1_ebm_start");
extern const uint8_t ebm_emo_angry2[] asm("_binary_emo_angry2_ebm_start");

extern enum emo_state_t g_emo_state;

#define LEDC_DRAW_EBM(ebm, ledc_update)                                      \
  if (ledc_update)                                                           \
  {                                                                          \
    ledc_update = false;                                                     \
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(g_panel, 0, 0, 128, 32, ebm)); \
  }

#define WINK_CHANGE(ebm1, cnt1, ebm2, cnt2) \
  if (*next_wink_cnt == 0)                  \
  {                                         \
    *ledc_update = true;                     \
    if (wink)                               \
    {                                       \
      LEDC_DRAW_EBM(ebm1, ledc_update);     \
      *next_wink_cnt = cnt1;                \
      wink = false;                         \
    }                                       \
    else                                    \
    {                                       \
      LEDC_DRAW_EBM(ebm2, ledc_update);     \
      *next_wink_cnt = cnt2;                \
      wink = true;                          \
    }                                       \
  }

void emo_manager(int *next_wink_cnt, bool *ledc_update)
{
  static bool wink = false;
  switch (g_emo_state)
  {
  case EMO_NEUTRAL:
    WINK_CHANGE(ebm_emo_neutral, 40, ebm_emo_wink, 3);
    break;

  case EMO_HAPPY:
    WINK_CHANGE(ebm_emo_happy1, 3, ebm_emo_happy2, 3);
    break;

  case EMO_SAD:
    WINK_CHANGE(ebm_emo_sad1, 3, ebm_emo_sad2, 3);
    break;

  case EMO_ANGRY:
    WINK_CHANGE(ebm_emo_angry1, 3, ebm_emo_angry2, 3);
    break;

  default:
    break;
  }
}

void display_task(void *pvParameter)
{
  ESP_LOGI(TAG, "display_task started");
  int display_cnt = 0;

  bool last_g_chat_server_connected = g_chat_server_connected;
  enum chat_state_t last_chat_state = g_chat_config.chat_state;
  bool ledc_update = true;
  int next_wink_cnt = 0;

  while (true)
  {
    if (last_g_chat_server_connected != g_chat_server_connected || last_chat_state != g_chat_config.chat_state)
    {
      last_g_chat_server_connected = g_chat_server_connected;
      last_chat_state = g_chat_config.chat_state;
      ledc_update = true;
    }

    switch (g_chat_config.chat_state)
    {

    case CHAT_STATE_WAIT_REGIST:
      // LED Set red
      if (g_chat_server_connected)
      {
        led_strip_set_pixel(led_strip, 0, LED_INDICATOR_BRGHTNSS, 0, 0);
        led_strip_refresh(led_strip);

        LEDC_DRAW_EBM(ebm_regist, ledc_update);
      }
      else
      {
        if (display_cnt % 2 == 0)
          led_strip_set_pixel(led_strip, 0, LED_INDICATOR_BRGHTNSS, 0, 0);
        else
          led_strip_set_pixel(led_strip, 0, 0, 0, 0);
        led_strip_refresh(led_strip);

        LEDC_DRAW_EBM(ebm_no_wifi, ledc_update);
      }

      break;

    // Set yellow
    case CHAT_STATE_IDLE:
      led_strip_set_pixel(led_strip, 0, LED_INDICATOR_BRGHTNSS, LED_INDICATOR_BRGHTNSS, 0);
      led_strip_refresh(led_strip);
      emo_manager(&next_wink_cnt, &ledc_update);
      break;

    // Set flashing blue
    case CHAT_STATE_CONNECTING:
      if (display_cnt % 2 == 0)
        led_strip_set_pixel(led_strip, 0, 0, 0, LED_INDICATOR_BRGHTNSS);
      else
        led_strip_set_pixel(led_strip, 0, 0, 0, 0);
      led_strip_refresh(led_strip);
      emo_manager(&next_wink_cnt, &ledc_update);
      break;

    // Set breathe blink green
    case CHAT_STATE_CONNECTED_WAIT_WS_TURN:
      int brightness = (int)(LED_INDICATOR_BRGHTNSS * sin(M_PI * (display_cnt / 10.0)));
      if (brightness < 0)
        brightness = -brightness;
      led_strip_set_pixel(led_strip, 0, 0, brightness, 0);
      led_strip_refresh(led_strip);
      emo_manager(&next_wink_cnt, &ledc_update);
      break;

    // Set green
    case CHAT_STATE_CONNECTED:
      led_strip_set_pixel(led_strip, 0, 0, LED_INDICATOR_BRGHTNSS, 0);
      led_strip_refresh(led_strip);
      emo_manager(&next_wink_cnt, &ledc_update);
      break;

    default:
      break;
    }
    display_cnt++;
    if (next_wink_cnt > 0)
      next_wink_cnt--;

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

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

void configure_display()
{
  configure_led();

  i2c_master_bus_handle_t i2c_bus = NULL;
  i2c_master_bus_config_t bus_config = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .i2c_port = DISPLAY_I2C_BUS_PORT,
      .sda_io_num = EXAMPLE_PIN_NUM_SDA,
      .scl_io_num = EXAMPLE_PIN_NUM_SCL,
      .flags.enable_internal_pullup = true,
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

  esp_lcd_panel_io_i2c_config_t io_config = {
      .dev_addr = 0x3C,
      .on_color_trans_done = NULL,
      .user_ctx = NULL,
      .control_phase_bytes = 1,
      .dc_bit_offset = 6,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
      .flags = {
          .dc_low_on_data = 0,
          .disable_control_phase = 0,
      },
      .scl_speed_hz = 400 * 1000,
  };

  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(i2c_bus, &io_config, &g_panel_io_handle));

  ESP_LOGI(TAG, "I2C driver installed");
  esp_lcd_panel_dev_config_t panel_config = {};
  panel_config.reset_gpio_num = -1;
  panel_config.bits_per_pixel = 1;

  esp_lcd_panel_ssd1306_config_t ssd1306_config = {
      .height = 32,
  };
  panel_config.vendor_config = &ssd1306_config;

  ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(g_panel_io_handle, &panel_config, &g_panel));
  ESP_LOGI(TAG, "SSD1306 driver installed");

  // Reset the display
  ESP_ERROR_CHECK(esp_lcd_panel_reset(g_panel));
  if (esp_lcd_panel_init(g_panel) != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to initialize display");
    return;
  }
  // Turn on the display
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(g_panel, true));

  // Clear the display
  ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(g_panel, 0, 0, 128, 32, NULL));

  // Show the no wifi image
  ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(g_panel, 0, 0, 128, 32, ebm_no_wifi));

  xTaskCreate(display_task, "display_task", 4096, NULL, PRIORITY_EMO_TASK, NULL);
}