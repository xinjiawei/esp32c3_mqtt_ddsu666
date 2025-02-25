/*
 * @Author: Caffreyfans
 * @Date: 2021-06-19 17:51:39
 * @LastEditTime: 2021-06-26 23:52:02
 * @Description:
 */
#include "web_handler.h"
#include "version.h"
#include "wifimanager.h"
// #include "ets_sys.h"
#include "form_parser.h"
#include "uart.h"
#include <rom/ets_sys.h>
// #include "ir.h"
//  #include "esp_heap_trace.h"

#define RET_BUFFER_SIZE 128
#define TMP_BUFFER_SIZE 64

static const char *TAG = "event handler";

#define CHECK_IS_NULL(value, ret) \
	if (value == NULL)            \
		return ret;

/*
 *获取系统信息
 **/
// todo 会触发 Guru Meditation Error: Core  0 panic'ed (Load access fault). Exception was unhandled.
static void get_info_handle(char **response)
{
	cJSON *root = cJSON_CreateObject();
	if (root == NULL)
		return;

	char ret_buffer[RET_BUFFER_SIZE];
	char tmp_buffer[TMP_BUFFER_SIZE];
	extern int debug;

	int64_t tick = xTaskGetTickCount();
	snprintf(ret_buffer, RET_BUFFER_SIZE, "%ldS", (unsigned long int) pdTICKS_TO_MS(tick) / 1000);
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
											 "SDIO",
											 "USB peripheral",
											 "JTAG",
											 "efuse error",
											 "power glitch detected",
											 "CPU lock up"};
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

	snprintf(ret_buffer, RET_BUFFER_SIZE, "%ldMHz", (unsigned long int) ets_get_cpu_frequency());
	cJSON_AddStringToObject(root, "cpu_freq", ret_buffer);

	uint32_t size_flash_chip;
	esp_flash_get_size(NULL, &size_flash_chip);

	snprintf(ret_buffer, RET_BUFFER_SIZE, "%ldMB",
			 (unsigned long int) size_flash_chip / 1024 / 1024);
	cJSON_AddStringToObject(root, "flash_size", ret_buffer);

	// cJSON_AddStringToObject(root, "flash_speed", );
	size_t total_bytes, used_bytes;
	esp_spiffs_info("spiffs", &total_bytes, &used_bytes);

	snprintf(ret_buffer, RET_BUFFER_SIZE, "%zuKB", total_bytes / 1024);
	cJSON_AddStringToObject(root, "fs_total", ret_buffer);

	snprintf(ret_buffer, RET_BUFFER_SIZE, "%zuKB", used_bytes / 1024);
	cJSON_AddStringToObject(root, "fs_used", ret_buffer);

	extern temperature_sensor_handle_t temp_handle;
	// 启用温度传感器
	ESP_ERROR_CHECK(temperature_sensor_enable(temp_handle));
	// 获取传输的传感器数据
	float tsens_out;
	ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_handle, &tsens_out));
	snprintf(ret_buffer, RET_BUFFER_SIZE, "%.1f", tsens_out);
	cJSON_AddStringToObject(root, "core_t", ret_buffer);
	// 温度传感器使用完毕后，禁用温度传感器，节约功耗
	ESP_ERROR_CHECK(temperature_sensor_disable(temp_handle));

	snprintf(ret_buffer, RET_BUFFER_SIZE, "%d", debug);
	cJSON_AddStringToObject(root, "is_debug", ret_buffer);

	*response = cJSON_Print(root);
	cJSON_Delete(root);
}

/*
 *打印可用堆内存*/
void print_free_heap()
{
	char ret_buffer[RET_BUFFER_SIZE];
	char tmp_buffer[TMP_BUFFER_SIZE];
	itoa(heap_caps_get_free_size(MALLOC_CAP_32BIT) / 1024, tmp_buffer, 10);
	snprintf(ret_buffer, RET_BUFFER_SIZE, "%sKB", tmp_buffer);
	printf("free_heap %s\r\n", ret_buffer);
}

/*
 *获取电表电源信息*/
static void get_power_info(char **response)
{
	extern float voltage;
	extern float current;
	extern float a_power;
	extern float r_power;
	extern float ap_power;
	extern float power_factor;
	extern float power_frequency;
	extern float total_engery;

	char ret_buffer[RET_BUFFER_SIZE];
	cJSON *root = cJSON_CreateObject();
	if (root == NULL)
		return;

	snprintf(ret_buffer, RET_BUFFER_SIZE, "%.1f", voltage);
	cJSON_AddStringToObject(root, "volts", ret_buffer);
	snprintf(ret_buffer, RET_BUFFER_SIZE, "%.3f", current);
	cJSON_AddStringToObject(root, "current", ret_buffer);
	snprintf(ret_buffer, RET_BUFFER_SIZE, "%.1f", a_power * 1000);
	cJSON_AddStringToObject(root, "ap", ret_buffer);
	snprintf(ret_buffer, RET_BUFFER_SIZE, "%.1f", r_power * 1000);
	cJSON_AddStringToObject(root, "rp", ret_buffer);
	snprintf(ret_buffer, RET_BUFFER_SIZE, "%.1f", ap_power * 1000);
	cJSON_AddStringToObject(root, "app", ret_buffer);
	snprintf(ret_buffer, RET_BUFFER_SIZE, "%.3f", power_factor);
	cJSON_AddStringToObject(root, "pf", ret_buffer);
	snprintf(ret_buffer, RET_BUFFER_SIZE, "%.2fHz", power_frequency);
	cJSON_AddStringToObject(root, "pfr", ret_buffer);

	snprintf(ret_buffer, RET_BUFFER_SIZE, "%.2f", total_engery);
	cJSON_AddStringToObject(root, "total_e", ret_buffer);

	snprintf(ret_buffer, RET_BUFFER_SIZE, "%d", get_loop_count());
	cJSON_AddStringToObject(root, "loop_count", ret_buffer);
	snprintf(ret_buffer, RET_BUFFER_SIZE, "%ds", get_rec_wait());
	cJSON_AddStringToObject(root, "rec_wait", ret_buffer);

	int timestamp = (int)pdTICKS_TO_MS((int64_t)xTaskGetTickCount()) / 1000;
	int last_uart_timestamp = get_uart_tx_rx_timestamp();
	snprintf(ret_buffer, RET_BUFFER_SIZE, "%ds", timestamp - last_uart_timestamp);
	cJSON_AddStringToObject(root, "after_loop", ret_buffer);

	*response = cJSON_Print(root);
	cJSON_Delete(root);
}

/*
 *事件处理接口*/
char *index_handler(int key, const char *value)
{
	int buffer = atoi(value);
	char *response_t = NULL;
	switch (key)
	{
	case 0: // brand
		get_power_info(&response_t);
		break;
	default:
		get_info_handle(&response_t);
		break;
	}
	extern int debug;
	if (debug)
		ESP_LOGI(TAG, "key: %d, value: %d, response: %s\r\n", key, buffer, response_t);
	return response_t;
}