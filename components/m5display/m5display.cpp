#include "m5display.h"
#include <M5Unified.h>

static constexpr int RESULT_Y = 0;
static constexpr int RESULT_H = 40;
static constexpr int SIGNAL_Y = 40;
static constexpr int SIGNAL_H = 20;
static constexpr int WAV_Y = 60;
static constexpr int WAV_H = 60;

static const char *label_for_output(int output)
{
    return (output == 1) ? "GOOD" : "BAD";
}

static uint32_t color_for_output(int output)
{
    return (output == 1) ? TFT_GREEN : TFT_RED;
}

void display_init()
{
    M5.Display.startWrite();
    M5.Display.fillRect(0, RESULT_Y, M5.Display.width(), RESULT_H + SIGNAL_H, TFT_BLACK);
    M5.Display.endWrite();
}

void display_model_output(int output, int32_t fileindex)
{
    M5.Display.startWrite();
    M5.Display.fillRect(0, RESULT_Y, M5.Display.width(), RESULT_H, TFT_BLACK);
    char output_txt[100];
    sprintf(output_txt, "%s:%ld", label_for_output(output), fileindex);
    M5.Display.setTextColor(color_for_output(output), TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(4, RESULT_Y + 8);
    M5.Display.printf(">> %s", output_txt);
    M5.Display.endWrite();
}

void display_signal_info(float impact_freq, float vibration_freq)
{
    M5.Display.startWrite();
    M5.Display.fillRect(0, SIGNAL_Y, M5.Display.width(), SIGNAL_H, TFT_BLACK);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(4, SIGNAL_Y + 4);
    M5.Display.printf("Imp:%.0fHz  Vib:%.0fHz", impact_freq, vibration_freq);
    M5.Display.endWrite();
}

void display_clear_result()
{
    M5.Display.startWrite();
    M5.Display.fillRect(0, RESULT_Y, M5.Display.width(), RESULT_H + SIGNAL_H, TFT_BLACK);
    M5.Display.endWrite();
}

void display_wav(wav_data_t *impact, wav_data_t *vibration)
{
    if (impact == nullptr || vibration == nullptr)
        return;

    int width = M5.Display.width();
    int height = WAV_H;
    int base_y = WAV_Y + height / 2;

    M5.Display.startWrite();
    M5.Display.fillRect(0, WAV_Y, width, height, TFT_BLACK);

    // Split screen: left half = impact, right half = vibration
    int impact_width = width / 2;
    int vibration_width = width - impact_width;

    // Find max amplitude across BOTH signals for a shared scale
    int32_t max_val = 1;
    for (size_t i = 0; i < impact->length; i++)
    {
        int32_t v = abs(impact->wav[i]);
        if (v > max_val)
            max_val = v;
    }
    for (size_t i = 0; i < vibration->length; i++)
    {
        int32_t v = abs(vibration->wav[i]);
        if (v > max_val)
            max_val = v;
    }

    // Draw impact (left half, yellow)
    int prev_y = base_y;
    for (int x = 0; x < impact_width; x++)
    {
        size_t idx = (size_t)x * impact->length / impact_width;
        if (idx >= impact->length)
            idx = impact->length - 1;

        int sample_y = base_y - (int)((int32_t)impact->wav[idx] * (height / 2) / max_val);
        sample_y = sample_y < WAV_Y ? WAV_Y : (sample_y >= WAV_Y + height ? WAV_Y + height - 1 : sample_y);

        int y0 = prev_y < sample_y ? prev_y : sample_y;
        int y1 = prev_y < sample_y ? sample_y : prev_y;
        M5.Display.drawFastVLine(x, y0, y1 - y0 + 1, TFT_YELLOW);
        prev_y = sample_y;
    }

    // Draw vibration (right half, cyan)
    prev_y = base_y;
    for (int x = 0; x < vibration_width; x++)
    {
        size_t idx = (size_t)x * vibration->length / vibration_width;
        if (idx >= vibration->length)
            idx = vibration->length - 1;

        int screen_x = impact_width + x;
        int sample_y = base_y - (int)((int32_t)vibration->wav[idx] * (height / 2) / max_val);
        sample_y = sample_y < WAV_Y ? WAV_Y : (sample_y >= WAV_Y + height ? WAV_Y + height - 1 : sample_y);

        int y0 = prev_y < sample_y ? prev_y : sample_y;
        int y1 = prev_y < sample_y ? sample_y : prev_y;
        M5.Display.drawFastVLine(screen_x, y0, y1 - y0 + 1, TFT_CYAN);
        prev_y = sample_y;
    }

    // Divider between impact and vibration
    M5.Display.drawFastVLine(impact_width, WAV_Y, height, TFT_DARKGREY);
    // Baselines
    M5.Display.drawFastHLine(0, base_y, impact_width, 0x303030u);
    M5.Display.drawFastHLine(impact_width, base_y, vibration_width, 0x303030u);

    M5.Display.endWrite();
}