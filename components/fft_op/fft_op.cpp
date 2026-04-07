#include <stdio.h>
#include <vector>
#include <algorithm>
#include "fft_op.h"
#include "esp_err.h"
#include "esp_dsp.h"
#include "esp_heap_caps.h"
#include "m5mic.h"
#include "freertos/FreeRTOS.h"

float *_window = nullptr;
float *_input_buffer = nullptr;
int _current_fft_size = 0;

float *fdata;
float dominant_frequency;
std::vector<float> fft_peaks;

bool fft_init()
{
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);

    if (ret != ESP_OK)
    {
        return false;
    }

    _input_buffer = (float *)heap_caps_malloc(FFT_SIZE * 2 * sizeof(float), MALLOC_CAP_8BIT);
    _window = (float *)heap_caps_malloc(FFT_SIZE * sizeof(float), MALLOC_CAP_8BIT);

    fdata = (typeof(fdata))heap_caps_malloc((FFT_SIZE / 2) * sizeof(fdata[0]), MALLOC_CAP_8BIT);

    fft_peaks.clear();
    // 3. Pre-calculate a Hann window (improvement discussed earlier!)
    dsps_wind_hann_f32(_window, FFT_SIZE);

    _current_fft_size = FFT_SIZE;

    return (_input_buffer && _window && fdata);
}

bool calculate_fft(wav_data_t *sound_data)
{
    fft_peaks.clear();
    for (int i = 0; i < FFT_SIZE; i++)
    {
        // for (int j = 0; j < 10; j++)
        // {
        //     printf("%d ", sound_data->wav[j]);
        // }
        if (i < sound_data->length)
        {
            _input_buffer[i * 2] = sinf(2 * M_PI * 2000 * i / SAMPLE_RATE) * _window[i];
            // _input_buffer[i * 2] = (float)(sound_data->wav[i]) * _window[i];
        }
        else
            _input_buffer[i * 2] = 0;
        // _input_buffer[i * 2] = sinf(2 * M_PI * 1000 * i / SAMPLE_RATE)*_window[i];
        _input_buffer[i * 2 + 1] = 0;
    }

    // 2. Execute the FFT (Radix-2)
    dsps_fft2r_fc32(_input_buffer, FFT_SIZE);

    // 3. Bit-reversal (Required by esp-dsp for 2r)
    dsps_bit_rev_fc32(_input_buffer, FFT_SIZE);

    // 4. Calculate Magnitude
    // The output is mirrored; we only need the first half (0 to N/2)
    float max_value = 0;
    float mag;
    for (int i = 0; i < FFT_SIZE / 2; i++)
    {
        float re = _input_buffer[i * 2];
        float im = _input_buffer[i * 2 + 1];
        // Calculate magnitude: sqrt(re^2 + im^2)
        mag = (2.0 / FFT_SIZE) * hypotf(re, im);
        fdata[i] = mag;

        if (mag > max_value)
        {
            max_value = mag;
        }
        if (fft_peaks.size() < 10)
        {
            fft_peaks.push_back(mag);
            if (fft_peaks.size() == 10)
            {
                std::sort(fft_peaks.begin(), fft_peaks.end(), std::greater<float>());
            }
        }
        else
        {
            // Case 2: only insert if better than smallest
            if (mag <= fft_peaks.back())
                continue;

            // Find position (descending order)
            auto pos = std::upper_bound(
                fft_peaks.begin(),
                fft_peaks.end(),
                mag,
                std::greater<float>());

            fft_peaks.insert(pos, mag);

            // Keep only top 10
            fft_peaks.pop_back();
        }
    }

    printf("fft_peaks:");
    for (auto i : fft_peaks)
    {
        printf("%f\t", i);
    }
    printf("\n");

    float threshold = 0.3 * max_value;
    float sum = 0;
    float weighted = 0;

    for (int i = 1; i < FFT_SIZE / 2; i++)
    {
        float mag = fdata[i];
        if (mag > threshold)
        {
            float freq = (float)i * SAMPLE_RATE / FFT_SIZE;
            sum += mag;
            weighted += freq * mag;
        }
    }
    if (sum > 0)
        dominant_frequency = weighted / sum;
    else
        dominant_frequency = 0;
    ESP_LOGI("FFT", "Dominant Frequency:%f", dominant_frequency);
    return true;
}

bool fft_function_t::init(int fft_size)
{

    esp_err_t ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK)
    {
        return false;
    }
    _current_fft_size = fft_size;

    _input_buffer = (float *)heap_caps_malloc(fft_size * 2 * sizeof(float), MALLOC_CAP_8BIT);
    _window = (float *)heap_caps_malloc(fft_size * sizeof(float), MALLOC_CAP_8BIT);

    fdata = (typeof(fdata))heap_caps_malloc((fft_size / 2) * sizeof(fdata[0]), MALLOC_CAP_8BIT);

    if (!_input_buffer || !_window || !fdata)
        return false;

    // Precompute Hann window
    dsps_wind_hann_f32(_window, _current_fft_size);

    // Reset peaks
    for (int i = 0; i < 10; i++)
        fft_peaks[i] = 0;
    num_peaks = 0;

    return true;
}

bool fft_function_t::calculate_fft(wav_data_t *sound_data)
{
    // fft_peaks = {0};
    uint count = 0;
    for (int i = 0; i < _current_fft_size; i++)
    {
        if (i < sound_data->length)
        {
            // _input_buffer[i * 2] = sinf(2 * M_PI * 1000 * i / SAMPLE_RATE);
            _input_buffer[i * 2] = (float)sound_data->wav[i];
            count++;
        }
        else
            _input_buffer[i * 2] = 0;
        _input_buffer[i * 2 + 1] = 0;
    }
    ESP_LOGI("FFT", "%d samples added to input buffer", count);

    // 2. Execute the FFT (Radix-2)
    dsps_fft2r_fc32(_input_buffer, _current_fft_size);

    // 3. Bit-reversal (Required by esp-dsp for 2r)
    dsps_bit_rev_fc32(_input_buffer, _current_fft_size);

    // 4. Calculate Magnitude
    // The output is mirrored; we only need the first half (0 to N/2)
    float max_value = 0;
    float mag;
    float re, im;
    num_peaks = 0;
    for (int i = 0; i < _current_fft_size / 2; i++)
    {
        re = _input_buffer[i * 2];
        im = _input_buffer[i * 2 + 1];
        // Calculate magnitude: sqrt(re^2 + im^2)
        mag = (2.0 / _current_fft_size) * sqrtf(re * re + im * im);
        fdata[i] = mag;

        if (mag > max_value)
        {
            max_value = mag;
        }
        if (num_peaks < 10)
        {
            fft_peaks[num_peaks++] = mag;
            // Bubble up
            for (int j = num_peaks - 1; j > 0; j--)
            {
                if (fft_peaks[j] > fft_peaks[j - 1])
                    std::swap(fft_peaks[j], fft_peaks[j - 1]);
                else
                    break;
            }
        }
        else if (mag > fft_peaks[num_peaks - 1])
        {
            fft_peaks[num_peaks - 1] = mag;
            for (int j = num_peaks - 1; j > 0; j--)
            {
                if (fft_peaks[j] > fft_peaks[j - 1])
                {
                    std::swap(fft_peaks[j], fft_peaks[j - 1]);
                }
                else
                {
                    break;
                }
            }
        }
    }
    // printf("Stack remaining: %d bytes\n", uxTaskGetStackHighWaterMark(NULL));

    // Print top peaks safely
    printf("fft_peaks:");
    for (int i = 0; i < num_peaks; i++)
        printf("%f\t", fft_peaks[i]);
    printf("\n");
    // free(peak_string);

    float threshold = 0.3 * max_value;
    float sum = 0;
    float weighted = 0;

    for (int i = 1; i < _current_fft_size / 2; i++)
    {
        float mag = fdata[i];
        if (mag > threshold)
        {
            float freq = (float)i * SAMPLE_RATE / _current_fft_size;
            sum += mag;
            weighted += freq * mag;
        }
    }
    dominant_frequency = (sum > 0) ? (weighted / sum) : 0.0f;
    ESP_LOGI("FFT", "Dominant Frequency:%f", dominant_frequency);
    // printf("Stack remaining: %d bytes\n", uxTaskGetStackHighWaterMark(NULL));

    return true;
}