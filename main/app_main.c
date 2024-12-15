#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_netif.h"
#include "driver/temperature_sensor.h"

#include "led.h"
#include "wifimanager.h"
#include "mqtt4.h"
#include "tools.h"
#include "uart.h"
#include "ota.h"
// #include "ir.h"
// #include "esp_heap_trace.h"

#define ECHO_TASK_STACK_SIZE (3072)

static const char *TAG = "main";
volatile int debug;
temperature_sensor_handle_t temp_handle = NULL;

const char *ota_url = CONFIG_DDSU666_OTA;
const char *mqtt4_connect_url = CONFIG_DDSU666_MQTT;

// #define NUM_RECORDS 10
// static heap_trace_record_t trace_record[NUM_RECORDS]; // 该缓冲区必须在内部 RAM 中

void app_main(void)
{
	// debug
	debug = 0;
	// heap_trace_init_standalone(trace_record, NUM_RECORDS);

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	filesys_init();

	ESP_LOGI(TAG, "[APP] Startup..");
	ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("mqtt", ESP_LOG_VERBOSE);
	esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);

	led_configure();

	// 初始化片上温度传感器
	temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-30, 50);
	ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_handle));

	// wifi 初始化完成tag
	int wifi_ok = 0;
	int *wifi_ok_tag = &wifi_ok;
	wifi_init(wifi_ok_tag);

	if (is_exist_ota_tag() == 1 && *wifi_ok_tag == 1)
	{
		mqtt_app_destroy();
		remove_rec_task();

		remove_ota_tag();
		ota_start();
	}

	mqtt_app_start();

	/*
	nec_tx_init();
	nec_rx_init();

	ir_nec_scan_code_t scan_code = {
		.address = 0x0440,
		.command = 0x3003,
	};
	nec_tx(scan_code);
	*/

	uart_init();
	xTaskCreate(uart_loop, "uart_get_data_task", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);
}
