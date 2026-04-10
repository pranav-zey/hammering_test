#ifndef __M5SDCARD_H__
#define __M5SDCARD_H__
#include "esp_err.h"

esp_err_t sd_card_init();

void write_audio_task(void *vp_args);
void give_write_audio_samples_smphr();
void give_write_audio_features_smphr();

#endif