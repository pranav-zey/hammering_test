#ifndef __AUDIOPROCESSING_H__
#define __AUDIOPROCESSING_H__
#include "m5mic.h"

#define IMPACT_DURATION 20     // ms
#define VIBRATION_DURATION 100 // ms

void audio_processing_init();
void edge_detection_task(void *vp_args);
void give_hammer_detection_semaphore();
void give_store_wait_semaphore();
wav_data_t *get_impact_samples();
wav_data_t *get_vibration_samples();

#endif