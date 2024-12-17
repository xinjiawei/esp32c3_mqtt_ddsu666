/* Esptouch example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "wifimanager.h"

#include "led.h"
#include "mqtt4.h"
#include "uart.h"

#include "tools.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const int WIFI_FAIL_BIT = BIT2;
static int ESPTOUCH_TRY_CONNECT_BIT = 0;
static const char *TAG = "wifimanager";
static const char *TAGTOUCH = "esp_touch_v1";

static const int ESP_MAXINUM_RETRY = 2;
static int s_retry_num = 0;

esp_netif_t *g_station_netif = NULL;

/*
 *智能配网任务入口*/
static void smartconfig_task(void *parm)
{
	EventBits_t uxBits;
	ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
	smartconfig_start_config_t cfg = {
		.enable_log = true,
		.esp_touch_v2_enable_crypt = false,
		.esp_touch_v2_key = NULL};
	ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
	while (1)
	{
		uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | WIFI_FAIL_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
		if (uxBits & CONNECTED_BIT)
		{
			ESP_LOGI(TAGTOUCH, "==WiFi Connected to ap");
		}
		if (uxBits & WIFI_FAIL_BIT)
		{
			ESP_LOGW(TAGTOUCH, "==WiFi Connected err, ubit: %ld", (unsigned long int) uxBits);
			if (ESPTOUCH_TRY_CONNECT_BIT)
			{
				ESP_LOGW(TAGTOUCH, "==WiFi esptouch Connected err, restart");
				esp_restart();
			}
		}
		if (uxBits & ESPTOUCH_DONE_BIT)
		{
			ESP_LOGI(TAGTOUCH, "==WiFi esptouch smartconfig over");
			esp_smartconfig_stop();

			// vTaskDelete(NULL);// 因为重启, 这个没必要了

			// 由于串口任务和mqtt在配网期间手动停止, 所以通过重启来重启串口任务.
			ESP_LOGW(TAG, "will restart");
			esp_restart();
		}
	}
}

/*
 *wifi事件处理*/
static void event_handler(void *arg, esp_event_base_t event_base,
						  int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		ESP_LOGI(TAG, "try to connect to the AP");
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		if (s_retry_num < ESP_MAXINUM_RETRY)
		{
			esp_wifi_connect();
			vTaskDelay(200 / portTICK_PERIOD_MS);
			ESP_LOGI(TAG, "retry to connect to the AP,  %d times try", s_retry_num + 1);
			s_retry_num++;
		}
		else
		{
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
			xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
		}
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		led_blink();
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
	}
	else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE)
	{
		ESP_LOGI(TAG, "Scan done");
	}
	else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL)
	{
		ESP_LOGI(TAG, "Found channel");
		led_loop(2);
	}
	else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
	{
		ESP_LOGI(TAG, "Got SSID and password");

		// set tag
		ESPTOUCH_TRY_CONNECT_BIT = 1;

		smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;

		uint8_t ssid[33] = {0};
		uint8_t password[65] = {0};
		uint8_t rvd_data[33] = {0};

		wifi_config_t wifi_config;
		bzero(&wifi_config, sizeof(wifi_config_t));
		memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
		memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

#ifdef CONFIG_SET_MAC_ADDRESS_OF_TARGET_AP
		wifi_config.sta.bssid_set = evt->bssid_set;
		if (wifi_config.sta.bssid_set == true)
		{
			ESP_LOGI(TAG, "Set MAC address of target AP: " MACSTR " ", MAC2STR(evt->bssid));
			memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
		}
#endif

		memcpy(ssid, evt->ssid, sizeof(evt->ssid));
		memcpy(password, evt->password, sizeof(evt->password));
		ESP_LOGI(TAG, "ESPTOUCH SSID:%s", ssid);
		ESP_LOGI(TAG, "ESPTOUCH PASSWORD:%s", password);
		if (evt->type == SC_TYPE_ESPTOUCH_V2)
		{
			ESP_ERROR_CHECK(esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)));
			ESP_LOGI(TAG, "RVD_DATA:");
			for (int i = 0; i < 33; i++)
			{
				printf("%02x ", rvd_data[i]);
			}
			printf("\n");
		}

		ESP_ERROR_CHECK(esp_wifi_disconnect());
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

		esp_wifi_connect();
	}
	else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
	{
		ESP_LOGI(TAG, "SC_EVENT_SEND_ACK_DONE");
		xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
	}
}

/*
 *主动连接处理*/
static bool wifi_connect_to_ap(const char *ssid, const char *password)
{
	int ap_authmode = WIFI_AUTH_WPA_WPA2_PSK;
	if (password[0] == '\0')
	{
		ESP_LOGW(TAG, "password para is null");
		ap_authmode = WIFI_AUTH_OPEN;
	}
	wifi_config_t sta_config = {
		.sta = {.threshold.authmode = ap_authmode,
				.pmf_cfg = {.capable = true, .required = false}},
	};
	strcpy((char *)sta_config.sta.ssid, ssid);
	strcpy((char *)sta_config.sta.password, password);
	ESP_LOGI(TAG, "connect wifi %s, %s", ssid, password);
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
	 * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). The
	 * bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT,
										   pdTRUE, pdTRUE, pdMS_TO_TICKS(10000));

	/* The event will not be processed after unregister */
	/* xEventGroupWaitBits() returns the bits before the call returned, hence we
	 * can test which event actually happened. */
	if (bits & CONNECTED_BIT)
	{
		esp_wifi_set_storage(WIFI_STORAGE_FLASH);
		return true;
	}
	return false;
}

/*
 *启动ap模式*/
bool wifi_start_ap()
{
	char ssid[32];
	snprintf(ssid, 32, "esp32c3");
	wifi_config_t wifi_config = {
		.ap = {
			.ssid_len = strlen(ssid),
			.channel = 11,
			.password = "",
			.max_connection = 2,
			.authmode = WIFI_AUTH_OPEN,
			.pmf_cfg = {
				.required = true,
			}},
	};
	strcpy((char *)wifi_config.ap.ssid, ssid);
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	return true;
}

/*
 *启动配网入口*/
static void start_esptouch_v1(void)
{
	remove_rec_task();	// 不启动uart任务
	mqtt_app_destroy(); // 不启动mqtt
	// 配网的优先级应该是最高的

	// 优先级最大为24
	xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 24, NULL);

	ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
}

/*
 *扫描周围ap*/
uint8_t wifi_scan_ap(wifi_ap_record_t *ap_infos, const uint8_t size)
{
	uint16_t number = size;
	uint16_t ap_count = 0;
	if (ap_infos == NULL || size == 0)
	{
		return ap_count;
	}

	esp_wifi_scan_start(NULL, true);
	esp_wifi_scan_get_ap_records(&number, ap_infos);
	esp_wifi_scan_get_ap_num(&ap_count);
	return ap_count;
}
/*
 *wifi重置*/
void wifi_reset()
{
	ESP_LOGW(TAG, "reset wifi, will restart and auto esptouch");
	wifi_config_t sta_config = {
		.sta.ssid = "",
		.sta.password = ""};

	ESP_ERROR_CHECK(esp_wifi_disconnect());
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
	esp_restart();
}

/*
 *wifi连接初始化*/
void wifi_init(int *wifi_ok)
{
	s_wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_handler_register(
		WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(
		IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_wifi_init(&cfg);
	esp_netif_create_default_wifi_ap();
	g_station_netif = esp_netif_create_default_wifi_sta();
	wifi_config_t sta_config;
	esp_wifi_get_config(WIFI_IF_STA, &sta_config);
	esp_wifi_set_config(WIFI_IF_STA, &sta_config);
	if (wifi_connect_to_ap((char *)sta_config.sta.ssid, (char *)sta_config.sta.password) != true)
	{
		ESP_LOGW(TAG, "connect wifi %s, %s error", sta_config.sta.ssid, sta_config.sta.password);
		// wifi_start_ap();
		*wifi_ok = 0;
		start_esptouch_v1();
	}
	else
	{
		*wifi_ok = 1;
		ESP_LOGI(TAG, "connected wifi %s, %s", sta_config.sta.ssid, sta_config.sta.password);
	}
}
