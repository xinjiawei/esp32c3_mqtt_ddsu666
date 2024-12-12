/*
 * @Author: Caffreyfans
 * @Date: 2021-06-19 17:51:39
 * @LastEditTime: 2021-06-26 23:52:02
 * @Description:
 */
#include "web_handler.h"

#include "wifimanager.h"
#include "ets_sys.h"
#include "form_parser.h"
#include "uart.h"
//#include "ir.h"
// #include "esp_heap_trace.h"

static const char *TAG = "event handler";

#define CHECK_IS_NULL(value, ret) \
  if (value == NULL) return ret;


/*
 *获取系统信息
 **/
static char *get_info_handle() {
  char *response = NULL;
  cJSON *root = cJSON_CreateObject();
  if (root == NULL) return NULL;

#define RET_BUFFER_SIZE 128
#define TMP_BUFFER_SIZE 64
  char ret_buffer[RET_BUFFER_SIZE];
  char tmp_buffer[TMP_BUFFER_SIZE];

  int64_t tick = xTaskGetTickCount();
  snprintf(ret_buffer, RET_BUFFER_SIZE, "%ldS", pdTICKS_TO_MS(tick) / 1000);
  cJSON_AddStringToObject(root, "boot", ret_buffer);

  itoa(heap_caps_get_total_size(MALLOC_CAP_32BIT) / 1024, tmp_buffer, 10);
  snprintf(ret_buffer, RET_BUFFER_SIZE, "%sKB", tmp_buffer);
  cJSON_AddStringToObject(root, "total_heap", ret_buffer);

  itoa(heap_caps_get_free_size(MALLOC_CAP_32BIT) / 1024, tmp_buffer, 10);
  snprintf(ret_buffer, RET_BUFFER_SIZE, "%sKB", tmp_buffer);
  cJSON_AddStringToObject(root, "free_heap", ret_buffer);

  static const char *reset_reason_str[] = {"can not be determined",
                                    "power-on event",
                                    "external pin",
                                    "esp_restart",
                                    "exception/panic",
                                    "interrupt watchdog",
                                    "task watchdog",
                                    "other watchdogs",
                                    "exiting deep sleep mode",
                                    "browout reset",
                                    "SDIO"};
  cJSON_AddStringToObject(root, "reset_reason",
                          reset_reason_str[esp_reset_reason()]);

  wifi_ap_record_t ap;
  esp_wifi_sta_get_ap_info(&ap);
  cJSON_AddStringToObject(root, "wifi_ssid", (char *)ap.ssid);
  esp_netif_ip_info_t ip_info;
  esp_netif_get_ip_info(g_station_netif, &ip_info);
  snprintf(ret_buffer, RET_BUFFER_SIZE, IPSTR, IP2STR(&ip_info.ip));
  cJSON_AddStringToObject(root, "wifi_ip", ret_buffer);

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(ret_buffer, RET_BUFFER_SIZE, MACSTR, MAC2STR(mac));
  cJSON_AddStringToObject(root, "wifi_mac", ret_buffer);

  snprintf(ret_buffer, RET_BUFFER_SIZE, "%d", ap.rssi);
  cJSON_AddStringToObject(root, "wifi_rssi", ret_buffer);

  cJSON_AddStringToObject(root, "build_time", __DATE__);

  // cJSON_AddStringToObject(root, "sketch_size", );

  // cJSON_AddStringToObject(root, "free_sketch_size", );

  cJSON_AddStringToObject(root, "core_version", CORE_VERSION);

  cJSON_AddStringToObject(root, "sdk_version", esp_get_idf_version());

  snprintf(ret_buffer, RET_BUFFER_SIZE, "%ldMHz", ets_get_cpu_frequency());
  cJSON_AddStringToObject(root, "cpu_freq", ret_buffer);

  uint32_t size_flash_chip;
  esp_flash_get_size(NULL, &size_flash_chip);

  snprintf(ret_buffer, RET_BUFFER_SIZE, "%ldMB",
		   size_flash_chip / 1024 / 1024);
  cJSON_AddStringToObject(root, "flash_size", ret_buffer);

  // cJSON_AddStringToObject(root, "flash_speed", );
  size_t total_bytes, used_bytes;
  esp_spiffs_info("spiffs", &total_bytes, &used_bytes);

  snprintf(ret_buffer, RET_BUFFER_SIZE, "%dKB", total_bytes / 1024);
  cJSON_AddStringToObject(root, "fs_total", ret_buffer);

  snprintf(ret_buffer, RET_BUFFER_SIZE, "%dKB", used_bytes / 1024);
  cJSON_AddStringToObject(root, "fs_used", ret_buffer);
  response = cJSON_Print(root);
  cJSON_Delete(root);
  return response;
}

/*
 *打印可用堆内存*/
void print_free_heap() {
  char ret_buffer[RET_BUFFER_SIZE];
  char tmp_buffer[TMP_BUFFER_SIZE];
  itoa(heap_caps_get_free_size(MALLOC_CAP_32BIT) / 1024, tmp_buffer, 10);
  snprintf(ret_buffer, RET_BUFFER_SIZE, "%sKB", tmp_buffer);
  printf("free_heap %s\r\n", ret_buffer);
  }

  /*
   *获取电表电源信息*/
  static char * get_power_info() {
      extern float voltage;
      extern float current;
      extern float a_power;
	  extern float r_power;
	  extern float ap_power;
	  extern float power_factor;
	  extern float power_frequency;
      extern float total_engery;

	  extern int debug;

#define RET_BUFFER_SIZE 128
		#define TMP_BUFFER_SIZE 64
	  char ret_buffer[RET_BUFFER_SIZE];
	  char *response = NULL;
	  cJSON *root = cJSON_CreateObject();
	  if (root == NULL) return NULL;
	  
	  snprintf(ret_buffer, RET_BUFFER_SIZE, "%.1fV", voltage);
	  cJSON_AddStringToObject(root, "volts", ret_buffer);
	  snprintf(ret_buffer, RET_BUFFER_SIZE, "%.3fA", current);
	  cJSON_AddStringToObject(root, "current", ret_buffer);
	  snprintf(ret_buffer, RET_BUFFER_SIZE, "%.1fW", a_power*1000);
	  cJSON_AddStringToObject(root, "ap", ret_buffer);
	  snprintf(ret_buffer, RET_BUFFER_SIZE, "%.1fW", r_power*1000);
	  cJSON_AddStringToObject(root, "rp", ret_buffer);
	  snprintf(ret_buffer, RET_BUFFER_SIZE, "%.1fW", ap_power*1000);
	  cJSON_AddStringToObject(root, "app", ret_buffer);
	  snprintf(ret_buffer, RET_BUFFER_SIZE, "%.3f", power_factor);
	  cJSON_AddStringToObject(root, "pf", ret_buffer);
	  snprintf(ret_buffer, RET_BUFFER_SIZE, "%.2fHz", power_frequency);
	  cJSON_AddStringToObject(root, "pfr", ret_buffer);

	  snprintf(ret_buffer, RET_BUFFER_SIZE, "%.2fkWH", total_engery);
	  cJSON_AddStringToObject(root, "total_e", ret_buffer);
	  
	  snprintf(ret_buffer, RET_BUFFER_SIZE, "%d", get_loop_count());
	  cJSON_AddStringToObject(root, "loop_count", ret_buffer);
	  snprintf(ret_buffer, RET_BUFFER_SIZE, "%d", get_rec_wait());
	  cJSON_AddStringToObject(root, "rec_wait", ret_buffer);
	  snprintf(ret_buffer, RET_BUFFER_SIZE, "%d", debug);
	  cJSON_AddStringToObject(root, "debug", ret_buffer);
	  
	  response = cJSON_Print(root);
	  cJSON_Delete(root);
	  return response;
  }
  
  /*
   *事件处理接口*/
  esp_err_t index_handler(int key, const char *value, char *response_s)
  {
  int buffer = atoi(value);
  char *response;
  switch (key)
  {
  case 0: // brand
	  response = get_power_info();
	  break;
  default:
	  response = get_info_handle();
	  break;  
  }
  strcpy(response_s, response);
  extern int debug;
  if(debug) ESP_LOGI(TAG, "key: %d, response: %s\r\n", key, response);
  free(response);
  return ESP_OK;
}