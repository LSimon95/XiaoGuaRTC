#include "audio.h"
#include "config.h"

#include <stdbool.h>

#include <esp_log.h>
#include <esp_afe_sr_models.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s_pdm.h>
#include <driver/i2s_std.h>
#include <stdint.h>
#include <string.h>

#include "chat.h"
#include "request.h"
#include "opus.h"
#include "peer_connection.h"

static const char *TAG = "AUDIO";

#define IN_SAMPLE_RATE 16000
#define OUT_SAMPLE_RATE 16000

#define PTIME_MS 40
#define WAKE_SPEECH_THRESHOLD 10

#define OPUS_BITRATE 20000
#define WAKE_AUDIO_BUFFER_SIZE 5 * OPUS_BITRATE / 8
#define OPUS_IN_FRAME_SIZE 640
#define OPUS_OUT_FRAME_SIZE 640
#define WAKE_WORK_CLAM_MS 500

#define OPUS_ENCODER_OUT_SIZE (OPUS_IN_FRAME_SIZE * OPUS_BITRATE * 2 / IN_SAMPLE_RATE)
#define OPUS_ENCODER_READ_SIZE (OPUS_IN_FRAME_SIZE * 2)

esp_afe_sr_data_t *g_afe_data = NULL;
QueueHandle_t g_audio_out_queue = NULL;
bool g_afe_enable = false;
extern chat_config_t g_chat_config;
extern PeerConnection *g_pc;

QueueHandle_t g_aec_ref_queue = NULL;

void wake_audio_detect()
{
  ESP_LOGI(TAG, "Wake audio detected");
}

void wake_word_detect(uint8_t *wake_audio_buffer, uint8_t *wake_audio_buffer_ptr)
{
  int start_time = esp_log_timestamp();
  cJSON *res = NULL;
  cJSON *data = cJSON_CreateObject();
  if (ws_send_cmd_and_wait_json(
          "wake", data, &res, 100, (const char *)wake_audio_buffer, (wake_audio_buffer_ptr - wake_audio_buffer)) == 0)
  {
    cJSON *status = cJSON_GetObjectItem(res, "status");
    if (status && status->valueint == 1)
    {
      ESP_LOGI(TAG, "Wake word detected time: %d ms", (int)(esp_log_timestamp() - start_time));
      if (g_chat_config.chat_state == CHAT_STATE_WAIT_REGIST)
      {

        wake_play_code();
      }
      else if (g_chat_config.chat_state == CHAT_STATE_IDLE)
        wake_peer_connect();
    }
  }

  if (res)
    cJSON_Delete(res);
  cJSON_Delete(data);
}

void wake_work_detect_loop(esp_afe_sr_iface_t *afe_handle, OpusEncoder *encoder)
{
  char *enc_in_buffer = (char *)malloc(1024 * 2);
  int enc_in_buffer_reamain = 0;

  int wake_audio_buffer_size = OPUS_BITRATE / 8 * 5;
  uint8_t *wake_audio_buffer = (uint8_t *)malloc(wake_audio_buffer_size);
  uint8_t *wake_audio_buffer_ptr = wake_audio_buffer;
  int speech_cnt = 0;
  int encode_frames = 0;
  g_afe_enable = true;

  while (g_chat_config.chat_state < CHAT_STATE_CONNECTING)
  {
    afe_fetch_result_t *fetch_result = afe_handle->fetch(g_afe_data);
    if ((fetch_result->vad_state == AFE_VAD_SPEECH) && (speech_cnt < WAKE_SPEECH_THRESHOLD))
      speech_cnt++;
    else if ((fetch_result->vad_state == AFE_VAD_SILENCE) && (speech_cnt > 0))
    {
      speech_cnt--;

      if (speech_cnt == 0)
      {
        g_afe_enable = false;
        if (encode_frames > 9600) // 64Kb/s 0.6s
          wake_word_detect(wake_audio_buffer, wake_audio_buffer_ptr);

        vTaskDelay(pdMS_TO_TICKS(WAKE_WORK_CLAM_MS));

        wake_audio_buffer_ptr = wake_audio_buffer;
        encode_frames = 0;
        afe_handle->reset_buffer(g_afe_data);
        g_afe_enable = true;
      }
    }

    if (speech_cnt > 0)
    {
      if ((wake_audio_buffer_ptr + OPUS_ENCODER_OUT_SIZE + 2) - wake_audio_buffer >= wake_audio_buffer_size)
        wake_audio_buffer_ptr = wake_audio_buffer;

      memcpy(enc_in_buffer + enc_in_buffer_reamain, fetch_result->data, fetch_result->data_size);
      enc_in_buffer_reamain += fetch_result->data_size;

      while (enc_in_buffer_reamain >= OPUS_ENCODER_READ_SIZE)
      {
        int enc_buffer_remain_size = wake_audio_buffer_size - (wake_audio_buffer_ptr - wake_audio_buffer) - 2;
        int len = opus_encode(encoder, (const opus_int16 *)enc_in_buffer, OPUS_IN_FRAME_SIZE, wake_audio_buffer_ptr + 2, enc_buffer_remain_size);
        encode_frames += OPUS_IN_FRAME_SIZE;
        if (len < 0)
        {
          ESP_LOGE(TAG, "Failed to encode audio");
          return;
        }
        *((uint16_t *)wake_audio_buffer_ptr) = len;

        enc_in_buffer_reamain -= OPUS_ENCODER_READ_SIZE;
        memcpy(enc_in_buffer, enc_in_buffer + OPUS_ENCODER_READ_SIZE, enc_in_buffer_reamain);
        wake_audio_buffer_ptr += len + 2;
        // ESP_LOGI(TAG, "encoded_bytes %d", len);
      }
    }
  }
  free(wake_audio_buffer);
  free(enc_in_buffer);
}

void rtc_io_loop(esp_afe_sr_iface_t *afe_handle, OpusEncoder *encoder)
{
  char enc_in_buffer[1024 * 2];
  int enc_in_buffer_reamain = 0;

  char enc_out_buffer[OPUS_ENCODER_OUT_SIZE];

  while (g_chat_config.chat_state >= CHAT_STATE_CONNECTING)
  {
    afe_fetch_result_t *fetch_result = afe_handle->fetch(g_afe_data);

    memcpy(enc_in_buffer + enc_in_buffer_reamain, fetch_result->data, fetch_result->data_size);
    enc_in_buffer_reamain += fetch_result->data_size;

    while (enc_in_buffer_reamain >= OPUS_ENCODER_READ_SIZE)
    {
      int len = opus_encode(encoder, (const opus_int16 *)enc_in_buffer, OPUS_IN_FRAME_SIZE, (unsigned char *)enc_out_buffer, OPUS_ENCODER_OUT_SIZE);
      if (len < 0)
      {
        ESP_LOGE(TAG, "Failed to encode audio");
        enc_in_buffer_reamain = 0;
        break;
      }

      enc_in_buffer_reamain -= OPUS_ENCODER_READ_SIZE;
      memcpy(enc_in_buffer, enc_in_buffer + OPUS_ENCODER_READ_SIZE, enc_in_buffer_reamain);

      // ESP_LOGI(TAG, "encoded_bytes %d", len);
      int res = chat_send_audio((const char *)enc_out_buffer, len);

      if (res < 0)
      {
        ESP_LOGE(TAG, "Failed to send audio");
        break;
      }
    }
  }
}

void audio_deliver_task(void *pvParameter)
{

  ESP_LOGI(TAG, "Audio deliver task started");

  // Init opus encoder
  OpusEncoder *encoder = opus_encoder_create(IN_SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, NULL);
  if (encoder == NULL)
  {
    ESP_LOGE(TAG, "Failed to create opus encoder");
    return;
  }

  opus_encoder_ctl(encoder, OPUS_SET_BITRATE(OPUS_BITRATE));
  opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(0));

  ESP_LOGI(TAG, "OPUS_IN_FRAME_SIZE %d enc_out_size %d enc_read_size %d", OPUS_IN_FRAME_SIZE, OPUS_ENCODER_OUT_SIZE, OPUS_ENCODER_READ_SIZE);

  esp_afe_sr_iface_t *afe_handle = &ESP_AFE_SR_HANDLE;

  while (1)
  {
    wake_work_detect_loop(afe_handle, encoder);
    rtc_io_loop(afe_handle, encoder);
  }
}

void audio_mic_task(void *pvParameter)
{
  i2s_chan_handle_t rx_handle = (i2s_chan_handle_t)pvParameter;

  //
  afe_config_t afe_config = {
      .aec_init = true,
      .se_init = false,
      .vad_init = true,
      .wakenet_init = false,
      .voice_communication_init = false,
      .voice_communication_agc_init = false,
      .voice_communication_agc_gain = 10,
      .vad_mode = VAD_MODE_3,
      .wakenet_model_name = NULL,
      .wakenet_model_name_2 = NULL,
      .wakenet_mode = DET_MODE_90,
      .afe_mode = SR_MODE_LOW_COST,
      .afe_perferred_core = 1,
      .afe_perferred_priority = PRIORITY_AFE_TASK,
      .afe_ringbuf_size = 50,
      .memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_INTERNAL,
      .afe_linear_gain = 1.0,
      .agc_mode = AFE_MN_PEAK_AGC_MODE_2,
      .pcm_config = {
          .total_ch_num = 2,
          .mic_num = 1,
          .ref_num = 1,
          .sample_rate = IN_SAMPLE_RATE,
      },
      .debug_init = false,
      .debug_hook = {{AFE_DEBUG_HOOK_MASE_TASK_IN, NULL}, {AFE_DEBUG_HOOK_FETCH_TASK_IN, NULL}},
      .afe_ns_mode = NS_MODE_SSP,
      .afe_ns_model_name = NULL,
      .fixed_first_channel = true,
  };

  esp_afe_sr_iface_t *afe_handle = &ESP_AFE_SR_HANDLE;
  g_afe_data = afe_handle->create_from_config(&afe_config);

  //
  int32_t buffer_read[OPUS_IN_FRAME_SIZE];
  int16_t buffer_pcm[OPUS_IN_FRAME_SIZE * 2 * 2]; // ｜mic｜ref｜
  int16_t *buffer_pcm_tail = buffer_pcm;

  i2s_channel_enable(rx_handle);

  int chunk_frame_size = afe_handle->get_feed_chunksize(g_afe_data);
  ESP_LOGI(TAG, "AFE feed chunk size %d", chunk_frame_size);

  while (1)
  {
    // Read from MIC
    size_t bytes_read;
    if (i2s_channel_read(rx_handle, buffer_read, OPUS_IN_FRAME_SIZE * sizeof(uint32_t), &bytes_read, portMAX_DELAY) != ESP_OK)
    {
      ESP_LOGE(TAG, "Failed to read from MIC");
      break;
    }

    int n_sample = bytes_read / sizeof(int32_t);
    if (n_sample != OPUS_IN_FRAME_SIZE)
    {
      ESP_LOGE(TAG, "Frame size mismatch %d", n_sample);
      continue;
    }

    // mic -> pcm
    for (int i = 0; i < n_sample; i++)
    {
      int32_t sample = (buffer_read[i] << 1) / 16384;
      if (sample > 32767)
        sample = 32767;
      if (sample < -32768)
        sample = -32768;
      buffer_pcm_tail[2 * i] = sample;
    }

    // ref -> pcm
    int16_t *pcm_ref = NULL;
    if (xQueueReceive(g_aec_ref_queue, &pcm_ref, 0) == pdTRUE)
    {
      for (int i = 0; i < n_sample; i++)
        buffer_pcm_tail[2 * i + 1] = pcm_ref[i];
      free(pcm_ref);
    }
    else
      for (int i = 0; i < n_sample; i++)
        buffer_pcm_tail[2 * i + 1] = 0;
    buffer_pcm_tail += 2 * n_sample;

    // feed to AFE
    int16_t *buffer_pcm_ptr_feed = buffer_pcm;
    while (buffer_pcm_tail - buffer_pcm_ptr_feed >= chunk_frame_size * 2)
    {
      afe_handle->feed(g_afe_data, buffer_pcm_ptr_feed);
      buffer_pcm_ptr_feed += chunk_frame_size * 2;
    }

    if (buffer_pcm_tail > buffer_pcm_ptr_feed)
    {
      int remain_size = buffer_pcm_tail - buffer_pcm_ptr_feed;
      memmove(buffer_pcm, buffer_pcm_ptr_feed, remain_size * sizeof(int16_t));
      buffer_pcm_tail = buffer_pcm + remain_size;
    }
    else
      buffer_pcm_tail = buffer_pcm;
  }
}

void audio_task(void *pvParameter)
{
  i2s_chan_handle_t tx_handle = NULL;
  i2s_chan_handle_t rx_handle = NULL;

  ESP_LOGI(TAG, "Audio task started");

  g_audio_out_queue = xQueueCreate(10, sizeof(opus_packet_t));
  if (g_audio_out_queue == NULL)
  {
    ESP_LOGE(TAG, "Failed to create audio out queue");
    return;
  }

  // Create a new channel for speaker
  i2s_chan_config_t chan_cfg = {
      .id = (i2s_port_t)0,
      .role = I2S_ROLE_MASTER,
      .dma_desc_num = 6,
      .dma_frame_num = 240,
      .auto_clear_after_cb = true,
      .auto_clear_before_cb = false,
      .intr_priority = 0,
  };
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

  i2s_std_config_t std_cfg = {
      .clk_cfg = {
          .sample_rate_hz = (uint32_t)OUT_SAMPLE_RATE,
          .clk_src = I2S_CLK_SRC_DEFAULT,
          .ext_clk_freq_hz = 0,
          .mclk_multiple = I2S_MCLK_MULTIPLE_256},
      .slot_cfg = {.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT, .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, .slot_mode = I2S_SLOT_MODE_MONO, .slot_mask = I2S_STD_SLOT_LEFT, .ws_width = I2S_DATA_BIT_WIDTH_32BIT, .ws_pol = false, .bit_shift = true, .left_align = true, .big_endian = false, .bit_order_lsb = false},
      .gpio_cfg = {.mclk = I2S_GPIO_UNUSED, .bclk = AUDIO_I2S_SPK_GPIO_BCLK, .ws = AUDIO_I2S_SPK_GPIO_LRCK, .dout = AUDIO_I2S_SPK_GPIO_DOUT, .din = I2S_GPIO_UNUSED, .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}}};
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));

  // Create a new channel for MIC
  chan_cfg.id = (i2s_port_t)1;
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));
  std_cfg.clk_cfg.sample_rate_hz = (uint32_t)IN_SAMPLE_RATE;
  std_cfg.gpio_cfg.bclk = AUDIO_I2S_MIC_GPIO_SCK;
  std_cfg.gpio_cfg.ws = AUDIO_I2S_MIC_GPIO_WS;
  std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
  std_cfg.gpio_cfg.din = AUDIO_I2S_MIC_GPIO_DIN;
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
  ESP_LOGI(TAG, "Simplex channels created");

  // Opus encoder
  OpusDecoder *decoder = opus_decoder_create(OUT_SAMPLE_RATE, 1, NULL);
  if (decoder == NULL)
  {
    ESP_LOGE(TAG, "Failed to create opus decoder");
    return;
  }

  g_aec_ref_queue = xQueueCreate(10, sizeof(int16_t *));

  i2s_channel_enable(tx_handle);

  xTaskCreate(audio_mic_task, "audio_recv_task", 4096 * 3, rx_handle, PRIORITY_MIC_TASK, NULL);
  while (!g_afe_data)
    vTaskDelay(pdMS_TO_TICKS(100));
  xTaskCreatePinnedToCore(audio_deliver_task, "audio_deliver_task", 7 * 4049, NULL, PRIORITY_AUDIO_DELIVER_TASK, NULL, 0);

  opus_packet_t opus_packet;
  int16_t pcm[OPUS_OUT_FRAME_SIZE];
  int32_t i32_samples_buffer[OPUS_OUT_FRAME_SIZE];
  size_t bytes_written;
  while (1)
  {

    if (xQueueReceive(g_audio_out_queue, &opus_packet, portMAX_DELAY) == pdTRUE)
    {
      int samples = opus_decode(decoder, (const unsigned char *)opus_packet.data, opus_packet.len, pcm, OPUS_OUT_FRAME_SIZE, 0);

      if (samples != OPUS_OUT_FRAME_SIZE)
        ESP_LOGE(TAG, "Frame size mismatch %d", samples);

      free(opus_packet.data);

      for (int i = 0; i < OPUS_OUT_FRAME_SIZE; i++)
        i32_samples_buffer[i] = (int32_t)(pcm[i]) * 327 * OUT_VOLUME;

      if (i2s_channel_write(tx_handle, i32_samples_buffer, OPUS_OUT_FRAME_SIZE * 4, &bytes_written, portMAX_DELAY))
      {
        ESP_LOGE(TAG, "Failed to write to speaker");
        break;
      }

      // aec ref
      int16_t *pcm_ref = malloc(OPUS_OUT_FRAME_SIZE * sizeof(int16_t));
      memcpy(pcm_ref, pcm, OPUS_OUT_FRAME_SIZE * sizeof(int16_t));
      int res = xQueueSend(g_aec_ref_queue, &pcm_ref, 0);
      if (res != pdTRUE)
        free(pcm_ref);
    }
  }

  opus_decoder_destroy(decoder);
  i2s_channel_disable(tx_handle);
  i2s_channel_disable(rx_handle);
  vQueueDelete(g_audio_out_queue);
  vQueueDelete(g_aec_ref_queue);
}