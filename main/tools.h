#pragma once
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
char *get_len_str(char *original, int len);
char * get_chip_id();

void print_sys_info();
unsigned int crc_cal(const char *pBuff, int len);

float float_from_8hex(int arr[]);
void print_ddsu666_params(uint8_t bytes[], float *voltage, float *current, float *power);
void print_ddsu666_total_energy(uint8_t bytes[], float *total_energy);
void debug_switch();