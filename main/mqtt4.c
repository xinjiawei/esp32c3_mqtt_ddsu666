#include "mqtt4.h"

#include "led.h"
#include "ota.h"
#include "tools.h"
#include "uart.h"
#include "web_handler.h"
#include "wifimanager.h"
// #include "ir.h"
//  #include "esp_heap_trace.h"

static const char *TAG = "mqtt";
char response_s[5120];
static int mqtt_disconnect_count = 0;
// mqtt允许的连续断开次数最大值
const static int mqtt_disconnect_count_max = 5;

static esp_mqtt_client_handle_t client;

static int is_start_mqtt = 1;
static void log_error_if_nonzero(const char *message, int error_code)
{
	if (error_code != 0)
	{
		ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
	}
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	char *chip_id = get_chip_id();
	snprintf(response_s, 52, "esp32_%s", chip_id);
	free(chip_id);

	ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
	esp_mqtt_event_handle_t event = event_data;
	esp_mqtt_client_handle_t client = event->client;
	int msg_id = 0;
	switch ((esp_mqtt_event_id_t)event_id)
	{
	case MQTT_EVENT_CONNECTED:
		mqtt_disconnect_count = 0;
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
		msg_id = esp_mqtt_client_publish(client, "online", response_s, 0, 0, 0);
		ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

		// msg_id = esp_mqtt_client_subscribe(client, "dht20", 0);
		// ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

		msg_id = esp_mqtt_client_subscribe(client, "sysop-get", 0);
		ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

		msg_id = esp_mqtt_client_subscribe(client, "sysop-set", 0);
		ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

		break;
	case MQTT_EVENT_DISCONNECTED:
		++mqtt_disconnect_count;
		ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED, count:%d", mqtt_disconnect_count);
		led_loop(3);
		if (mqtt_disconnect_count == mqtt_disconnect_count_max)
		{
			esp_restart();
		}
		break;

	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
		// msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
		// ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_PUBLISHED:
		ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_DATA:
		ESP_LOGI(TAG, "MQTT_EVENT_DATA");
		printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
		printf("DATA=%.*s\r\n", event->data_len, event->data);

		char *c_topic = get_len_str(event->topic, event->topic_len);
		char *c_data = get_len_str(event->data, event->data_len);
		char *response = response_s;
		if (strcmp(c_topic, "sysop-get") == 0)
		{
			free(c_topic);
			// todo 执行可能导致系统崩溃, 怀疑是c99标准和nano format兼容问题
			if (strcmp(c_data, "info-sys") == 0)
			{
				printf("trigger get sys info \r\n");
				char *response_t = index_handler(-1, "");
				strcpy(response, response_t);
				free(response_t);
			}
			if (strcmp(c_data, "info-power") == 0)
			{
				printf("trigger get power info \r\n");
				char *response_t = index_handler(0, "");
				strcpy(response, response_t);
				free(response_t);
			}
			free(c_data);
		}
		if (strcmp(c_topic, "sysop-set") == 0)
		{
			free(c_topic);
			if (strcmp(c_data, "reset_wifi") == 0)
			{
				wifi_reset();
			}
			if (strcmp(c_data, "restart_os") == 0)
			{
				esp_restart();
			}
			if (strcmp(c_data, "ota_update") == 0)
			{
				response = "ota update";
				create_ota_tag();
			}

			if (strcmp(c_data, "remove_uart_rec_task") == 0)
			{
				remove_rec_task();
				response = "remove_uart_rec_task";
			}
			if (strcmp(c_data, "change_rec_wait1") == 0)
			{
				change_rec_wait(1);
				response = "change_rec_wait1";
			}
			if (strcmp(c_data, "change_rec_wait5") == 0)
			{
				change_rec_wait(5);
				response = "change_rec_wait5";
			}
			if (strcmp(c_data, "change_rec_wait60") == 0)
			{
				change_rec_wait(60);
				response = "change_rec_wait60";
			}
			if (strcmp(c_data, "clear_total_engery") == 0)
			{
				set_is_clear_total_engery();
				response = "clear_total_engery, you should manual reboot";
			}
			if (strcmp(c_data, "debug_mode") == 0)
			{
				debug_switch();
				extern int debug;
				if (debug)
				{
					response = "debug_mode on";
				}
				else
					response = "debug_mode off";
			}
			free(c_data);
		}
		msg_id = esp_mqtt_client_publish(client, "esp32_response", response, 0, 0, 0);
		extern int debug;
		if (debug)
			printf("sent sys info successful, msg_id=%d, resp: %s\r\n", msg_id, response);
		// free(response);
		led_blink();
		break;
	case MQTT_EVENT_ERROR:
		ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
		if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
		{
			log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
			log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
			log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
			ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
		}
		break;
	default:
		ESP_LOGI(TAG, "Other event id:%d", event->event_id);
		break;
	}
	print_free_heap();
}

void mqtt_app_start()
{
	if (!is_start_mqtt)
	{
		return;
	}
	extern const char *mqtt4_connect_url;
	esp_mqtt_client_config_t mqtt_cfg = {
		.broker.address.uri = mqtt4_connect_url,
		.task.priority = 10,
		.task.stack_size = 10240,
		.network.refresh_connection_after_ms = 55000 // 55秒刷新连接
	};

	client = esp_mqtt_client_init(&mqtt_cfg);
	/* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	esp_mqtt_client_start(client);
}

void mqtt_app_destroy()
{
	is_start_mqtt = 0;
	esp_mqtt_client_destroy(client); // 没必要
}