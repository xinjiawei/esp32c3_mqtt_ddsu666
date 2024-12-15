#pragma once
#ifndef WIFI_MANAGER_H_
#define WIFI_MANAGER_H_
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_eap_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sys/unistd.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"

#include "esp_wifi.h"

void wifi_init(int *wifi_ok);
void wifi_reset();
bool wifi_start_ap();
uint8_t wifi_scan_ap(wifi_ap_record_t *ap_infos, const uint8_t size);

extern esp_netif_t *g_station_netif;
#endif  // WIFI_MANAGER_H_
