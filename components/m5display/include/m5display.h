#ifndef _M5DISPLAY_H__
#define _M5DISPLAY_H__
#include "stdint.h"
#include "m5mic.h"

void display_init();
void display_model_output(int output, int32_t fileindex);
void display_signal_info(float impact_freq, float vibration_freq);
void display_clear_result();
void display_wav(wav_data_t *impact, wav_data_t *vibration);

#endif