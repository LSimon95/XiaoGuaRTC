#include "request.h"

#include <esp_log.h>
#include <esp_crt_bundle.h>

static const char *TAG = "REQUEST";

esp_http_client_handle_t init_handle(esp_http_client_method_t method, const char *url, cJSON *header, const char *content_type, const char *accept_type)
{
  esp_http_client_config_t config = {};
  config.url = url;
  config.crt_bundle_attach = esp_crt_bundle_attach;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == NULL)
  {
    ESP_LOGE(TAG, "Failed to initialize HTTP client");
    return NULL;
  }

  if (method == HTTP_METHOD_GET)
    ESP_LOGI(TAG, "Sending GET request to %s", url);
  else if (method == HTTP_METHOD_POST)
    ESP_LOGI(TAG, "Sending POST request to %s", url);

  esp_http_client_set_method(client, method);

  // Set headers
  if (header != NULL)
  {
    cJSON *header_item = cJSON_GetArrayItem(header, 0);
    while (header_item != NULL)
    {
      esp_http_client_set_header(client, header_item->string, header_item->valuestring);
      header_item = header_item->next;
    }
  }

  esp_http_client_set_header(client, "Content-Type", content_type);
  if (accept_type)
    esp_http_client_set_header(client, "Accept", accept_type);

  return client;
}

int get_json_response(esp_http_client_handle_t client, cJSON **res)
{
  // Fetch headers
  int64_t content_len = esp_http_client_fetch_headers(client);
  if (content_len <= 0)
    ESP_LOGW(TAG, "Failed to fetch headers: %d", (int)content_len);

  // Get response
  int status_code = esp_http_client_get_status_code(client);
  if (status_code != 200)
  {
    ESP_LOGE(TAG, "HTTP request failed with status code %d", status_code);
  }

  // Parse response
  int64_t content_length = esp_http_client_get_content_length(client);
  if (content_length <= 0)
    goto cleanup;

  char *buffer = malloc(content_length + 1);
  if (buffer == NULL)
  {
    ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
    goto cleanup;
  }

  ESP_LOGI(TAG, "Reading response %d bytes", (int)content_length);

  int read_len = esp_http_client_read(client, buffer, content_length + 1);
  if (read_len <= 0)
  {
    ESP_LOGE(TAG, "Failed to read response %d", read_len);
    free(buffer);
    goto cleanup;
  }

  buffer[read_len] = '\0';
  if (res)
    *res = cJSON_Parse(buffer);
  free(buffer);

cleanup:
  esp_http_client_cleanup(client);
  return status_code;
}

int request(esp_http_client_method_t method, const char *url, cJSON *header, cJSON *body, cJSON **res)
{
  if (res)
    *res = NULL;
  esp_http_client_handle_t client = init_handle(method, url, header, "application/json", "application/json");
  if (client == NULL)
    return -1;

  // Set body
  char *body_str = NULL;
  if (body != NULL)
  {
    body_str = cJSON_PrintUnformatted(body);
    cJSON_Minify(body_str);
  }

  // client open and write data
  esp_err_t err = esp_http_client_open(client, body_str ? strlen(body_str) : 0);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %d", err);
    goto fail;
  }

  // Write data
  if (body_str)
  {
    int write_len = esp_http_client_write(client, body_str, strlen(body_str));
    if (write_len <= 0)
    {
      ESP_LOGE(TAG, "Failed to write data: %d", write_len);
      goto fail;
    }
  }

  return get_json_response(client, res);

fail:
  esp_http_client_cleanup(client);
  return -1;
}

int request_binary_body(esp_http_client_method_t method, const char *url, cJSON *header, void *content, int content_size, cJSON **res)
{
  if (res)
    *res = NULL;
  esp_http_client_handle_t client = init_handle(method, url, header, "application/octet-stream", "application/json");

  if (client == NULL)
    return -1;

  // client open and write data
  esp_err_t err = esp_http_client_open(client, content_size);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %d", err);
    goto fail;
  }

  // Write data
  int write_len = esp_http_client_write(client, content, content_size);
  if (write_len <= 0)
  {
    ESP_LOGE(TAG, "Failed to write data: %d", write_len);
    goto fail;
  }

  return get_json_response(client, res);

fail:
  esp_http_client_cleanup(client);
  return -1;
}

int request_binary_response(esp_http_client_method_t method, const char *url, cJSON *header, cJSON *body, void *content, int *content_size)
{
  esp_http_client_handle_t client = init_handle(method, url, header, "application/json", NULL);
  if (client == NULL)
    return -1;

  // Set body
  char *body_str = NULL;
  if (body != NULL)
  {
    body_str = cJSON_PrintUnformatted(body);
    cJSON_Minify(body_str);
  }

  // client open and write data
  esp_err_t err = esp_http_client_open(client, body_str ? strlen(body_str) : 0);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %d", err);
    goto fail;
  }

  // Write data
  if (body_str)
  {
    int write_len = esp_http_client_write(client, body_str, strlen(body_str));
    if (write_len <= 0)
    {
      ESP_LOGE(TAG, "Failed to write data: %d", write_len);
      goto fail;
    }
  }

  // Fetch headers
  int64_t content_len = esp_http_client_fetch_headers(client);
  if (content_len <= 0)
    ESP_LOGW(TAG, "Failed to fetch headers: %d", (int)content_len);

  // Get response
  int status_code = esp_http_client_get_status_code(client);
  if (status_code != 200)
  {
    ESP_LOGE(TAG, "HTTP request failed with status code %d", status_code);
  }

  // Parse response
  int64_t content_length = esp_http_client_get_content_length(client);
  if (content_length <= 0)
    goto cleanup;

  if (*content_size < content_length)
    ESP_LOGW(TAG, "Content size is too small, expected %d bytes", (int)content_length);

  ESP_LOGI(TAG, "Reading response %d bytes", (int)content_length);

  int read_len = esp_http_client_read(client, content, *content_size);
  if (read_len <= 0)
  {
    ESP_LOGE(TAG, "Failed to read response %d", read_len);
    goto cleanup;
  }
  *content_size = read_len;

cleanup:
  esp_http_client_cleanup(client);
  return status_code;

fail:
  esp_http_client_cleanup(client);
  return -1;
}
