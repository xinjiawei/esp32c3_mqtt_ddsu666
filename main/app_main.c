#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"

#include "led.h"
#include "wifimanager.h"
#include "mqtt4.h"
#include "tools.h"
#include "uart.h"
//#include "ir.h"
//#include "esp_heap_trace.h"

#define ECHO_TASK_STACK_SIZE (3072)

static const char *TAG = "main";
int debug;

// #define NUM_RECORDS 10
//static heap_trace_record_t trace_record[NUM_RECORDS]; // 该缓冲区必须在内部 RAM 中

void app_main(void)
{
	//debug
	debug = 0;
	//heap_trace_init_standalone(trace_record, NUM_RECORDS);
	ESP_LOGI(TAG, "[APP] Startup..");
	ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
	esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
	esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
	esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
	esp_log_level_set("transport", ESP_LOG_VERBOSE);
	esp_log_level_set("outbox", ESP_LOG_VERBOSE);

	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_vfs_spiffs_conf_t conf = {.base_path = "/ir",
								  .partition_label = "spiffs",
								  .max_files = 5,
								  .format_if_mount_failed = true};

	// Use settings defined above to initialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);
	if (ret != ESP_OK)
	{
		if (ret == ESP_FAIL)
		{
			ESP_LOGI(TAG, "Failed to mount or format filesystem");
		}
		else if (ret == ESP_ERR_NOT_FOUND)
		{
			ESP_LOGI(TAG, "Failed to find SPIFFS partition");
		}
		else
		{
			ESP_LOGI(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return;
	}
	size_t total = 0, used = 0;
	ret = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret != ESP_OK)
	{
		ESP_LOGI(TAG,
				 "Failed to get SPIFFS partition information (%s). Formatting...",
				 esp_err_to_name(ret));
		esp_spiffs_format(conf.partition_label);
		return;
	}
	else
	{
		ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
	}

	led_configure();

	wifi_init();

	//nec_tx_init();
	//nec_rx_init();

	//ir_nec_scan_code_t scan_code = {
	//	.address = 0x0440,
	//	.command = 0x3003,
	//};
	//nec_tx(scan_code);

	uart_init();
	
	xTaskCreate(uart_loop, "uart_get_data_task", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);

	mqtt_app_start();
}
