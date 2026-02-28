#include <stdio.h>
#include "m5display.h"
#include <M5Unified.h>
#include <set>
#include <esp_heap_caps.h>
#include "audioprocessing.h"
/*

*/
#define TIME_STEPS 15

wav_drawer_t wav_drawer;
fft_drawer_t fft_drawer;

fft_peak_t fft_peak;
fft_history_t fft_history;
mfcc_history_t mfcc_history;
uint8_t *stripe_buf = nullptr;

bool wav_drawer_t::setup(LGFX_Device *gfx, const rect_t &rect)
{
    if (gfx == nullptr)
    {
        return false;
    }
    _gfx = gfx;
    draw_rect = rect;
    gfx->fillRect(rect.x, rect.y, rect.w, rect.h, bg_color);
    gfx->drawFastVLine(rect.x + (rect.w >> 1), rect.y, rect.h, line_color);
    int width = rect.w;
    prev_y = (int16_t *)heap_caps_malloc(width * sizeof(int16_t), MALLOC_CAP_8BIT);
    prev_h = (int16_t *)heap_caps_malloc(width * sizeof(int16_t), MALLOC_CAP_8BIT);
    memset(prev_y, 0, width * sizeof(int16_t));
    memset(prev_h, 0, width * sizeof(int16_t));

    return true;
}

bool wav_drawer_t::update(const wav_data_t &wav_data)
{
    auto gfx = _gfx;

    int32_t width = draw_rect.w;
    int32_t height = draw_rect.h;

    int wav_count = wav_data.length;
    int wav_index = wav_data.searchEdge(draw_rect.w >> 1, wav_count / 3);
    auto wav = wav_data.wav;

    int32_t max_value = 1;
    auto wi = wav_index;
    for (int i = 0; i < width; ++i)
    {
        int32_t tmp = abs(wav[wi]);
        if (max_value < tmp)
        {
            max_value = tmp;
        }
        if (++wi >= wav_count)
        {
            wi = 0;
        }
    }
    int new_k = (draw_rect.h << 15) / max_value;
    if (new_k > 65536)
    {
        new_k = 65536;
    }
    static int k;
    if (k > new_k)
    {
        k = new_k;
    }
    else
    {
        k = (k * 127 + new_k) >> 7;
    }
    int32_t value1 = (32768 - wav[wav_index] * k) >> 16;
    int32_t value2 = value1;
    int32_t base_y = draw_rect.y + (height >> 1);

    for (int i = 0; i < width; ++i)
    {
        if (++wav_index >= wav_count)
        {
            wav_index = 0;
        }
        int32_t x = i + draw_rect.x;
        int32_t y = prev_y[i];
        int32_t h = prev_h[i];

        gfx->setColor(i == (width >> 1) ? line_color : bg_color);
        gfx->drawFastVLine(x, base_y + y, h);

        int32_t value0 = value1;
        value1 = value2;
        value2 = (32768 - wav[wav_index] * k) >> 16;
        int32_t value_01 = (value0 + value1) >> 1;
        int32_t value_12 = (value1 + value2) >> 1;

        int32_t y_min = (value_01 < value_12) ? value_01 : value_12;
        if (y_min > value1)
        {
            y_min = value1;
        }

        int32_t y_max = (value_01 > value_12) ? value_01 : value_12;
        if (y_max < value1)
        {
            y_max = value1;
        }

        y = y_min;
        h = y_max + 1 - y;
        prev_y[i] = y;
        prev_h[i] = h;
        gfx->drawPixel(x, base_y, line_color);
        /// draw new wave.
        gfx->drawFastVLine(x, base_y + y, h, fg_color);
    }

    return true;

    return false;
}

bool fft_drawer_t::setup(LGFX_Device *gfx, const rect_t &rect)
{
    if (gfx == nullptr)
    {
        return false;
    }
    _gfx = gfx;
    draw_rect = rect;
    gfx->fillRect(rect.x, rect.y, rect.w, rect.h, bg_color);
    int width = rect.w;
    prev_y = (uint16_t *)heap_caps_malloc(width * sizeof(int16_t), MALLOC_CAP_8BIT);
    prev_h = (uint16_t *)heap_caps_malloc(width * sizeof(int16_t), MALLOC_CAP_8BIT);
    memset(prev_y, 0, width * sizeof(int16_t));
    memset(prev_h, 0, width * sizeof(int16_t));
    return true;
}

bool fft_drawer_t::update(fft_data_t &fft_data)
{
    auto gfx = _gfx;

    int32_t width = draw_rect.w;
    int32_t height = draw_rect.h - 1;

    int32_t value1 = height - (((int32_t)(fft_data.get_data_by_pixel(0, width) * height)) >> 16);
    if (value1 < 0)
    {
        value1 = 0;
    }
    int32_t value2 = value1;
    for (int i = 0; i < width; ++i)
    {
        int32_t x = i + draw_rect.x;
        int32_t y = prev_y[i];
        int32_t h = prev_h[i];

        gfx->drawFastVLine(x, draw_rect.y + y, h, bg_color);

        int32_t value0 = value1;
        value1 = value2;
        value2 = height - (((int32_t)(fft_data.get_data_by_pixel(i + 1, width) * height)) >> 16);
        if (value2 < 0)
        {
            value2 = 0;
        }

        int32_t value_01 = (value0 + value1) >> 1;
        int32_t value_12 = (value1 + value2) >> 1;

        int32_t y_min = (value_01 < value_12) ? value_01 : value_12;
        if (y_min > value1)
        {
            y_min = value1;
        }

        int32_t y_max = (value_01 > value_12) ? value_01 : value_12;
        if (y_max < value1)
        {
            y_max = value1;
        }

        y = y_min;
        h = y_max + 1 - y;
        prev_y[i] = y;
        prev_h[i] = h;

        gfx->drawFastVLine(x, draw_rect.y + y, h, fg_color);
    }

    return true;
}

bool fft_peak_t::setup(LGFX_Device *gfx, const rect_t &rect)
{
    if (gfx == nullptr)
    {
        return false;
    }
    _gfx = gfx;
    draw_rect = rect;
    gfx->fillRect(rect.x, rect.y, rect.w, rect.h, bg_color);

    return true;
}

bool fft_history_t::setup(LGFX_Device *gfx, const rect_t &rect)
{
    if (gfx == nullptr)
    {
        return false;
    }
    _gfx = gfx;
    draw_rect = rect;
    int width = rect.w;
    int height = rect.h;

    color_map = (uint8_t *)heap_caps_malloc(width * height * sizeof(uint8_t), MALLOC_CAP_8BIT);
    memset(color_map, 0, width * height * sizeof(uint8_t));
    return true;
}

bool mfcc_history_t::setup(LGFX_Device *gfx, const rect_t &rect)
{
    if (gfx == nullptr)
    {
        return false;
    }
    _gfx = gfx;
    draw_rect = rect;
    int width = rect.w;
    int height = rect.h;

    color_map = (uint8_t *)heap_caps_malloc((width)*height * sizeof(uint8_t), MALLOC_CAP_8BIT);
    memset(color_map, 0, width * height * sizeof(uint8_t));
    return true;
}

bool mfcc_history_t::update(const float *mfcc_data)
{
    int32_t width = draw_rect.w;
    int32_t height = draw_rect.h;

    uint8_t STEP_PX = width/TIME_STEPS;
    const int bins = MFCC_NUM_CEPS;

    // 1️⃣ Shift image RIGHT by STEP_PX
    for (int y = 0; y < height; y++)
    {
        memmove(
            &color_map[y * width + STEP_PX],
            &color_map[y * width],
            width - STEP_PX);
    }

    // 2️⃣ Clear leftmost stripe
    for (int y = 0; y < height; y++)
    {
        memset(&color_map[y * width], 0, STEP_PX);
    }

    // 3️⃣ Draw new MFCC bins into leftmost stripe
    for (int i = 0; i < bins; i++)
    {
        int y = (i * height) / bins;

        float v = mfcc_data[i];

        if (v < -100.0f)
            v = -100.0f;
        if (v > 100.0f)
            v = 100.0f;

        uint8_t gray =
            (uint8_t)((v + 100.0f) * 255.0f / 200.0f);

        for (int x = 0; x < STEP_PX; x++)
        {
            color_map[y * width + x] = gray;
        }
    }

    // 4️⃣ Push full image
    _gfx->pushGrayscaleImage(
        draw_rect.x,
        draw_rect.y,
        width,
        height,
        color_map,
        m5gfx::color_depth_t::grayscale_8bit,
        fg_color,
        bg_color);
    return true;
}

void display_init()
{
    M5.Display.startWrite();

    int16_t w = M5.Display.width();
    int16_t h = M5.Display.height() >> 2;
    printf("height:%d", h);

    rect_t rect_fft_peak = {0, 0, w, h};
    rect_t rect_fft_drawer = {0, h, w, h};
    rect_t rect_fft_history = {0, (int16_t)(2 * h), w, h};
    rect_t rect_wav_drawer = {0, (int16_t)(3 * h), w, h};

    fft_peak.setup(&M5.Display, rect_fft_peak);
    fft_drawer.setup(&M5.Display, rect_fft_drawer);
    mfcc_history.setup(&M5.Display, rect_fft_history);
    wav_drawer.setup(&M5.Display, rect_wav_drawer);

    uint8_t STEP_PX = w/TIME_STEPS;
    stripe_buf = (uint8_t *)heap_caps_malloc(STEP_PX * h, MALLOC_CAP_8BIT);

    M5.Display.setTextSize(w / 64.0f, h / 16.0f);
    M5.Display.setFont(&fonts::AsciiFont8x16);
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
}

void display_update()
{
    M5.Display.display();
    wav_data_t *wav_data = nullptr;
    wav_data = get_wav_data();
    wav_drawer.update(*wav_data);

    fft_data_t *fft_data = get_fft_data();
    fft_drawer.update(*fft_data);

    float *mfcc_data = get_mfcc_data();
    mfcc_history.update(mfcc_data);
}