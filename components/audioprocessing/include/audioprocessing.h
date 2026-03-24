#ifndef __AUDIOPROCESSING_H__
#define __AUDIOPROCESSING_H__

#include "m5mic.h"

// The larger the FFT_BITS, the higher the accuracy of the FFT, but the processing load also increases
#define FFT_BITS 10
#define FFT_SIZE (1u << FFT_BITS)

#define MFCC_MEL_BINS 40
#define MFCC_NUM_CEPS 13

#define TRANSIENT_ENERGY_THERSHOLD 1000
struct fft_data_t
{
    float *fdata = nullptr;
    float dominant_frequency = 0;
    size_t length = 0;
    size_t sample_rate;

    struct wav_data_t *wav_data = nullptr;
    uint8_t fft_size_bits = 0;

    float get_data_by_pixel(uint16_t x, uint16_t width)
    {
        if (length <= width)
        {
            int index = x * length / width;
            if (index >= length)
            {
                index = length - 1;
            }
            return fdata[index];
        }
        int index0 = x * length / width;
        int index1 = (x + 1) * length / width;
        if (index0 >= length)
        {
            index0 = length - 1;
        }
        if (index1 >= length)
        {
            index1 = length - 1;
        }
        float value = 0;
        for (int i = index0; i < index1; ++i)
        {
            if (value < fdata[i])
            {
                value = fdata[i];
            }
        }
        return value;
    }
};

class fft_function_t
{
    float *_window = nullptr;
    float *_input_buffer = nullptr;
    int _current_fft_size = 0;

public:
    bool setup(uint8_t max_fft_size_bits);
    bool update(fft_data_t *fft_data);
};

void audio_processing_init(void);
void fft_processing();
void mfcc_processing();
void detect_hammer_edge();
void give_hammer_detection_semaphore();
void give_fft_processing_semaphore();
void give_mfcc_processing_semaphore();
fft_data_t *get_fft_data();
float *get_mfcc_data();
#endif