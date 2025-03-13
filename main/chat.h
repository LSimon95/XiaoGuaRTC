#pragma once

#include <cJSON.h>

enum chat_state_t
{
  CHAT_STATE_WAIT_REGIST = 0,
  CHAT_STATE_IDLE,
  CHAT_STATE_CONNECTING,
  CHAT_STATE_CONNECTED_WAIT_WS_TURN,
  CHAT_STATE_CONNECTED,
};

enum emo_state_t
{
  EMO_NEUTRAL = 0,
  EMO_HAPPY,
  EMO_SAD,
  EMO_ANGRY,
};

typedef struct
{
  char *token;
  enum chat_state_t chat_state;
  char *turn_server_url;
  char *turn_server_username;
  char *turn_server_credential;
} chat_config_t;

void chat_task(void *pvParameters);
void wake_play_code();
void wake_peer_connect();
int ws_send_cmd_and_wait_json(const char *cmd, cJSON *data, cJSON **res, int timeout, const char* bin_data, int bin_len);
int chat_send_audio(const char*  data, size_t len);