#pragma once
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sdkconfig.h"

void uart_init();
void uart_loop(void *arg);
void remove_rec_task();
void change_rec_wait(int sec);
void set_is_clear_total_engery();
int get_loop_count();
int get_rec_wait();
int get_uart_tx_rx_timestamp();