#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "m5stack.h"
#include "M5Unified.h"
#include "M5GFX.h"
// #include "m5display.h"
#include "m5mic.h"
#include "audioprocessing.h"
#include "display_p.h"

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
    xTaskCreatePinnedToCore(edge_detection_task, "FFT_LOOP", 16384, NULL, 7, NULL, 1);
    // xTaskCreatePinnedToCore(display_loop_task, "DISPLAY_LOOP", 4096, NULL, 5, NULL, 1);
}
