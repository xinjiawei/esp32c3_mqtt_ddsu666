#include "uart.h"
#include "tools.h"
#include "watchdog.h"
/**
 * This is a example which echos any data it receives on UART back to the sender using RS485 interface in half duplex mode.
 */
static const char *TAG = "RS485_uart";
static const char *dogTAG = "watchdog";

// Note: Some pins on target chip cannot be assigned for UART communication.
// Please refer to documentation for selected board and target to configure pins using Kconfig.
#define ECHO_TEST_TXD (5)
#define ECHO_TEST_RXD (6)

// RTS for RS485 Half-Duplex Mode manages DE/~RE
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)

// CTS is not used in RS485 Half-Duplex Mode
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define BUF_SIZE (127)

// Read packet timeout
#define PACKET_READ_TICS (100 / portTICK_PERIOD_MS)
#define ECHO_TASK_STACK_SIZE (2048)
#define ECHO_TASK_PRIO (10)
#define ECHO_UART_PORT (1)

// Timeout threshold for UART = number of symbols (~10 tics) with unchanged state on receive pin
#define ECHO_READ_TOUT (3) // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks

const static int uart_num = ECHO_UART_PORT;
typedef unsigned char byte;
static char ddsu666_params[8] = {0x01, 0x03, 0x20, 0x00, 0x00, 0x12, 0x00, 0x00};
static char ddsu666_Ep[8] = {0x01, 0x03, 0x40, 0x00, 0x00, 0x02, 0x00, 0x00};
static char ddsu666_clean_total_engery[11] = {0x01, 0x10, 0x00, 0x02, 0x00, 0x01, 0x02, 0x00, 0x01, 0x00, 0x00};

volatile float voltage = 0.0;
volatile float current = 0.0;
volatile float a_power = 0.0;
volatile float r_power = 0.0;
volatile float ap_power = 0.0;
volatile float power_factor = 0.0;
volatile float power_frequency = 0.0;

volatile float total_engery = 0.0;

static uint8_t *data_rec_p;
// 重置电能计量位
static int is_clear_total_engery = 0;
static volatile int uart_tx_rx_timestamp = 0;
extern volatile int debug;

// 循环开关
static int loop = 1;
// 循环计数
static volatile int loop_count = 0;
// 开机默认5秒一次循环
static int rec_wait = 5;
// 300循环后为60秒一次循环
static int stable_rec_wait = 60;

static int temp_max_loop = 300;

// 串口初始化
void uart_init()
{
	uart_config_t uart_config = {
		.baud_rate = 9600,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};

	// Set UART log level
	esp_log_level_set(TAG, ESP_LOG_INFO);

	ESP_LOGI(TAG, "Start uart configure.");

	// Install UART driver (we don't need an event queue here)
	// In this example we don't even use a buffer for sending data.
	ESP_ERROR_CHECK(uart_driver_install(uart_num, BUF_SIZE * 2, 0, 0, NULL, 0));

	// Configure UART parameters
	ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

	ESP_LOGI(TAG, "UART set pins, mode and install driver.");

	// Set UART pins as per KConfig settings
	ESP_ERROR_CHECK(uart_set_pin(uart_num, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

	// Set RS485 half duplex mode
	ESP_ERROR_CHECK(uart_set_mode(uart_num, UART_MODE_RS485_HALF_DUPLEX));

	// Set read timeout of UART TOUT feature
	ESP_ERROR_CHECK(uart_set_rx_timeout(uart_num, ECHO_READ_TOUT));

	// Allocate buffers for UART rec
	data_rec_p = (uint8_t *)malloc(sizeof(uint8_t) * BUF_SIZE);

	// 看门狗初始化
	task_watchdog_init();
}

/*
 *串口发送信息*/
static void uart_send(char data_p[], int len)
{

	int crc = crc_cal(data_p, len - 2);
	data_p[len - 2] = crc & 0xFF;		   // 校验位低
	data_p[len - 1] = (crc & 0xFF00) >> 8; // 校验位高

	ESP_LOGI(TAG, "UART send len: %d", len);
	if (debug)
	{
		printf("crc: 0x%02X, 0x%02X \r\n", crc & 0xFF, (crc & 0xFF00) >> 8);
		ESP_LOGI(TAG, "UART send data:\r\n");

		for (size_t i = 0; i < len; i++)
		{
			printf("0x%02x ", (uint8_t)data_p[i]);
		}
		printf("\r\n");
	}
	if (uart_write_bytes(uart_num, data_p, len) != len)
	{
		ESP_LOGE(TAG, "Send data critical failure.");
		// add your code to handle sending failure here
		abort();
	}
}

/*
 *串口接受信息*/
static int uart_rec(uint8_t *data_rec_p)
{
	// Read data from UART
	return uart_read_bytes(uart_num, data_rec_p, sizeof(uint8_t) * BUF_SIZE, PACKET_READ_TICS);
}

/*
 *解析串口接受信息*/
static void get_rec_data(uint8_t *data_rec_p, int data_rec_len)
{
	uint8_t byte_i;
	// Write data back to UART
	if (data_rec_len > 0)
	{
		uint8_t hexArray[128];
		ESP_LOGI(TAG, "Received %u bytes:", data_rec_len);
		for (int i = 0; i < data_rec_len; i++)
		{
			byte_i = data_rec_p[i];
			if (debug)
			{
				printf("i: %d, int:%d, 0x%02X\r\n", i, data_rec_p[i], byte_i);
			}
			hexArray[i] = byte_i;
		}
		if (debug)
			printf("end\r\n");
		if (data_rec_len == 41)
		{
			volatile float *voltage_p = &voltage;
			volatile float *current_p = &current;
			volatile float *a_power_p = &a_power;
			volatile float *r_power_p = &r_power;
			volatile float *ap_power_p = &ap_power;
			volatile float *power_factor_p = &power_factor;
			volatile float *power_frequency_p = &power_frequency;

			print_ddsu666_params(hexArray, voltage_p, current_p, a_power_p, r_power_p,
								 ap_power_p, power_factor_p, power_frequency_p);
		}
		if (data_rec_len == 9)
		{
			volatile float *total_engery_p = &total_engery;
			print_ddsu666_total_energy(hexArray, total_engery_p);
		}
	}
	else
		ESP_LOGI(TAG, "no data rec");
}
/*
 *清空累计电量*/
static void clear_total_engery()
{
	is_clear_total_engery = 0;
	// send
	uart_send(ddsu666_clean_total_engery, sizeof(ddsu666_clean_total_engery)); // CHECK SOFTWARE VERSION
	vTaskDelay(300 / portTICK_PERIOD_MS);

	// receive
	uint8_t *data_rec_c;
	data_rec_c = (uint8_t *)malloc(sizeof(uint8_t) * 32);
	int rec_len_c = uart_rec(data_rec_c);
	vTaskDelay(1000 / portTICK_PERIOD_MS);

	uint8_t byte_i;
	// Write data back to UART
	if (rec_len_c > 0)
	{
		ESP_LOGI(TAG, "clear clear_total_engery received %u bytes:", rec_len_c);
		for (int i = 0; i < rec_len_c; i++)
		{
			byte_i = data_rec_c[i];
			printf("i: %d, int:%d, 0x%02X\r\n", i, data_rec_c[i], byte_i);
		}
		printf("end\r\n");
	}
	else
		ESP_LOGW(TAG, "clear clear_total_engery error");
}

/*循环函数*/
void uart_loop(void *arg)
{

	// Subscribe this task to TWDT, then check if it is subscribed
	ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
	ESP_ERROR_CHECK(esp_task_wdt_status(NULL));
	ESP_LOGI(dogTAG, "Subscribed to TWDT\n");

	ESP_LOGI(TAG, "UART send.\r");

	// 以下为了配合看门狗喂狗时间10秒限制进行更改.

	// 基准循环, 每秒+1;
	int inner_loop = 0;
	while (loop)
	{
		{
			// 喂狗
			esp_task_wdt_reset();
			// 清除电能累计数据
			if (is_clear_total_engery)
				clear_total_engery();
		}

		// inner_loop 每秒+1, 正好作为rec_wait的比较基准;
		if (inner_loop >= rec_wait - 1) // 串口有1秒等待时间, so - 1
		{
			++loop_count;	// 实际循环+1
			inner_loop = 0; // 基准循环置零

			// temp_max_loop次实际循环之后恢复常态stable_rec_wait秒循环
			if (loop_count == temp_max_loop)
			{
				ESP_LOGI(TAG, "restore defult %ds loop", stable_rec_wait);
				change_rec_wait(stable_rec_wait); // 把
			}

			// 记录运行时间戳
			uart_tx_rx_timestamp = (int)pdTICKS_TO_MS(xTaskGetTickCount()) / 1000;

			// 查询电压电流有用功率
			// send
			if (debug)
				printf("^^^^^^^^\r\n");
			uart_send(ddsu666_params, sizeof(ddsu666_params)); // CHECK SOFTWARE VERSION
			vTaskDelay(250 / portTICK_PERIOD_MS);

			// receive
			int rec_len = uart_rec(data_rec_p);
			vTaskDelay(250 / portTICK_PERIOD_MS);
			get_rec_data(data_rec_p, rec_len);
			// vTaskDelay(500 / portTICK_PERIOD_MS);

			// 查询累计电量
			// send
			uart_send(ddsu666_Ep, sizeof(ddsu666_Ep));
			vTaskDelay(250 / portTICK_PERIOD_MS);
			// receive
			rec_len = uart_rec(data_rec_p);
			vTaskDelay(250 / portTICK_PERIOD_MS);
			get_rec_data(data_rec_p, rec_len);
			if (debug)
				printf("^^^^^^^^\r\n");
		}
		else
			++inner_loop;

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
	// 停止循环删除任务
	if (!loop)
	{
		// Unsubscribe this task
		ESP_ERROR_CHECK(esp_task_wdt_delete(NULL));
		ESP_LOGI(dogTAG, "Unsubscribed from TWDT\n");
		// If we manually initialized the TWDT, deintialize it now
		ESP_ERROR_CHECK(esp_task_wdt_deinit());
		ESP_LOGI(dogTAG, "TWDT deinitialized\n");
		vTaskDelete(NULL);
	}
}

/*
 *删除串口循环任务*/
void remove_rec_task()
{
	loop = 0;
}

/*
 *更改循环等待时间*/
void change_rec_wait(int sec)
{

	loop_count = 0;
	rec_wait = sec;
}
/*
 *清除累计电量*/
void set_is_clear_total_engery()
{
	is_clear_total_engery = 1;
}

int get_loop_count()
{
	return loop_count;
}

int get_rec_wait()
{
	return rec_wait;
}

int get_uart_tx_rx_timestamp()
{
	return uart_tx_rx_timestamp;
}
