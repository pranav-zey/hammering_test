#ifndef __M5SDCARD_H__
#define __M5SDCARD_H__
#include "esp_err.h"
#include "m5mic.h"

#define MOUNT_POINT "/sdcard"
#define TEST_LOG_FILE "/inference_test.csv"
#define TEST_DIR "/test_samples"

esp_err_t sd_card_init();

void write_audio_task(void *vp_args);
void give_write_audio_samples_smphr();
void give_write_audio_features_smphr();
int32_t get_file_index();

esp_err_t get_audio_sample(char *filename, wav_data_t *sound_data);
esp_err_t write_output_log(char *impact_filename, char *vibration_filename, int output);

#endif