#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "m5stack.h"
#include "M5GFX.h"
#include "m5display.h"
#include "m5mic.h"
#include "audioprocessing.h"

static wav_data_t wav_data;

void audio_loop_task(void *vp_args)
{
    while (true)
    {
        get_audio();
        vTaskDelay(pdMS_TO_TICKS(15));
    }
    vTaskDelete(NULL);
}
void fft_loop_task(void *vp_args)
{
    while (true)
    {
        // ESP_LOGI("FFT_LOOP","FFT LOOP");
        audio_processing();
    }
}
void display_loop_task(void *vp_args)
{
    while (true)
    {
        // ESP_LOGI("DISPLAY LOOP","Just got here");
        display_update();
        vTaskDelay(pdMS_TO_TICKS(35)); 
    }
}

void m5_device_init()
{
    auto cfg = M5.config();
#if defined(__M5GFX_M5MODULEDISPLAY__)
    cfg.module_display.logical_width = 320;
    cfg.module_display.logical_height = 180;
#endif
#if defined(__M5GFX_M5ATOMDISPLAY__)
    cfg.atom_display.logical_width = 320;
    cfg.atom_display.logical_height = 180;
#endif

    M5.begin(cfg);

    display_init();

    audio_init();

    audio_processing_init();

    xTaskCreatePinnedToCore(audio_loop_task, "AUDIO_LOOP", 8192, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(fft_loop_task, "FFT_LOOP", 8192, NULL, 7, NULL, 1);
    xTaskCreatePinnedToCore(display_loop_task, "DISPLAY_LOOP", 4096, NULL, 5, NULL, 1);
}
