#pragma once
#ifndef HANDLER_H_
#define HANDLER_H_
#include "esp_err.h"
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_flash.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "driver/temperature_sensor.h"

#include "version.h"
char *index_handler(int key, const char *value);
void print_free_heap();
#endif  // HANDLER_H_