#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "nvs_flash.h"
#include "m5stack.h"
#include "M5Unified.h"
#include "M5GFX.h"
#include "m5display.h"
#include "m5mic.h"
#include "audioprocessing.h"
#include "m5sdcard.h"

// Macro to use either the mic samples or the test samples from sd card.
#define SDCARD_DATA_TEST

void m5_device_init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

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
    sd_card_init();
    audio_processing_init();

#ifndef SDCARD_DATA_TEST
    audio_init();

    xTaskCreatePinnedToCore(audio_loop_task, "AUDIO_LOOP", 8192, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(edge_detection_task, "TRANSIENT_DETECTION", 16384, NULL, 7, NULL, 1);
    xTaskCreatePinnedToCore(write_audio_task, "SDCARD_LOOP", 8192, NULL, 6, NULL, 0);
#else
    xTaskCreatePinnedToCore(test_sdcard_samples, "TEST_SAMPLES", 16384, NULL, 7, NULL, 1);
#endif
}
