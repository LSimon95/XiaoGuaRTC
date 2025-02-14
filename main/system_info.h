#pragma once


#define configUSE_TRACE_FACILITY 1

#include <esp_err.h>
#include <freertos/FreeRTOS.h>

size_t GetFlashSize();
size_t GetMinimumFreeHeapSize();
size_t GetFreeHeapSize();
void GetMacAddress(char* mac_str);
// CONFIG_IDF_TARGET
void LogRealTimeStats();