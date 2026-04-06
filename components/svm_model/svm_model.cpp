#include <stdio.h>
#include "svm_model.h"
#include <iostream>
#include "audioprocessing.h"
#include "svm_pytorch_model_exported.h"

int32_t svm_inference(const int8_t *x)
{
    int32_t acc = BIAS;

    for (int i = 0; i < INPUT_SIZE; i++)
    {
        acc += WEIGHTS[i] * x[i];
    }

    return acc;
}

int predict(input_struct_t *input)
{
    // create a single array with the input structure.
    float x[INPUT_SIZE];

    // input order = [impact_dom_freq, impact_mfcc, impact fft, vib_dom_freq, vib_mfcc, vib_mfcc]
    int start_idx = 0;
    x[start_idx] = input->impact_dom_freq;
    start_idx += 1;
    std::copy(input->impact_mfcc_values, input->impact_mfcc_values + MFCC_NUM_CEPS, x + start_idx);
    start_idx += MFCC_NUM_CEPS;
    std::copy(input->impact_fft_coeff, input->impact_fft_coeff + 10, x + start_idx);
    start_idx += 10;
    x[start_idx] = input->vibration_dom_freq;
    start_idx += 1;
    std::copy(input->vibration_mfcc_values, input->vibration_mfcc_values + MFCC_NUM_CEPS, x + start_idx);
    start_idx += MFCC_NUM_CEPS;
    std::copy(input->vibration_fft_coeff, input->vibration_fft_coeff + 10, x + start_idx);
    start_idx += 10;

    // quantize the input
    int8_t x_quant[INPUT_SIZE];
    for (int i = 0; i < INPUT_SIZE; i++)
    {
        x_quant[i] = (int8_t)roundf(x[i] / X_SCALE);
    }

    int32_t acc = svm_inference(x_quant);
    float y = acc * W_SCALE * X_SCALE;
    if (y > 0)
        return 1;
    else
        return 0;
}
