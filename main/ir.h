#pragma once
#ifndef IRBABY_IR_H_
#define IRBABY_IR_H_
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "ir_nec_encoder.h"
#include "ir_nec_decoder.h"

void nec_tx_init();
void nec_tx(ir_nec_scan_code_t scan_code);

void nec_rx_init();
void nec_rx(const int timewait);
int ir_transmission();
#endif // IRBABY_IR_H_