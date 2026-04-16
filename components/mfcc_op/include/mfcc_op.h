#ifndef __MFCC_OP_H__
#define __MFCC_OP_H__

#include "m5mic.h"

#define MFCC_MEL_BINS 40
#define MFCC_NUM_CEPS 13

void mfcc_init(void);
bool mfcc_calc_function(wav_data_t *sound_data, bool is_impact);
void get_mfcc_value(float *mfcc_value, bool is_impact);

#endif
