#pragma once

#include <cJSON.h>
#include <esp_http_client.h>

int request(esp_http_client_method_t method, const char *url, cJSON *header, cJSON *body, cJSON **res);
int request_binary_body(esp_http_client_method_t method, const char *url, cJSON *header, void *content, int content_size, cJSON **res);
int request_binary_response(esp_http_client_method_t method, const char *url, cJSON *header, cJSON *body, void* content, int* content_size);