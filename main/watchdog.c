/* Task_Watchdog Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "watchdog.h"

#define TWDT_TIMEOUT_MS 10000

static const char *TAG = "watchdog";

// 触发重启, 实测不需要menu打开CONFIG_ESP_TASK_WDT_INIT
void esp_task_wdt_isr_user_handler(void)
{
	esp_restart();
}

// 初始化看门狗
void task_watchdog_init()
{
	// If the TWDT was not initialized automatically on startup, manually intialize it now
	esp_task_wdt_config_t twdt_config = {
		.timeout_ms = TWDT_TIMEOUT_MS,
		.idle_core_mask = (1 << CONFIG_FREERTOS_NUMBER_OF_CORES) - 1, // Bitmask of all cores
		.trigger_panic = false,
	};
	ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));
	ESP_LOGI(TAG, "TWDT initialized\n");
}