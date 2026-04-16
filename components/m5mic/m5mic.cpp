#include <stdio.h>
#include "m5mic.h"
#include <stdio.h>
#include <esp_heap_caps.h>
#include "m5mic.h"
#include "M5Unified.h"
#include "esp_log.h"
#include "audioprocessing.h"

static wav_data_t wav_data;

uint8_t sample_count = 0;

void mic_init()
{
    auto cfg = M5.Mic.config();
    cfg.pin_data_in = 32;
    cfg.pin_ws = 33;
    cfg.use_adc = false;
    cfg.i2s_port = i2s_port_t::I2S_NUM_0;
    cfg.dma_buf_count = 3;
    cfg.dma_buf_len = WAVE_BLOCK_SIZE;
    cfg.over_sampling = 1;
    cfg.noise_filter_level = 0;
    cfg.sample_rate = SAMPLE_RATE;
    cfg.magnification = cfg.use_adc ? 16 : 1;

    // use for Unit PDM ( Port A )

    M5.Mic.config(cfg);

    if (!M5.Mic.isEnabled())
    {
        M5.Display.printf("microphone is not available.");
        //  M5_LOGE("microphone is not available.");
        esp_restart();
    }
    M5.Mic.begin();
}

void get_audio()
{
    if (M5.Mic.isEnabled())
    {
        int wav_idx = wav_data.latest_index;
        if (M5.Mic.isRecording() < 2)
        {
            // ESP_LOGI("MIC", "MIC recording");

            if (M5.Mic.record(&(wav_data.wav[wav_idx]), WAVE_BLOCK_SIZE, SAMPLE_RATE))
            {

                wav_idx += WAVE_BLOCK_SIZE;
                if (wav_idx >= WAVE_TOTAL_SIZE)
                {
                    wav_idx = 0; // fills in the old data.
                }
                wav_data.latest_index = wav_idx;
                if (sample_count >= 4)
                {
                    // transfer required semaphores.
                    give_hammer_detection_semaphore();

                    // give_fft_processing_semaphore();
                    // give_mfcc_processing_semaphore();
                }
                else
                {
                    sample_count++;
                }
            }
            else
            {
                ESP_LOGI("MIC", "Recording failed");
            }
        }
        else
        {
            ESP_LOGI("MIC", "MIC Queue full");
        }
    }
}

void audio_init()
{
    mic_init();
    wav_data.length = WAVE_TOTAL_SIZE;
    wav_data.wav = (typeof(wav_data.wav))heap_caps_malloc(WAVE_TOTAL_SIZE * sizeof(wav_data.wav[0]), MALLOC_CAP_8BIT);
    memset(wav_data.wav, 0, WAVE_TOTAL_SIZE * sizeof(int16_t));
}

void audio_loop_task(void *pv_args)
{
    while (true)
    {
        get_audio();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    vTaskDelete(NULL);
}

wav_data_t *get_wav_data()
{
    return &wav_data;
}