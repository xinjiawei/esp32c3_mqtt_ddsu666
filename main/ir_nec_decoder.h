#pragma once
#include "driver/rmt_rx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

void example_parse_nec_frame(rmt_symbol_word_t *rmt_nec_symbols, size_t symbol_num);

bool example_rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data);
