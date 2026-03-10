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

wav_data_t impact_signal;
int impact_duration = 20; // ms
wav_data_t vibration_signal;
int vibration_duration = 100; // ms

SemaphoreHandle_t hammer_edge_detect_smphr = NULL;
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
    // ESP_LOGI("AUDIO Processing", " MFCC Size:%d MFCC Values:%s", sizeof(mfcc_out) / sizeof(mfcc_out[0]), mfcc_value);
    // ESP_LOGI("AUDIO Processing", "%s", mfcc_value);
    return true;
}

void audio_processing_init()
{

    wave_data = get_wav_data();

    while (hammer_edge_detect_smphr == NULL)
    {
        hammer_edge_detect_smphr = xSemaphoreCreateBinary();
    }

    impact_signal.length = SAMPLE_RATE * impact_duration / 1000;
    impact_signal.wav = (typeof(impact_signal.wav))heap_caps_malloc(impact_signal.length * sizeof(impact_signal.wav[0]), MALLOC_CAP_8BIT);
    memset(impact_signal.wav, 0, impact_signal.length * sizeof(int16_t));

    vibration_signal.length = SAMPLE_RATE * vibration_duration / 1000;
    vibration_signal.wav = (typeof(vibration_signal.wav))heap_caps_malloc(vibration_signal.length * sizeof(vibration_signal.wav[0]), MALLOC_CAP_8BIT);
    memset(vibration_signal.wav, 0, vibration_signal.length * sizeof(int16_t));

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
                    impact_signal.latest_index++;
                    // if the impact_signal length is staisfied: exit
                    if (impact_signal.latest_index == impact_signal.length) // buffer is completely filled.
                    {
                        vibration_start_index = i + 1;
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
                    impact_signal.latest_index++;
                    // if the impact_signal length is staisfied: exit
                    if (impact_signal.latest_index == impact_signal.length)
                    {
                        vibration_start_index = i + 1;
                        complete_samples = true;
                        break;
                    }
                }
                // samples from start of the buffer to end of new index.
                for (int i = 0; i < wave_data->latest_index; i++)
                {
                    impact_signal.wav[impact_signal.latest_index] = wave_data->wav[i];
                    impact_signal.latest_index++;
                    // if the impact_signal length is staisfied: exit
                    if (impact_signal.latest_index == impact_signal.length)
                    {
                        vibration_start_index = i + 1;
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
            // get the required number of vibration signal samples.
        }
    }
}
void give_hammer_detection_semaphore()
{
    xSemaphoreGive(hammer_edge_detect_smphr);
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