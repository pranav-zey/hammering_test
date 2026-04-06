#include <stdio.h>
#include "mfcc_op.h"
#include <dl_mfcc.hpp>
#include "m5mic.h"
#include "audioprocessing.h"

#ifndef FFT_BITS 
#define FFT_BITS 10
#define FFT_SIZE (1u << FFT_BITS)
#endif


dl::audio::MFCC *mfcc_op = nullptr;
dl::audio::MFCC *impact_mfcc_op = nullptr;
dl::audio::MFCC *vibration_mfcc_op = nullptr;

float impact_mfcc_out[MFCC_NUM_CEPS];
float vibration_mfcc_out[MFCC_NUM_CEPS];

void mfcc_init(void)
{
    // mfcc initialization
    dl::audio::SpeechFeatureConfig speech_features_t;
    speech_features_t.sample_rate = SAMPLE_RATE;
    speech_features_t.frame_length = (FFT_SIZE * 1000) / SAMPLE_RATE;       // ≈ 21 ms
    speech_features_t.frame_shift = (WAVE_BLOCK_SIZE * 1000) / SAMPLE_RATE; // ≈ 10 ms
    speech_features_t.num_mel_bins = MFCC_MEL_BINS;
    speech_features_t.num_ceps = MFCC_NUM_CEPS;
    speech_features_t.low_freq = 0.0f;
    speech_features_t.high_freq = SAMPLE_RATE * 0.5f;
    mfcc_op = new dl::audio::MFCC(speech_features_t);
    // mfcc_op->print_config();

    // mfcc initalization for impact duration processing
    speech_features_t.frame_length = IMPACT_DURATION;
    speech_features_t.frame_shift = IMPACT_DURATION;
    impact_mfcc_op = new dl::audio::MFCC(speech_features_t);

    // mfcc initalization for vibration duration processing
    speech_features_t.frame_length = VIBRATION_DURATION;
    speech_features_t.frame_shift = VIBRATION_DURATION;
    vibration_mfcc_op = new dl::audio::MFCC(speech_features_t);
}

bool mfcc_calc_function(wav_data_t *sound_data, bool is_impact)
{
    ESP_LOGI("MFCC", "Calculating MFCC");
    if (is_impact)
    {
        // impact
        impact_mfcc_op->process_frame(sound_data->wav, sound_data->length, impact_mfcc_out);
    }
    else
    {
        // vibration
        vibration_mfcc_op->process_frame(sound_data->wav, sound_data->length, vibration_mfcc_out);
    }
    return true;
}