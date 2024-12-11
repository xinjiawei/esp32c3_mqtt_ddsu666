#include "ir.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define EXAMPLE_IR_TX_GPIO_NUM 5
#define EXAMPLE_IR_RX_GPIO_NUM 6
#define EXAMPLE_IR_RESOLUTION_HZ 1000000 // 1MHz resolution, 1 tick = 1us


static const char *TAG = "IR";

rmt_channel_handle_t tx_channel = NULL;
rmt_encoder_handle_t nec_encoder = NULL;

rmt_channel_handle_t rx_channel = NULL;
QueueHandle_t receive_queue = NULL;

void nec_rx_init()
{
	ESP_LOGI(TAG, "create RMT RX channel");
	rmt_rx_channel_config_t rx_channel_cfg = {
		.clk_src = RMT_CLK_SRC_DEFAULT,
		.resolution_hz = EXAMPLE_IR_RESOLUTION_HZ,
		.mem_block_symbols = 64, // amount of RMT symbols that the channel can store at a time
		.gpio_num = EXAMPLE_IR_RX_GPIO_NUM,
	};
	ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_channel_cfg, &rx_channel));

	ESP_LOGI(TAG, "register RX done callback");
	receive_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
	assert(receive_queue);
	rmt_rx_event_callbacks_t cbs = {
		.on_recv_done = example_rmt_rx_done_callback,
	};
	ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, receive_queue));

	ESP_LOGI(TAG, "enable RMT RX channels");
	ESP_ERROR_CHECK(rmt_enable(rx_channel));
}

void nec_tx_init() {
	ESP_LOGI(TAG, "create RMT TX channel");
	rmt_tx_channel_config_t tx_channel_cfg = {
		.clk_src = RMT_CLK_SRC_DEFAULT,
		.resolution_hz = EXAMPLE_IR_RESOLUTION_HZ,
		.mem_block_symbols = 64, // amount of RMT symbols that the channel can store at a time
		.trans_queue_depth = 4,	 // number of transactions that allowed to pending in the background, this example won't queue multiple transactions, so queue depth > 1 is sufficient
		.gpio_num = EXAMPLE_IR_TX_GPIO_NUM,
	};
	ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_channel_cfg, &tx_channel));

	ESP_LOGI(TAG, "modulate carrier to TX channel");
	rmt_carrier_config_t carrier_cfg = {
		.duty_cycle = 0.33,
		.frequency_hz = 38000, // 38KHz
	};
	ESP_ERROR_CHECK(rmt_apply_carrier(tx_channel, &carrier_cfg));
	
	ESP_LOGI(TAG, "install IR NEC encoder");
	ir_nec_encoder_config_t nec_encoder_cfg = {
		.resolution = EXAMPLE_IR_RESOLUTION_HZ,
	};
	
	ESP_ERROR_CHECK(rmt_new_ir_nec_encoder(&nec_encoder_cfg, &nec_encoder));

	ESP_LOGI(TAG, "enable RMT TX channels");
	ESP_ERROR_CHECK(rmt_enable(tx_channel));
}

void nec_rx(const int timewait) {
	// the following timing requirement is based on NEC protocol
	rmt_receive_config_t receive_config = {
		.signal_range_min_ns = 1250,	 // the shortest duration for NEC signal is 560us, 1250ns < 560us, valid signal won't be treated as noise
		.signal_range_max_ns = 12000000, // the longest duration for NEC signal is 9000us, 12000000ns > 9000us, the receive won't stop early
	};
	// save the received RMT symbols
	rmt_symbol_word_t raw_symbols[64]; // 64 symbols should be sufficient for a standard NEC frame
	rmt_rx_done_event_data_t rx_data;
	// ready to receive
	ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));
	// wait for RX done signal
	int loop = 5;
	for (size_t i = 0; i < loop; ++i)
	{
		if (xQueueReceive(receive_queue, &rx_data, pdMS_TO_TICKS(timewait)) == pdPASS)
		{
			// parse the receive symbols and print the result
			example_parse_nec_frame(rx_data.received_symbols, rx_data.num_symbols);
			// start receive again
			if (i != loop - 1)
			{
				ESP_LOGI(TAG, "loop%d, IR receive", i);
				ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));
			}
			else ESP_LOGI(TAG, "final loop, IR not receive");
		}
	}
}

void nec_tx(ir_nec_scan_code_t scan_code) {
	// this example won't send NEC frames in a loop
	rmt_transmit_config_t transmit_config = {
		.loop_count = 0, // no loop
	};
	// timeout, transmit predefined IR NEC packets
	ESP_ERROR_CHECK(rmt_transmit(tx_channel, nec_encoder, &scan_code, sizeof(scan_code), &transmit_config));
}

int ir_transmission() {
	ESP_LOGI(TAG, "ir_transmission command");
	return 0;
}