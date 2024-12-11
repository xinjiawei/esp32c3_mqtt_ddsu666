/*
 * @Author: Caffreyfans
 * @Date: 2021-06-19 17:51:23
 * @LastEditTime: 2021-06-26 23:49:29
 * @Description: this is web handler for response ajax
 */
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

#include "version.h"
esp_err_t index_handler(int key, const char *value, char *response);
void print_free_heap();
#endif  // HANDLER_H_