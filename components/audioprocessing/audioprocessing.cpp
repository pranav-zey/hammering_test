#include <stdio.h>
#include "audioprocessing.h"
#include <esp_err.h>
#include <esp_dsp.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <dl_mfcc.hpp>

fft_function_t fft_function;
wav_data_t *wave_data;
fft_data_t fft_data;
static float mfcc_out[MFCC_NUM_CEPS];
static int16_t prev_sample = 0;
dl::audio::MFCC *mfcc_op = nullptr;

SemaphoreHandle_t fft_processing_smphr = NULL;
SemaphoreHandle_t mfcc_processing_smphr = NULL;

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

bool mfcc_calc_function()
{
    dl::audio::SpeechFeatureConfig mfcc_op_config = mfcc_op->config();
    int frame_len = mfcc_op_config.frame_length * SAMPLE_RATE / 1000;

    int16_t mfcc_frame[frame_len];
    size_t start = (wave_data->latest_index - frame_len + wave_data->length) % wave_data->length;

    for (int i = 0; i < frame_len; i++)
    {
        mfcc_frame[i] = wave_data->wav[(start + i) % wave_data->length];
    }
    // ESP_LOGI("AUDIO", "sample[0]=%d sample[1]=%d", mfcc_frame[0], mfcc_frame[1]);
    mfcc_op->process_frame(mfcc_frame, frame_len, mfcc_out, prev_sample);
    prev_sample = mfcc_frame[frame_len - 1];
    char mfcc_value[150] = {0};
    char tmp[32];
    for (int i = 0; i < MFCC_NUM_CEPS; i++)
    {
        snprintf(tmp, sizeof(tmp), " %.3f", mfcc_out[i]);
        strncat(mfcc_value, tmp, sizeof(mfcc_value) - strlen(mfcc_value) - 1);
    }
    ESP_LOGI("AUDIO Processing", " MFCC Size:%d MFCC Values:%s", sizeof(mfcc_out) / sizeof(mfcc_out[0]), mfcc_value);
    // ESP_LOGI("AUDIO Processing", "%s", mfcc_value);
    return true;
}

void audio_processing_init()
{

    wave_data = get_wav_data();
    // FFT initialization
    fft_function.setup(FFT_BITS);

    fft_data.fft_size_bits = FFT_BITS;
    fft_data.sample_rate = SAMPLE_RATE;
    fft_data.wav_data = wave_data;
    fft_data.length = (1 << (fft_data.fft_size_bits - 1)) + 1;
    fft_data.fdata = (typeof(fft_data.fdata))heap_caps_malloc(WAVE_TOTAL_SIZE * sizeof(fft_data.fdata[0]), MALLOC_CAP_8BIT);
    while (fft_processing_smphr == NULL)
    {
        fft_processing_smphr = xSemaphoreCreateBinary();
    }
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
    mfcc_op->print_config();

    while (mfcc_processing_smphr == NULL)
    {
        mfcc_processing_smphr = xSemaphoreCreateBinary();
    }
}

void fft_processing()
{
    if (xSemaphoreTake(fft_processing_smphr, portMAX_DELAY))
    {
        // ESP_LOGI("AUDIO_PROCESSING", "Execution FFT");
        fft_function.update(&fft_data);
    }
}

void mfcc_processing()
{
    if (xSemaphoreTake(mfcc_processing_smphr, portMAX_DELAY))
    {
        // ESP_LOGI("AUDIO_PROCESSING", "Executing mfcc");
        mfcc_calc_function();
    }
}

void give_fft_processing_semaphore()
{
    xSemaphoreGive(fft_processing_smphr);
}
void give_mfcc_processing_semaphore()
{
    xSemaphoreGive(mfcc_processing_smphr);
}

fft_data_t *get_fft_data()
{
    return &fft_data;
}

float *get_mfcc_data()
{
    return mfcc_out;
}