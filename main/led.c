#include "led.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stdio.h>

static const char *TAG = "led";

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO 8
#define CONFIG_BLINK_LED_GPIO 1

static void blink_led(const uint32_t s_led_state)
{
	/* Set the GPIO level according to the state (LOW or HIGH)*/
	gpio_set_level(BLINK_GPIO, s_led_state);
}

/*
 *led≥ı ºªØ*/
void led_configure(void)
{
	ESP_LOGI(TAG, "configured to blink GPIO LED!");
	gpio_reset_pin(BLINK_GPIO);
	/* Set the GPIO as a push/pull output */
	gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

/*
 *led…¡À∏*/
void led_blink(void)
{
	blink_led(0);
	vTaskDelay(100 / portTICK_PERIOD_MS);
	blink_led(1);
}