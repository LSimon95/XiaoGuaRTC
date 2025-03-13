#include "chat.h"

#include "config.h"
#include "audio.h"

#include "request.h"
#include <esp_mac.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include "opus.h"
#include "peer.h"
#include "peer_connection.h"
#include <esp_websocket_client.h>

static const char *TAG = "CHAT";
PeerConnection *g_pc;
PeerConnectionState eState = PEER_CONNECTION_CLOSED;
int gDataChannelOpened = 0;
chat_config_t g_chat_config;

extern QueueHandle_t g_audio_out_queue;
QueueHandle_t g_rtc_datachannel_queue = NULL;

esp_websocket_client_handle_t g_ws_client = NULL;
SemaphoreHandle_t g_ws_client_lock = NULL;
cJSON *g_ws_res = NULL;
char *g_local_sdp_wait_send = NULL;
char *g_remote_sdp_wait_recv = NULL;

bool g_chat_server_connected = false;

bool g_ws_playing = false;

enum emo_state_t g_emo_state = EMO_NEUTRAL;

static int wait_ws_res(int timeout)
{
  if (g_ws_res)
  {
    cJSON_Delete(g_ws_res);
    g_ws_res = NULL;
  }

  int cnt = 0;
  while (g_ws_res == NULL)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
    if (cnt++ > timeout)
    {
      ESP_LOGE(TAG, "Failed to get ws response");
      return -1;
    }
  }
  return 0;
}

void ws_recv(void *event_handler_arg,
             esp_event_base_t event_base,
             int32_t event_id,
             void *event_data)
{
  esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

  // ESP_LOGI(TAG, "ws received data: opcode %d len %d", data->op_code, data->data_len);

  if (data->op_code == WS_TRANSPORT_OPCODES_TEXT)
  {
    cJSON *ws_res = cJSON_Parse(data->data_ptr);
    if (!ws_res)
    {
      ESP_LOGE(TAG, "Failed to parse ws data");
      return;
    }

    const char *cmd_str = cJSON_Print(ws_res);
    ESP_LOGI(TAG, "ws received data: %s", cmd_str);
    free((void *)cmd_str);

    if (!g_ws_playing && cJSON_GetObjectItem(ws_res, "audio_start"))
    {
      g_ws_playing = true;
      cJSON_Delete(ws_res);
      return;
    }
    else if (g_ws_playing && cJSON_GetObjectItem(ws_res, "audio_end"))
    {
      g_ws_playing = false;
      cJSON_Delete(ws_res);

      if (g_chat_config.chat_state == CHAT_STATE_CONNECTED_WAIT_WS_TURN)
        g_chat_config.chat_state = CHAT_STATE_CONNECTED;
      return;
    }
    else
    {
      cJSON *cmd = cJSON_GetObjectItem(ws_res, "cmd");
      if (cmd)
      {
        if (strcmp(cmd->valuestring, "server_sdp") == 0)
        {
          const char *sdp = cJSON_GetObjectItem(ws_res, "sdp")->valuestring;
          ESP_LOGI(TAG, "Received remote sdp%s", sdp);
          g_remote_sdp_wait_recv = malloc(strlen(sdp) + 1);
          strcpy(g_remote_sdp_wait_recv, sdp);

          cJSON_Delete(ws_res);
          return;
        }
        else if (strcmp(cmd->valuestring, "emotion") == 0)
        {
          cJSON *emotion = cJSON_GetObjectItem(ws_res, "emotion");
          if (emotion)
          {
            if (strcmp(emotion->valuestring, "happy") == 0)
              g_emo_state = EMO_HAPPY;       
            else if (strcmp(emotion->valuestring, "sad") == 0)
              g_emo_state = EMO_SAD;
            else if (strcmp(emotion->valuestring, "angry") == 0)
              g_emo_state = EMO_ANGRY;
            else
              g_emo_state = EMO_NEUTRAL;
          }
          cJSON_Delete(ws_res);
          return;
        }
      }
    }

    if (g_ws_res)
    {
      cJSON_Delete(ws_res);
      const char *g_ws_res_str = cJSON_Print(g_ws_res);
      ESP_LOGW(TAG, "g_ws_res is not NULL %s", g_ws_res_str);
      free((void *)g_ws_res_str);
      return;
    }

    g_ws_res = ws_res;
  }
  else if (data->op_code == WS_TRANSPORT_OPCODES_BINARY)
  {
    static char *stream_buf = NULL;
    static int stream_buf_len = 0;

    if (data->data_len < 2)
    {
      ESP_LOGE(TAG, "Invalid data len %d", data->data_len);
      return;
    }

    if (!stream_buf && *(uint16_t *)data->data_ptr > data->data_len - 2)
    {
      stream_buf = malloc(*(uint16_t *)data->data_ptr + 2);
      memcpy(stream_buf, data->data_ptr, data->data_len);
      stream_buf_len = data->data_len;
      return;
    }

    char *opus_data = NULL;
    int data_len = 0;
    if (stream_buf)
    {
      ESP_LOGI(TAG, "stream_buf_len %d, data_len %d, total_len %d", stream_buf_len, data->data_len, *(uint16_t *)stream_buf);
      memcpy(stream_buf + stream_buf_len, data->data_ptr, data->data_len);
      if (stream_buf_len + data->data_len - 2 < *(uint16_t *)stream_buf)
      {
        stream_buf_len += data->data_len;
        return;
      }
      else if (stream_buf_len + data->data_len - 2 > *(uint16_t *)stream_buf)
      {
        ESP_LOGE(TAG, "stream_buf_len %d, data_len %d, total_len %d", stream_buf_len, data->data_len, *(uint16_t *)stream_buf);
        free(stream_buf);
        stream_buf = NULL;
        stream_buf_len = 0;
        return;
      }
      data_len = *(uint16_t *)stream_buf - 2;
      opus_data = malloc(data_len);
      memcpy(opus_data, stream_buf + 2, data_len);
      free(stream_buf);
      stream_buf = NULL;
      stream_buf_len = 0;
    }
    else
    {
      opus_data = malloc(data->data_len - 2);
      memcpy(opus_data, data->data_ptr + 2, data->data_len - 2);
      data_len = data->data_len - 2;
    }

    opus_packet_t opus_packet = {
        .data = opus_data,
        .len = data_len,
    };
    xQueueSend(g_audio_out_queue, &opus_packet, portMAX_DELAY);
  }
}

static int ws_send_json_cmd(const char *cmd, cJSON *data)
{
  if (g_ws_client == NULL)
  {
    ESP_LOGE(TAG, "ws client is not connected");
    return -1;
  }

  cJSON_AddStringToObject(data, "cmd", cmd);

  char *data_str = cJSON_PrintUnformatted(data);
  int data_sent = esp_websocket_client_send_text(g_ws_client, data_str, strlen(data_str), portMAX_DELAY);
  free(data_str);

  ESP_LOGI(TAG, "ws send cmd: %s", cmd);

  return data_sent;
}

int ws_send_cmd_and_wait_json(const char *cmd, cJSON *data, cJSON **res, int timeout, const char *bin_data, int bin_len)
{
  xSemaphoreTake(g_ws_client_lock, portMAX_DELAY);

  if (g_ws_res)
  {
    cJSON_Delete(g_ws_res);
    g_ws_res = NULL;
  }

  if (ws_send_json_cmd(cmd, data) < 0)
  {
    xSemaphoreGive(g_ws_client_lock);
    return -1;
  }

  if (bin_data && bin_len > 0)
    esp_websocket_client_send_bin(g_ws_client, bin_data, bin_len, portMAX_DELAY);

  if (wait_ws_res(timeout) == 0)
  {
    if (res)
      *res = cJSON_Duplicate(g_ws_res, 1);

    cJSON_Delete(g_ws_res);
    g_ws_res = NULL;

    xSemaphoreGive(g_ws_client_lock);
    return 0;
  }

  xSemaphoreGive(g_ws_client_lock);
  return -1;
}

int ws_send_cmd_and_wait_play_done(const char *cmd, cJSON *data, int timeout)
{
  xSemaphoreTake(g_ws_client_lock, portMAX_DELAY);

  if (ws_send_json_cmd(cmd, data) < 0)
  {
    xSemaphoreGive(g_ws_client_lock);
    return -1;
  }

  g_ws_playing = false;

  uint32_t cnt = 0;
  while (!g_ws_playing && ++cnt < timeout)
    vTaskDelay(pdMS_TO_TICKS(10));

  while (g_ws_playing && ++cnt < timeout)
    vTaskDelay(pdMS_TO_TICKS(10));

  if (cnt++ >= timeout)
  {
    ESP_LOGE(TAG, "Timeout to wait play done");
    xSemaphoreGive(g_ws_client_lock);
    g_ws_playing = false;
    return -1;
  }

  xSemaphoreGive(g_ws_client_lock);
  g_ws_playing = false;
  return 0;
}

cJSON *request_code()
{
  cJSON *cmd = cJSON_CreateObject();
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char mac_str[18];
  sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(mac));
  cJSON_AddStringToObject(cmd, "mac", mac_str);

  cJSON *res = NULL;
  if (ws_send_cmd_and_wait_json("request_code", cmd, &res, 100, NULL, 0) != 0)
  {
    ESP_LOGE(TAG, "request_code failed");
    goto fail;
  }
  cJSON_Delete(cmd);

  cJSON *code = cJSON_GetObjectItem(res, "code");
  if (code == NULL)
  {
    ESP_LOGE(TAG, "Failed to get code");
    goto fail;
  }
  return res;

fail:
  if (res)
    cJSON_Delete(res);
  return NULL;
}

void wake_play_code()
{
  cJSON *cmd = cJSON_CreateObject();
  ws_send_cmd_and_wait_play_done("request_audio_code", cmd, 2000);
  cJSON_Delete(cmd);
}

void regist_board(void)
{
  nvs_handle_t nvs_handle;
  bool start_audio_task = true;

  g_chat_config.token = NULL;
  g_chat_config.chat_state = CHAT_STATE_WAIT_REGIST;

  while (1)
  {

    ESP_LOGI(TAG, "Registering the board");
    cJSON *regist_code_res = request_code();
    if (regist_code_res)
    {
      cJSON *code = cJSON_GetObjectItem(regist_code_res, "code");
      if (code)
      {
        ESP_LOGI(TAG, "Received code: %s", code->valuestring);

        if (start_audio_task)
        {
          xTaskCreate(audio_task, "audio_task", 4096 * 5, NULL, PRIORITY_AUDIO_TASK, NULL);
          start_audio_task = false;
        }
      }

      cJSON *token = cJSON_GetObjectItem(regist_code_res, "token");
      if (token)
      {
        ESP_LOGI(TAG, "Received token: %s", token->valuestring);
        nvs_open("token", NVS_READWRITE, &nvs_handle);
        nvs_set_str(nvs_handle, "token", token->valuestring);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);

        ESP_LOGI(TAG, "Token saved to NVS and esp will restart in 3 seconds");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
      }

      cJSON_Delete(regist_code_res);
    }

    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

static void on_ice_connection_state_change(PeerConnectionState state, void *user_data)
{
  ESP_LOGI(TAG, "PeerConnectionState: %d", state);
  eState = state;

  // not support datachannel close event
  if (eState != PEER_CONNECTION_COMPLETED)
  {
    gDataChannelOpened = 0;
  }

  if (eState == PEER_CONNECTION_COMPLETED)
    g_chat_config.chat_state = CHAT_STATE_CONNECTED_WAIT_WS_TURN;

  if (eState == PEER_CONNECTION_FAILED)
    g_chat_config.chat_state = CHAT_STATE_IDLE;
}

static void on_message(char *msg, size_t len, void *userdata, uint16_t sid)
{

  ESP_LOGI(TAG, "Datachannel message: %.*s", len, msg);
}

static void on_open(void *userdata)
{

  ESP_LOGI(TAG, "Datachannel opened");
  gDataChannelOpened = 1;
}

static void on_close(void *userdata)
{
}

static void on_ice_candidate(char *sdp_text, void *userdata)
{
  ESP_LOGI(TAG, "on_ice_candidate: %s", sdp_text);

  g_local_sdp_wait_send = sdp_text;

  while (true)
  {
    if (g_remote_sdp_wait_recv)
    {
      peer_connection_set_remote_description(g_pc, g_remote_sdp_wait_recv);
      free(g_remote_sdp_wait_recv);
      g_remote_sdp_wait_recv = NULL;
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  ESP_LOGI(TAG, "Get remote sdp success");
}

static void peer_connection_loop_task(void *pvParameters)
{
  while (1)
  {
    if (g_pc)
    {
      peer_connection_loop(g_pc);
      PeerConnectionState state = peer_connection_get_state(g_pc);
      if (state == PEER_CONNECTION_CLOSED || state == PEER_CONNECTION_FAILED)
      {
        ESP_LOGE(TAG, "Peer connection closed or failed");
        g_chat_config.chat_state = CHAT_STATE_IDLE;
        vTaskDelete(NULL);
      }
    }
  }
}

int chat_send_audio(const char *data, size_t len)
{
  if (g_local_sdp_wait_send != NULL)
  {
    cJSON *cmd = cJSON_CreateObject();
    cJSON_AddStringToObject(cmd, "sdp", g_local_sdp_wait_send);
    cJSON_AddStringToObject(cmd, "cmd", "board_sdp");

    char *ws_res_str = cJSON_PrintUnformatted(cmd);
    int res_send_sdp = esp_websocket_client_send_text(g_ws_client, ws_res_str, strlen(ws_res_str), portMAX_DELAY);
    free(ws_res_str);
    cJSON_Delete(cmd);

    if (res_send_sdp < 0)
      ESP_LOGE(TAG, "Failed to send sdp");
    g_local_sdp_wait_send = NULL;
  }

  int res = -1;
  if (!g_ws_playing)
  {
    if (g_chat_config.chat_state == CHAT_STATE_CONNECTED)
      res = peer_connection_send_audio(g_pc, (const uint8_t *)data, len);
    else
      res = esp_websocket_client_send_bin(g_ws_client, data, len, portMAX_DELAY);
    if (res <= 0)
      ESP_LOGE(TAG, "Failed to send audio");

    return res;
  }

  return 0;
}

void ws_talk_task(void *pvParameters)
{
  cJSON *cmd = cJSON_CreateObject();
  ws_send_cmd_and_wait_play_done("chat", cmd, portMAX_DELAY);
  cJSON_Delete(cmd);

  g_ws_playing = false;
  vTaskDelete(NULL);

  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void on_audio_track(uint8_t *data, size_t size, void *userdata)
{
  char *opus_data = malloc(size);
  memcpy(opus_data, data, size);

  opus_packet_t opus_packet = {
      .data = opus_data,
      .len = size,
  };

  xQueueSend(g_audio_out_queue, &opus_packet, portMAX_DELAY);
}

void wake_peer_connect()
{
  g_chat_config.chat_state = CHAT_STATE_CONNECTING;
  xTaskCreate(ws_talk_task, "ws_talk_task", 4096, NULL, PRIORITY_WS_TASK, NULL);

  if (g_pc)
    peer_connection_destroy(g_pc);

  if (g_local_sdp_wait_send)
  {
    g_local_sdp_wait_send = NULL;
  }

  if (g_remote_sdp_wait_recv)
  {
    free(g_remote_sdp_wait_recv);
    g_remote_sdp_wait_recv = NULL;
  }

  // if (g_rtc_datachannel_queue)
  //   vQueueDelete(g_rtc_datachannel_queue);

  // g_rtc_datachannel_queue = xQueueCreate(10, sizeof(char *) * 32);

  PeerConfiguration config = {
      .ice_servers = {
          {.urls = g_chat_config.turn_server_url,
           .username = g_chat_config.turn_server_username,
           .credential = g_chat_config.turn_server_credential}},
      .audio_codec = CODEC_OPUS,
      .datachannel = DATA_CHANNEL_NONE,
      .onaudiotrack = on_audio_track,
  };

  ESP_LOGI(TAG, "Creating peer connection");
  ESP_LOGI(TAG, "Free memory: %d", (int)esp_get_free_heap_size());

  g_pc = peer_connection_create(&config);
  peer_connection_oniceconnectionstatechange(g_pc, on_ice_connection_state_change);
  peer_connection_ondatachannel(g_pc, on_message, on_open, on_close);
  peer_connection_onicecandidate(g_pc, on_ice_candidate);

  peer_connection_create_offer(g_pc);
  xTaskCreatePinnedToCore(peer_connection_loop_task, "peer_connection_loop_task", 4096 * 2, NULL, PRIORITY_PC_TASK, NULL, 1);

  // while (peer_connection_get_state(g_pc) <= PEER_CONNECTION_CONNECTED)
  //   vTaskDelay(pdMS_TO_TICKS(10));

  ESP_LOGI(TAG, "Create peer connection success");
}

void connect_chat_server(const char *token)
{
  cJSON *res = NULL;
  char url[256];
  cJSON *header = cJSON_CreateObject();
  cJSON_AddStringToObject(header, "Host", "zideai.com");
  sprintf(url, "%s/webapi/nikaboard/chat_server/", BASE_URL);
  int status_code = request(HTTP_METHOD_GET, url, header, NULL, &res);
  cJSON_Delete(header);

  if (status_code != 200)
  {
    ESP_LOGE(TAG, "Failed to get chat server, status code: %d", status_code);
    goto fail;
  }

  const char *server_url = cJSON_GetObjectItem(res, "server")->valuestring;

  if (!server_url)
  {
    ESP_LOGE(TAG, "Failed to get chat server");
    goto fail;
  }

  ESP_LOGI(TAG, "Chat server: %s", server_url);

  if (g_ws_client)
    esp_websocket_client_destroy(g_ws_client);

  esp_websocket_client_config_t ws_config = {
      .uri = server_url,
  };

  g_ws_client = esp_websocket_client_init(&ws_config);
  esp_websocket_register_events(g_ws_client, WEBSOCKET_EVENT_DATA, ws_recv, NULL);
  if (g_ws_client == NULL)
  {
    ESP_LOGE(TAG, "Failed to create websocket client");
    goto fail;
  }

  esp_err_t error = esp_websocket_client_start(g_ws_client);

  if (error != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to start websocket client %d", error);
    goto fail;
  }

  while (1)
  {
    if (esp_websocket_client_is_connected(g_ws_client))
      break;
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  cJSON_Delete(res);

  // Read token from NVS
  if (token)
    esp_websocket_client_send_text(g_ws_client, token, strlen(token), portMAX_DELAY);
  else
  {
    char null_str = '\0';
    esp_websocket_client_send_bin(g_ws_client, &null_str, 1, portMAX_DELAY);
  }

  if (wait_ws_res(6000) < 0)
    goto fail;

  char *ws_res_str = cJSON_Print(g_ws_res);
  ESP_LOGI(TAG, "ws_res: %s", ws_res_str);
  free(ws_res_str);

  cJSON *status = cJSON_GetObjectItem(g_ws_res, "status");

  bool to_regist = token == NULL;

  if (status)
  {
    if (strcmp(status->valuestring, "ok") == 0)
    {
      ESP_LOGI(TAG, "Chat server(ws) connected");
      g_chat_server_connected = true;
    }
    else if (strcmp(status->valuestring, "404") == 0)
    {
      ESP_LOGI(TAG, "Chat server(ws) connected, but not found board");
      g_chat_server_connected = true;
      to_regist = true;
    }

    if (!g_chat_server_connected)
    {
      ESP_LOGE(TAG, "Failed to connect chat server, status: %s", status->valuestring);
      goto fail;
    }

    g_ws_client_lock = xSemaphoreCreateMutex();

    if (to_regist)
      regist_board();
    else
    {
      // copy config
      cJSON *config = cJSON_GetObjectItem(g_ws_res, "config");
      if (config)
      {
        cJSON *ice_server = cJSON_GetObjectItem(config, "ice_server");
        if (ice_server)
        {
          strcpy(g_chat_config.turn_server_url, cJSON_GetObjectItem(ice_server, "url")->valuestring);
          strcpy(g_chat_config.turn_server_username, cJSON_GetObjectItem(ice_server, "username")->valuestring);
          strcpy(g_chat_config.turn_server_credential, cJSON_GetObjectItem(ice_server, "credential")->valuestring);
        }
        else
        {
          ESP_LOGE(TAG, "Failed to get ice server");
          goto fail;
        }
      }
      return;
    }
  }

fail:
  ESP_LOGE(TAG, "Failed to connect chat server, please restart the device");
  while (1)
    vTaskDelay(pdMS_TO_TICKS(5000));
}

void chat_task(void *pvParameters)
{

  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open("token", NVS_READONLY, &nvs_handle);

  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to open NVS, register the board");
    connect_chat_server(NULL);
  }

  g_chat_config.token = malloc(96);

  g_chat_config.turn_server_url = malloc(128);
  g_chat_config.turn_server_username = malloc(64);
  g_chat_config.turn_server_credential = malloc(64);
  g_chat_config.chat_state = CHAT_STATE_IDLE;

  // Read token from NVS
  size_t len = 96;
  nvs_get_str(nvs_handle, "token", g_chat_config.token, &len);
  nvs_close(nvs_handle);
  ESP_LOGI(TAG, "Token: %s", g_chat_config.token);

  // Connect to chat server
  connect_chat_server(g_chat_config.token);

  g_pc = NULL;
  /* Test audio */
  xTaskCreate(audio_task, "audio_task", 4096 * 5, NULL, PRIORITY_AUDIO_TASK, NULL);

  while (1)
    vTaskDelay(pdMS_TO_TICKS(5000));
}
