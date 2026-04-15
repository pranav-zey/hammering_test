#ifndef __AUDIOPROCESSING_H__
#define __AUDIOPROCESSING_H__
#include "m5mic.h"
#include "mfcc_op.h"

#define IMPACT_DURATION 20     // ms
#define VIBRATION_DURATION 100 // ms

struct audio_features_t
{
    float dom_freq;
    float fft_coeff[10];
    float mfcc_values[MFCC_NUM_CEPS];
};

void audio_processing_init();
void edge_detection_task(void *vp_args);
void test_sdcard_samples(void *vp_args);
void give_hammer_detection_semaphore();
void give_store_wait_semaphore();
wav_data_t *get_impact_samples();
wav_data_t *get_vibration_samples();
audio_features_t *get_impact_features();
audio_features_t *get_vibration_features();

#endif