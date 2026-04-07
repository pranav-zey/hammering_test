#ifndef __M5MIC_H__
#define __M5MIC_H__

#include <esp_mac.h>
#include <stdint.h>
#include <math.h>

// The higher the sample rate, the higher the frequency results obtained by FFT.
// If limited to the audible range, 24kHz to 48kHz is sufficient.
#define SAMPLE_RATE 48000

// WAVE_BLOCK_SIZE is the size of one processing when capturing data from I2S.
// If it is too large, the loop cycle will be slow and the frequency of drawing updates will decrease.
// If it is too small, recoding interruptions will occur.
// For example, if the sample rate is 96kHz and the block size is 384, data will be captured every 4ms.
// Therefore, the drawing process, FFT process, and other loop iterations must be completed within 4ms.
#define WAVE_BLOCK_SIZE 512

#ifndef FFT_BITS
#define WAVE_TOTAL_SIZE (WAVE_BLOCK_SIZE * 4)
#else
#define WAVE_BLOCK_COUNT (3 + (FFT_SIZE / WAVE_BLOCK_SIZE))
#define WAVE_TOTAL_SIZE (WAVE_BLOCK_SIZE * WAVE_BLOCK_COUNT)
#endif
struct wav_data_t
{
    int16_t *wav = nullptr;
    size_t length = 0;
    size_t latest_index = 0;
    int16_t max_value = 0;

    size_t searchEdge(size_t offset, size_t search_length) const
    {
        int mem_position = latest_index + offset;
        if (mem_position >= length)
        {
            mem_position -= length;
        }
        int mem_difference = 0;
        int position = -1;
        uint32_t counter[2] = {0, 0};
        bool prev_sign = false;

        for (size_t i = 0; i < search_length; ++i)
        {
            size_t idx = latest_index + i;
            if (idx >= length)
            {
                idx -= length;
            }
            int value = wav[idx];
            bool sign = value < 0;
            if (prev_sign != sign)
            {
                prev_sign = sign;
                if (sign)
                { // When changing from positive to negative
                    if (position >= 0)
                    {
                        int diff = counter[0] + counter[1];
                        if (mem_difference < diff)
                        {
                            mem_difference = diff;
                            mem_position = position;
                        }
                    }
                }
                counter[sign] = 0;
                if (i >= offset)
                {
                    int pidx = (idx ? idx : length) - 1;
                    int cv = abs(wav[idx]);
                    int pv = abs(wav[pidx]);
                    position = (cv < pv) ? idx : pidx;
                }
            }
            uint32_t v = value * value;
            counter[sign] += v >> 7;
        }
        mem_position -= offset;
        if (mem_position < 0)
        {
            mem_position += length;
        }
        return mem_position;
    }
};

void audio_init();
void audio_loop_task(void *pv_args);

wav_data_t *get_wav_data();

#endif