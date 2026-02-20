#ifndef __M5DISPLAY_H__
#define __M5DISPLAY_H__
#include <stdint.h>
#include <M5Unified.h>
#include <set>
#include <m5mic.h>

struct rect_t
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
};

class wav_drawer_t
{
    LGFX_Device *_gfx = nullptr;
    int16_t *prev_y = nullptr;
    int16_t *prev_h = nullptr;
    rect_t draw_rect = {0, 0, 0, 0};
    uint32_t bg_color = 0x000000u;
    uint32_t fg_color = 0xFFFFFFu;
    uint32_t line_color = 0x303030u;

public:
    bool setup(LGFX_Device *gfx, const rect_t &rect);
    bool update(const wav_data_t &wav_data);
};

class fft_drawer_t
{
    LGFX_Device *_gfx = nullptr;
    uint16_t *prev_y = nullptr;
    uint16_t *prev_h = nullptr;
    rect_t draw_rect = {0, 0, 0, 0};
    uint32_t bg_color = 0x000066u;
    uint32_t fg_color = 0x00FF00u;

public:
    bool setup(LGFX_Device *gfx, const rect_t &rect);
    bool update(fft_data_t &fft_data);
};

class fft_peak_t
{
    LGFX_Device *_gfx = nullptr;
    rect_t draw_rect = {0, 0, 0, 0};
    uint32_t bg_color = 0x000000u;
    uint32_t fg_color = 0x00FFFFu;
    std::set<uint16_t> peak_index_set;
    uint16_t prev_peak_index = UINT16_MAX;
    char text_buf[10] = {
        0,
    };
    char prev_text[10] = {
        0,
    };
    uint8_t step = 0;

public:
    bool setup(LGFX_Device *gfx, const rect_t &rect);
    //   bool update(const fft_data_t& fft_data);
};

class fft_history_t
{
    LGFX_Device *_gfx = nullptr;
    uint8_t *color_map = nullptr;
    rect_t draw_rect = {0, 0, 0, 0};
    uint32_t bg_color = 0x000033u;
    uint32_t fg_color = 0xFFFF00u;
    int step = 0;

public:
    bool setup(LGFX_Device *gfx, const rect_t &rect);
    // bool update(const fft_data_t &fft_data);
};

void display_init();

void display_update();

#endif