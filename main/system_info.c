#include "system_info.h"

#include <freertos/task.h>
#include <esp_log.h>
#include <esp_flash.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_partition.h>
#include <esp_app_desc.h>
#include <esp_ota_ops.h>

#define TAG "SystemInfo"

size_t GetFlashSize()
{
  uint32_t flash_size;
  if (esp_flash_get_size(NULL, &flash_size) != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to get flash size");
    return 0;
  }
  return (size_t)flash_size;
}

size_t GetMinimumFreeHeapSize()
{
  return esp_get_minimum_free_heap_size();
}

size_t GetFreeHeapSize()
{
  return esp_get_free_heap_size();
}

void GetMacAddress(char *mac_str)
{
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(mac_str, 18, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void LogRealTimeStats()
{

  char run_time_stats[512];
  vTaskGetRunTimeStats(run_time_stats);
  ESP_LOGI(TAG, "Run time stats: %s", run_time_stats);

  int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);
}