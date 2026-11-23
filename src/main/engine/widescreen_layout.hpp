#pragma once
// Helpers for aspect-ratio dependent rendering.
//
// The game width is no longer limited to the fixed presets (4:3 = 320,
// 16:10 = 356, 16:9 = 404, 21:9 = 536): the FULL aspect mode picks any width in
// that range to match the display. Code that used to test for the 21:9 preset
// uses these width-based checks instead. For the presets they give exactly the
// previous results.

#include "globals.hpp"
#include "frontend/config.hpp"

namespace widescreen_layout
{
    // Native (1x) game width.
    inline int native_width()
    {
        return S16_WIDTH + (config.s16_x_off << 1);
    }

    // Wider than 16:9: true for 21:9, and for FULL on displays wider than 16:9.
    // The music-select side art and the stage 3 cloud tiling are needed here.
    inline bool wider_than_16_9()
    {
        return config.s16_x_off > (S16_WIDTH_WIDE - S16_WIDTH) / 2;
    }

    // The 21:9 music-select side art is authored on a 536-pixel canvas centred
    // on the 4:3 image. Narrower widths show its centre: subtract this shift
    // from canvas x coordinates and clip to [0, native_width()).
    inline int side_art_shift()
    {
        return (S16_WIDTH_ULTRAWIDE - native_width()) / 2;
    }
}
