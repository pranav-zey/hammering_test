#include <stdio.h>
#include "audioprocessing.h"
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "m5mic.h"
#include "esp_log.h"
#include "mfcc_op.h"
#include "fft_op.h"
#include "svm_model.h"
#include <iostream>
#include "m5sdcard.h"
#include "m5display.h"

#define TRANSIENT_ENERGY_THERSHOLD 10000

fft_function_t impact_fft_operator;
fft_function_t vibration_fft_operator;

wav_data_t *wave_data;

wav_data_t impact_signal;
wav_data_t vibration_signal;

audio_features_t impact_features;
audio_features_t vibration_features;

SemaphoreHandle_t hammer_edge_detect_smphr = NULL;

SemaphoreHandle_t audio_store_wait_smphr = NULL;

void detect_hammer_edge()
{
    if (xSemaphoreTake(hammer_edge_detect_smphr, portMAX_DELAY))
    {

        size_t start = (wave_data->length + wave_data->latest_index - WAVE_BLOCK_SIZE) % wave_data->length;
        int lower_bound = abs(wave_data->wav[start]);
        int upper_bound = abs(wave_data->wav[start]);
        int lower_bound_index = start;
        bool transient_detected = false;
        for (size_t i = 1; i < WAVE_BLOCK_SIZE; i++)
        {
            int value = abs(wave_data->wav[(start + i) % wave_data->length]);
            if (value < upper_bound)
            {
                lower_bound = value;
                lower_bound_index = (start + i) % wave_data->length;
                upper_bound = value;
            }
            else
            {
                upper_bound = value;
            }
            int lower_energy = lower_bound * lower_bound;
            int upper_energy = upper_bound * upper_bound;
            int energy_difference = upper_bound - lower_bound;
            if (energy_difference > TRANSIENT_ENERGY_THERSHOLD)
            {
                ESP_LOGI("TRANSIENT", " Transient detected:%d, lower bound: %d, upper bound: %d", energy_difference, lower_energy, upper_energy);
                // Reset the impact signal and vibration signal to read the new samples.
                impact_signal.latest_index = 0;
                vibration_signal.latest_index = 0;
                transient_detected = true;
                break;
            }
        }

        if (!transient_detected)
        {
            return;
        }

        ESP_LOGI("TRANSIENT", " Transient processing");
        int vibration_start_index = 0;
        bool complete_samples = false;
        while (impact_signal.latest_index < impact_signal.length)
        {
            int start_index = lower_bound_index;
            if (impact_signal.latest_index != 0) // if buffer is filling for the first time.
            {
                xSemaphoreTake(hammer_edge_detect_smphr, portMAX_DELAY); // wait for new sample.
                start_index = wave_data->latest_index - WAVE_BLOCK_SIZE; // if reading for second time then the start index should be start of new block
            }
            if (wave_data->latest_index > start_index) // there data in no wrapped around the buffer.
            {
                for (int i = start_index; i < wave_data->latest_index; i++)
                {
                    impact_signal.wav[impact_signal.latest_index] = wave_data->wav[i]; // copy the data
                    if (impact_signal.max_value < abs(wave_data->wav[i]))
                    {
                        impact_signal.max_value = wave_data->wav[i];
                    }
                    impact_signal.latest_index++;
                    // if the impact_signal length is staisfied: exit
                    if (impact_signal.latest_index == impact_signal.length) // buffer is completely filled.
                    {
                        vibration_start_index = (i + 1) % wave_data->length;
                        complete_samples = true;
                        break;
                    }
                }
            }
            else // data is wrapped around the buffer.
            {
                // samples from the point of start to end of the audio buffer
                for (int i = start_index; i < wave_data->length; i++)
                {
                    impact_signal.wav[impact_signal.latest_index] = wave_data->wav[i];
                    if (impact_signal.max_value < abs(wave_data->wav[i]))
                    {
                        impact_signal.max_value = wave_data->wav[i];
                    }
                    impact_signal.latest_index++;
                    // if the impact_signal length is staisfied: exit
                    if (impact_signal.latest_index == impact_signal.length)
                    {
                        vibration_start_index = (i + 1) % wave_data->length;
                        complete_samples = true;
                        break;
                    }
                }
                // samples from start of the buffer to end of new index.
                for (int i = 0; i < wave_data->latest_index; i++)
                {
                    impact_signal.wav[impact_signal.latest_index] = wave_data->wav[i];
                    if (impact_signal.max_value < abs(wave_data->wav[i]))
                    {
                        impact_signal.max_value = wave_data->wav[i];
                    }
                    impact_signal.latest_index++;
                    // if the impact_signal length is staisfied: exit
                    if (impact_signal.latest_index == impact_signal.length)
                    {
                        vibration_start_index = (i + 1) % wave_data->length;
                        complete_samples = true;
                        break;
                    }
                }
            }

            if (complete_samples)
            {
                ESP_LOGI("IMPACT SAMPLES", "Number of samples:%d", impact_signal.latest_index);
                break;
            }
        }

        complete_samples = false;
        while (vibration_signal.latest_index < vibration_signal.length)
        {
            int start_index = vibration_start_index;
            if (vibration_signal.latest_index != 0)
            {
                xSemaphoreTake(hammer_edge_detect_smphr, portMAX_DELAY);
                start_index = wave_data->latest_index - WAVE_BLOCK_SIZE;
            }

            if (wave_data->latest_index > start_index) // there data in no wrapped around the buffer.
            {
                for (int i = start_index; i < wave_data->latest_index; i++)
                {
                    vibration_signal.wav[vibration_signal.latest_index] = wave_data->wav[i]; // copy the data
                    if (vibration_signal.max_value < abs(wave_data->wav[i]))
                    {
                        vibration_signal.max_value = wave_data->wav[i];
                    }
                    vibration_signal.latest_index++;
                    // if the impact_signal length is staisfied: exit
                    if (vibration_signal.latest_index == vibration_signal.length) // buffer is completely filled.
                    {
                        complete_samples = true;
                        break;
                    }
                }
            }
            else // data is wrapped around the buffer.
            {
                // samples from the point of start to end of the audio buffer
                for (int i = start_index; i < wave_data->length; i++)
                {
                    vibration_signal.wav[vibration_signal.latest_index] = wave_data->wav[i];
                    if (vibration_signal.max_value < abs(wave_data->wav[i]))
                    {
                        vibration_signal.max_value = wave_data->wav[i];
                    }
                    vibration_signal.latest_index++;
                    // if the impact_signal length is staisfied: exit
                    if (vibration_signal.latest_index == vibration_signal.length)
                    {
                        complete_samples = true;
                        break;
                    }
                }
                // samples from start of the buffer to end of new index.
                for (int i = 0; i < wave_data->latest_index; i++)
                {
                    vibration_signal.wav[vibration_signal.latest_index] = wave_data->wav[i];
                    if (vibration_signal.max_value < abs(wave_data->wav[i]))
                    {
                        vibration_signal.max_value = wave_data->wav[i];
                    }
                    vibration_signal.latest_index++;
                    // if the impact_signal length is staisfied: exit
                    if (vibration_signal.latest_index == vibration_signal.length)
                    {
                        complete_samples = true;
                        break;
                    }
                }
            }
            if (complete_samples)
            {
                ESP_LOGI("VIBRATION SAMPLES", "Number of samples:%d", vibration_signal.latest_index);
                break;
            }
        }
        give_write_audio_samples_smphr();
        // impact signal analysis
        // caclulate FFT for all the samples
        impact_fft_operator.calculate_fft(&impact_signal);
        impact_features.dom_freq = impact_fft_operator.get_dom_freq();
        impact_fft_operator.get_fft_coeff(impact_features.fft_coeff);
        // calculate MFCC for all the samples
        mfcc_calc_function(&impact_signal, true);
        get_mfcc_value(impact_features.mfcc_values, true);

        // vibration signal analysis
        // caclulate FFT for all the samples
        vibration_fft_operator.calculate_fft(&vibration_signal);
        vibration_features.dom_freq = vibration_fft_operator.get_dom_freq();
        vibration_fft_operator.get_fft_coeff(vibration_features.fft_coeff);
        // calculate MFCC for all the samples
        mfcc_calc_function(&vibration_signal, false);
        get_mfcc_value(vibration_features.mfcc_values, false);

        give_write_audio_features_smphr();

        svm_input_struct_t inference_input;
        inference_input.impact_dom_freq = impact_features.dom_freq;
        inference_input.impact_fft_coeff = impact_features.fft_coeff;
        inference_input.vibration_dom_freq = vibration_features.dom_freq;
        inference_input.vibration_fft_coeff = vibration_features.fft_coeff;
        inference_input.impact_mfcc_values = impact_features.mfcc_values;
        inference_input.vibration_mfcc_values = vibration_features.mfcc_values;

        // ML model inference
        int output = svm_predict(&inference_input);
        ESP_LOGI("Model", "output:%d", output);

        int32_t fileindex = get_file_index();
        display_model_output(output, fileindex);
        display_signal_info(impact_features.dom_freq, vibration_features.dom_freq);
        display_wav(&impact_signal, &vibration_signal);
        xSemaphoreTake(audio_store_wait_smphr, portMAX_DELAY);
    }
}

void audio_processing_init()
{
    wave_data = get_wav_data();

    while (hammer_edge_detect_smphr == NULL)
    {
        hammer_edge_detect_smphr = xSemaphoreCreateBinary();
    }
    while (audio_store_wait_smphr == NULL)
    {
        audio_store_wait_smphr = xSemaphoreCreateBinary();
    }

    impact_signal.length = SAMPLE_RATE * IMPACT_DURATION / 1000;
    impact_signal.wav = (typeof(impact_signal.wav))heap_caps_malloc(impact_signal.length * sizeof(impact_signal.wav[0]), MALLOC_CAP_8BIT);
    memset(impact_signal.wav, 0, impact_signal.length * sizeof(int16_t));

    vibration_signal.length = SAMPLE_RATE * VIBRATION_DURATION / 1000;
    vibration_signal.wav = (typeof(vibration_signal.wav))heap_caps_malloc(vibration_signal.length * sizeof(vibration_signal.wav[0]), MALLOC_CAP_8BIT);
    memset(vibration_signal.wav, 0, vibration_signal.length * sizeof(int16_t));

    mfcc_init();

    // fft_init();
    impact_fft_operator.init(1024);
    vibration_fft_operator.init(4096);
}

void edge_detection_task(void *vp_args)
{
    while (true)
    {
        detect_hammer_edge();
    }
}

void test_sdcard_samples(void *vp_args)
{
    char *impact_filename = MOUNT_POINT "/test_samples/impact_sample_70.wav";
    char *vibration_filename = MOUNT_POINT "/test_samples/vibration_sample_70.wav";
    wav_data_t impact_sample;
    wav_data_t vibration_sample;
    while (true)
    {
        get_audio_sample(impact_filename, &impact_sample);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
void give_hammer_detection_semaphore()
{
    xSemaphoreGive(hammer_edge_detect_smphr);
}
void give_store_wait_semaphore()
{
    xSemaphoreGive(audio_store_wait_smphr);
}

wav_data_t *get_impact_samples()
{
    return &impact_signal;
}

wav_data_t *get_vibration_samples()
{
    return &vibration_signal;
}

audio_features_t *get_impact_features()
{
    return &impact_features;
}
audio_features_t *get_vibration_features()
{
    return &vibration_features;
}