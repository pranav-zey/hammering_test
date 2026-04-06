#ifndef __FFT_H__
#define __FFT_H__
#include "m5mic.h"

// #define FFT_BITS 10
#define FFT_BITS 13 // 13  =  8192 samples for 4300 = 100ms samples cover for vibration sound
#define FFT_SIZE (1u << FFT_BITS)

class fft_function_t
{
private:
    float *_window = nullptr;
    float *_input_buffer = nullptr;
    int _current_fft_size = 0;

    float *fdata;
    float dominant_frequency;

    float fft_peaks[10] = {0};
    int num_peaks = 0;

public:
    bool init(int fft_size);
    bool calculate_fft(wav_data_t *sound_data);
};

bool fft_init();
bool calculate_fft(wav_data_t *sound_data);
#endif