#include <stdio.h>
#include "m5sdcard.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "m5mic.h"
#include "audioprocessing.h"
#include "errno.h"

static const char *TAG = "sdcard";

#define FILE_INDEX_NVS_KEY "file_index_key"

nvs_handle_t file_index_handle;
int32_t file_index = 0;

SemaphoreHandle_t write_audio_samples_smphr = NULL;
SemaphoreHandle_t write_audio_features_smphr = NULL;

esp_err_t write_wave_file(wav_data_t *sound_data, char *filename);
esp_err_t write_features_file(audio_features_t *features, char *filename);

esp_err_t sd_card_init()
{
    esp_err_t ret;
    sdmmc_card_t *card;

    ret = nvs_open("storage", NVS_READWRITE, &file_index_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Reading counter from NVS...");
    ret = nvs_get_i32(file_index_handle, FILE_INDEX_NVS_KEY, &file_index);
    switch (ret)
    {
    case ESP_OK:
        ESP_LOGI(TAG, "Read counter = %" PRIu32, file_index);
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGW(TAG, "The value is not initialized yet!");
        file_index = 0;
        ESP_LOGI(TAG, "Writing counter to NVS...");
        ret = nvs_set_i32(file_index_handle, FILE_INDEX_NVS_KEY, file_index);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to write index!");
        }
        break;
    default:
        ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(ret));
        file_index = 0;
    }

    ESP_LOGI(TAG, "Mounting sd card...");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = VSPI_HOST;

    // spi_bus_config_t bus_cfg = {
    //     .mosi_io_num = 23,
    //     .miso_io_num = 38,
    //     .sclk_io_num = 18,
    //     .quadwp_io_num = -1,
    //     .quadhd_io_num = -1,
    //     .max_transfer_sz = 4000,
    // };

    // ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    // if (ret != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
    //     return ret;
    // }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = GPIO_NUM_4;
    slot_config.host_id = (spi_host_device_t)host.slot;

    // 4️⃣ Mount configuration
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    // 5️⃣ Mount SD card
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount SD card (%s). Check pull-ups and wiring.",
                 esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted successfully!");

    // 6️⃣ Print card info
    sdmmc_card_print_info(stdout, card);

    while (write_audio_samples_smphr == NULL)
    {
        write_audio_samples_smphr = xSemaphoreCreateBinary();
    }
    while (write_audio_features_smphr == NULL)
    {
        write_audio_features_smphr = xSemaphoreCreateBinary();
    }
    return ESP_OK;
}

void write_audio_task(void *vp_args)
{
    while (true)
    {
        xSemaphoreTake(write_audio_samples_smphr, portMAX_DELAY);
        ESP_LOGI(TAG, "Writing wav files in sd card");
        // get impact samples
        wav_data_t *impact_samples;
        impact_samples = get_impact_samples();
        char file_name[100] = "";
        sprintf(file_name, MOUNT_POINT "/impact_sample_%ld.wav", file_index);
        // write Impact samples
        esp_err_t err = write_wave_file(impact_samples, file_name);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Cannot write impact file");
            // give semaphore to continue execution on hammer edge detection
            give_store_wait_semaphore();
            continue;
        }

        // get vibration samples
        wav_data_t *vibration_samples;
        sprintf(file_name, MOUNT_POINT "/vibration_sample_%ld.wav", file_index);
        vibration_samples = get_vibration_samples();
        // write vibration samples
        err = write_wave_file(vibration_samples, file_name);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Cannot write vibration file");
            // give semaphore to continue execution on hammer edge detection
            give_store_wait_semaphore();
            continue;
        }

        // write the calculated fft and mfcc into a file.
        xSemaphoreTake(write_audio_features_smphr, portMAX_DELAY);

        audio_features_t *impact_features = get_impact_features();
        sprintf(file_name, MOUNT_POINT "/impact_features_%ld.csv", file_index);
        err = write_features_file(impact_features, file_name);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Cannot write vibration file");
            // give semaphore to continue execution on hammer edge detection
            give_store_wait_semaphore();
            continue;
        }

        audio_features_t *vibration_features = get_vibration_features();
        sprintf(file_name, MOUNT_POINT "/vibration_features_%ld.csv", file_index);
        err = write_features_file(vibration_features, file_name);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Cannot write vibration file");
            // give semaphore to continue execution on hammer edge detection
            give_store_wait_semaphore();
            continue;
        }
        // store the file index in nvs
        file_index++;
        ESP_LOGI(TAG, "Writing counter to NVS...");
        err = nvs_set_i32(file_index_handle, FILE_INDEX_NVS_KEY, file_index);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to write index!");
        }
        err = nvs_commit(file_index_handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to commit index!");
        }
        // give semaphore to continue execution on hammer edge detection
        give_store_wait_semaphore();
    }
}

void give_write_audio_samples_smphr()
{
    xSemaphoreGive(write_audio_samples_smphr);
}
void give_write_audio_features_smphr()
{
    xSemaphoreGive(write_audio_features_smphr);
}

esp_err_t write_wave_file(wav_data_t *sound_data, char *filename)
{
    uint32_t rate = SAMPLE_RATE; // Sample rate per ms
    uint16_t chan_num = 1;       // Number of channels
    uint16_t bits = 16;          // Bit depth
    uint32_t length = sound_data->length * chan_num * bits / 8;
    int16_t byte;

    // Writes data to wav file
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
    {
        ESP_LOGE(TAG, "Output file couldn't be opened:%s (errno: %d, %s)", filename, errno, strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }

    //// WAVE Header Data
    fwrite("RIFF", 1, 4, fp);
    uint32_t chunk_size = length + 44 - 8;
    fwrite(&chunk_size, 4, 1, fp);
    fwrite("WAVE", 1, 4, fp);
    fwrite("fmt ", 1, 4, fp);
    uint32_t subchunk1_size = 16;
    fwrite(&subchunk1_size, 4, 1, fp);
    uint16_t fmt_type = 1; // 1 = PCM
    fwrite(&fmt_type, 2, 1, fp);
    fwrite(&chan_num, 2, 1, fp);
    fwrite(&rate, 4, 1, fp);
    // (Sample Rate * BitsPerSample * Channels) / 8
    uint32_t byte_rate = rate * bits * chan_num / 8;
    fwrite(&byte_rate, 4, 1, fp);
    uint16_t block_align = chan_num * bits / 8;
    fwrite(&block_align, 2, 1, fp);
    fwrite(&bits, 2, 1, fp);

    // Marks the start of the data
    fwrite("data", 1, 4, fp);
    fwrite(&length, 4, 1, fp); // Data size

    for (uint32_t i = 0; i < sound_data->length; i++)
    {
        byte = sound_data->wav[i];
        fwrite(&byte, sizeof(int16_t), 1, fp);
    }

    fclose(fp);
    ESP_LOGI(TAG, "%s file written successfully!", filename);
    return ESP_OK;
}

esp_err_t write_features_file(audio_features_t *features, char *filename)
{
    // Writes data to wav file
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
    {
        ESP_LOGE(TAG, "Output file couldn't be opened:%s (errno: %d, %s)", filename, errno, strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }
    fprintf(fp, "dom_freq,");
    for (int i = 0; i < 10; i++)
    {
        fprintf(fp, "fft_coeff_%d,", i);
    }
    for (int i = 0; i < MFCC_NUM_CEPS; i++)
    {
        if (i < MFCC_NUM_CEPS - 1)
            fprintf(fp, "mfcc_coeff_%d,", i);
        else
            fprintf(fp, "mfcc_coeff_%d\n", i);
    }
    fprintf(fp, "%f,", features->dom_freq);
    for (int i = 0; i < 10; i++)
    {
        fprintf(fp, "%f,", features->fft_coeff[i]);
    }
    for (int i = 0; i < MFCC_NUM_CEPS; i++)
    {
        if (i < MFCC_NUM_CEPS)
            fprintf(fp, "%f,", features->mfcc_values[i]);
        else
            fprintf(fp, "%f", features->mfcc_values[i]);
    }
    fclose(fp);
    return ESP_OK;
}

int32_t get_file_index()
{
    return file_index;
}

esp_err_t get_audio_sample(char *filename, wav_data_t *sound_data)
{
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        ESP_LOGE(TAG, "File couldn't be opened:%s (errno: %d, %s)", filename, errno, strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }
    typedef struct
    {
        char riff[4]; // "RIFF"
        uint32_t overall_size;

        char wave[4]; // "WAVE"

        char fmt_chunk_marker[4]; // "fmt "
        uint32_t length_of_fmt;   // 16 for PCM
        uint16_t format_type;     // 1 = PCM
        uint16_t channels;
        uint32_t sample_rate;
        uint32_t byterate;
        uint16_t block_align;
        uint16_t bits_per_sample;

        char data_chunk_header[4]; // "data"
        uint32_t data_size;
    } wav_header_t;

    wav_header_t header;

    fread(&header, sizeof(wav_header_t), 1, fp);

    int length = header.data_size / (header.channels * (header.bits_per_sample / 8));
    // Allocate buffer for audio data

    sound_data->length = length;
    if (sound_data->wav != nullptr)
        heap_caps_free(sound_data->wav);
    sound_data->wav = (typeof(sound_data->wav))heap_caps_malloc(sound_data->length * sizeof(sound_data->wav[0]), MALLOC_CAP_8BIT);

    fread(sound_data->wav, sizeof(int16_t), length, fp);

    fclose(fp);

    return ESP_OK;
}