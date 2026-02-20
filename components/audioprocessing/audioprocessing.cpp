#include <stdio.h>
#include "audioprocessing.h"
#include <esp_err.h>
#include <esp_dsp.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

fft_function_t fft_function;
fft_data_t fft_data;

SemaphoreHandle_t audio_processing_smphr = NULL;

bool fft_function_t::setup(uint8_t max_fft_size_bits)
{
    int fft_size = 1 << max_fft_size_bits;

    // 1. Initialize the DSP library tables
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK)
        return false;

    // 2. Allocate memory for windowing and processing
    // esp-dsp FFT works on complex numbers (Real, Imag, Real, Imag...)
    _input_buffer = (float *)heap_caps_malloc(fft_size * 2 * sizeof(float), MALLOC_CAP_8BIT);
    _window = (float *)heap_caps_malloc(fft_size * sizeof(float), MALLOC_CAP_8BIT);

    // 3. Pre-calculate a Hann window (improvement discussed earlier!)
    dsps_wind_hann_f32(_window, fft_size);

    _current_fft_size = fft_size;

    return (_input_buffer && _window);
}

__attribute__((optimize("O3"))) bool fft_function_t::update(fft_data_t *fft_data)
{
    int n = 1 << fft_data->fft_size_bits;
    static int16_t src[FFT_SIZE];
    size_t start = (fft_data->wav_data->latest_index + fft_data->wav_data->length - FFT_SIZE) % fft_data->wav_data->length;
    for (int i = 0; i < FFT_SIZE; i++)
    {
        src[i] = fft_data->wav_data->wav[(start + i) % fft_data->wav_data->length];
    }

    for (int i = 0; i < n; i++)
    {
        _input_buffer[i * 2] = (float)src[i] * _window[i];
        _input_buffer[i * 2 + 1] = 0;
    }

    // 2. Execute the FFT (Radix-2)
    dsps_fft2r_fc32(_input_buffer, n);

    // 3. Bit-reversal (Required by esp-dsp for 2r)
    dsps_bit_rev_fc32(_input_buffer, n);

    // 4. Calculate Magnitude
    // The output is mirrored; we only need the first half (0 to N/2)
    for (int i = 0; i < n / 2; i++)
    {
        float re = _input_buffer[i * 2];
        float im = _input_buffer[i * 2 + 1];
        // Calculate magnitude: sqrt(re^2 + im^2)
        fft_data->fdata[i] = sqrtf(re * re + im * im);
    }

    return true;
}

void audio_processing_init()
{
    fft_function.setup(FFT_BITS);

    fft_data.fft_size_bits = FFT_BITS;
    fft_data.sample_rate = SAMPLE_RATE;
    fft_data.wav_data = get_wav_data();
    fft_data.length = (1 << (fft_data.fft_size_bits - 1)) + 1;
    fft_data.fdata = (typeof(fft_data.fdata))heap_caps_malloc(WAVE_TOTAL_SIZE * sizeof(fft_data.fdata[0]), MALLOC_CAP_8BIT);
    while (audio_processing_smphr == NULL)
    {
        audio_processing_smphr = xSemaphoreCreateBinary();
    }
}

void audio_processing()
{
    if (xSemaphoreTake(audio_processing_smphr, portMAX_DELAY))
    {
        ESP_LOGI("AUDIO_PROCESSING","Execution FFT");
        fft_function.update(&fft_data);
    }
}

void give_audio_processing_semaphore(){
    xSemaphoreGive(audio_processing_smphr);
}

fft_data_t *get_fft_data()
{
    return &fft_data;
}
