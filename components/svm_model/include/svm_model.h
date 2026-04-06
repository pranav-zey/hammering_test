#ifndef __SVM_MODEL_H__
#define __SVM_MODEL_H__

struct input_struct_t
{
    float impact_dom_freq;
    float *impact_mfcc_values;
    float *impact_fft_coeff;
    float vibration_dom_freq;
    float *vibration_mfcc_values;
    float *vibration_fft_coeff;
};

int predict(input_struct_t *input);

#endif
