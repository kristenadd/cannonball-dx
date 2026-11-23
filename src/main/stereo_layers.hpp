#pragma once
// Stereoscopic layer separation (visionOS).
//
// Normally every System 16 video pass draws into one frame buffer. For stereo
// presentation the passes are split by depth instead:
//
//   frame buffer   road background + background/foreground tiles  (far: shown by SDL)
//   LAYER_ROAD     road surface, shown as BANDS horizontal bands
//   LAYER_SPRITE0 + k   sprites whose base (lowest row) is in band k
//   LAYER_TEXT     text / HUD                                     (nearest)
//
// Depth comes from the screen row, the same way for the road and for sprites:
// on OutRun's road, the further below the horizon something touches the
// ground, the nearer it is (row distance from the horizon is proportional to
// 1 / distance, which is what stereo disparity follows). A sprite therefore
// sits at the depth of the road band it stands on, just in front of it, so
// nearer road never covers it.
//
// Each layer starts filled with EMPTY. Passes draw palette indices over it as
// usual; sprite shadows set the shadow bit on whatever is underneath, so a
// shadow over EMPTY marks a translucent shadow pixel. Layers are double
// buffered exactly like Video::pixel_buffers: one is drawn while the previous
// frame is presented. Sprite layers record which rows they touched so only
// those rows are cleared and converted.

#include <algorithm>
#include <cstdint>
#include <vector>

#include "apple_platform.hpp"
#include "globals.hpp"
#include "frontend/config.hpp"

namespace stereo
{
    constexpr int BANDS = 16;               // road bands == sprite depth layers
    constexpr int SPRITE_LAYERS = BANDS;
    enum Layer
    {
        LAYER_ROAD = 0,
        LAYER_SPRITE0 = 1,
        LAYER_TEXT = LAYER_SPRITE0 + SPRITE_LAYERS,
        LAYER_COUNT
    };

    // Not a valid palette index (indices are < 2 * S16_PALETTE_ENTRIES).
    constexpr uint16_t EMPTY = 0x4000;
    inline bool is_empty(uint16_t v)  { return (v & 0x4000) != 0; }
    inline bool is_shadow_over_empty(uint16_t v) { return (v & 0x4000) && (v & 0x3000); }

    // Stereo depth setting: 0 = off, 1..3 = low/medium/high. visionOS only.
    inline bool active()
    {
#if CB_VISIONOS
        return config.video.stereo_depth > 0;
#else
        return false;
#endif
    }

    struct RowRange { int first = 0, last = -1; };   // inclusive, empty if last < first

    struct Buffers
    {
        // Same padding as Video::pixel_buffers: the sprite renderer may write
        // up to `pad` elements before the start and two rows past the end.
        std::vector<uint16_t> storage[2][LAYER_COUNT];
        uint16_t* layer[2][LAYER_COUNT] = {};
        RowRange rows[2][LAYER_COUNT];
        int width = 0, height = 0, pad = 0;
        int current = 0;
    };

    inline Buffers& buffers()
    {
        static Buffers b;
        return b;
    }

    inline void allocate(int width, int height, int pad)
    {
        Buffers& b = buffers();
        b.width = width; b.height = height; b.pad = pad; b.current = 0;
        const size_t size = static_cast<size_t>(width) * (height + 2) + pad;
        for (int f = 0; f < 2; ++f)
            for (int l = 0; l < LAYER_COUNT; ++l) {
                b.storage[f][l].assign(size, EMPTY);
                b.layer[f][l] = b.storage[f][l].data() + pad;
                b.rows[f][l] = RowRange{};
            }
    }

    inline void release()
    {
        Buffers& b = buffers();
        for (int f = 0; f < 2; ++f)
            for (int l = 0; l < LAYER_COUNT; ++l) {
                std::vector<uint16_t>().swap(b.storage[f][l]);
                b.layer[f][l] = nullptr;
            }
        b.width = b.height = 0;
    }

    inline bool allocated() { return buffers().width > 0; }

    // Layer being drawn this frame / presented from the previous frame.
    inline uint16_t* draw_layer(int l) { Buffers& b = buffers(); return b.layer[b.current][l]; }
    inline const uint16_t* present_layer(int l) { Buffers& b = buffers(); return b.layer[b.current ^ 1][l]; }
    inline RowRange present_rows(int l) { Buffers& b = buffers(); return b.rows[b.current ^ 1][l]; }

    inline bool is_sprite_layer(int l) { return l >= LAYER_SPRITE0 && l < LAYER_SPRITE0 + SPRITE_LAYERS; }

    inline void begin_frame()
    {
        Buffers& b = buffers();
        for (int l = 0; l < LAYER_COUNT; ++l) {
            std::vector<uint16_t>& s = b.storage[b.current][l];
            RowRange& r = b.rows[b.current][l];
            if (is_sprite_layer(l)) {
                // Clear only what was drawn into this buffer last time. A
                // sprite may spill a little outside its rows (padding), so
                // clear one extra row either side.
                if (r.last >= r.first) {
                    const int first = std::max(0, r.first - 1);
                    const int last  = std::min(b.height + 1, r.last + 1);
                    std::fill(s.begin() + b.pad + static_cast<size_t>(first) * b.width,
                              s.begin() + b.pad + static_cast<size_t>(last + 1) * b.width, EMPTY);
                    std::fill(s.begin(), s.begin() + b.pad, EMPTY);
                }
                r = RowRange{};
            } else {
                std::fill(s.begin(), s.end(), EMPTY);
                r = RowRange{0, b.height - 1};
            }
        }
    }

    inline void swap() { buffers().current ^= 1; }

    // Depth band of a sprite from its base (lowest) screen row.
    inline int sprite_band(int base_row)
    {
        const int h = std::max(1, buffers().height);
        return std::clamp(base_row * BANDS / h, 0, BANDS - 1);
    }

    // Record rows a sprite occupies in its layer (clipped to the screen).
    inline void note_rows(int l, int first, int last)
    {
        Buffers& b = buffers();
        first = std::max(first, 0);
        last  = std::min(last, b.height - 1);
        if (last < first) return;
        RowRange& r = b.rows[b.current][l];
        if (r.last < r.first) { r.first = first; r.last = last; }
        else { r.first = std::min(r.first, first); r.last = std::max(r.last, last); }
    }
}
