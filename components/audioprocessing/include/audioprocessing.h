#ifndef __AUDIOPROCESSING_H__
#define __AUDIOPROCESSING_H__

#define IMPACT_DURATION 20 // ms
#define VIBRATION_DURATION 100 // ms

void audio_processing_init();
void edge_detection_task(void *vp_args);
void give_hammer_detection_semaphore();

#endif