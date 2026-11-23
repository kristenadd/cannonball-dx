#pragma once
// SDL_Renderer implementation of the glb:: presentation backend.
//
// Used where OpenGL ES is unavailable (visionOS, where SDL renders with Metal).
// It provides the same API as the GLES backend in gl_backend.hpp, minus the
// GLSL CRT shaders: shader sources are accepted and ignored, and uniform
// setters are no-ops. The game image and the optional 8-bit overlay (multiplied
// over the image, as in the GLES path) are drawn with SDL_RenderCopy.
//
// Coordinates follow the GLES backend: set_*_rect_pixels() take bottom-left
// origin rectangles and the *_top_left() helpers convert from top-left.

#include <SDL.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace glb {

struct State {
    SDL_Window*   window     = nullptr;
    SDL_Renderer* renderer   = nullptr;
    SDL_Texture*  texGame    = nullptr;
    SDL_Texture*  texOverlay = nullptr;
    bool overlayReady = false;
    bool linearFilter = false;

    // Backbuffer (renderer output, in pixels) and texture sizes
    int fbW = 0, fbH = 0;
    int gameW = 0, gameH = 0;
    int overlayW = 0, overlayH = 0;

    // Destination rects (bottom-left origin, like the GLES backend)
    bool useDstRect = false; int dstX=0,dstY=0,dstW=0,dstH=0;
    bool useOverlayDstRect = false; int ovDstX=0,ovDstY=0,ovDstW=0,ovDstH=0;

    enum class PixFmt { RGBA, ABGR, A8, RGB555 };
    PixFmt gameFmt    = PixFmt::RGBA;
    PixFmt overlayFmt = PixFmt::A8;

    std::vector<uint32_t> scratch;   // A8 overlay / RGB555 game expansion
    std::vector<uint32_t> rgb555_lut; // 5-5-5-1 -> ABGR8888
};

inline State G{};

// ---------------- Context / window helpers ----------------
inline void request_context_attributes() {}
inline Uint32 window_flags() { return SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE; }

// The "context" for this backend is the SDL_Renderer.
inline void* create_context(SDL_Window* window)
{
    SDL_Renderer* r = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!r)
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
    G.renderer = r;
    return r;
}

inline void destroy_context(void* context)
{
    if (context)
        SDL_DestroyRenderer(static_cast<SDL_Renderer*>(context));
    if (G.renderer == context)
        G.renderer = nullptr;
}

inline void drawable_size(SDL_Window* window, int* w, int* h)
{
    if (G.renderer && SDL_GetRendererOutputSize(G.renderer, w, h) == 0)
        return;
    SDL_GetWindowSizeInPixels(window, w, h);
}

inline std::string adapt_shader_source(const char* src) { return src ? std::string(src) : std::string(); }

// ---------------- Texture helpers ----------------
inline Uint32 sdl_format(State::PixFmt fmt)
{
    switch (fmt) {
        // 16-bit 5-5-5-1 frames are expanded to 32-bit on upload: Metal's
        // packed 16-bit formats are not available everywhere (e.g. the
        // visionOS simulator), so all game textures are 8888.
        case State::PixFmt::RGB555: return SDL_PIXELFORMAT_ABGR8888;
        // GL_RGBA bytes R,G,B,A in memory.
        case State::PixFmt::RGBA:   return SDL_PIXELFORMAT_ABGR8888;
        // GL_ABGR_EXT bytes A,B,G,R in memory.
        case State::PixFmt::ABGR:   return SDL_PIXELFORMAT_RGBA8888;
        case State::PixFmt::A8:     return SDL_PIXELFORMAT_ABGR8888; // expanded on upload
    }
    return SDL_PIXELFORMAT_ABGR8888;
}

inline SDL_Texture* make_texture(Uint32 format, int w, int h, bool linear)
{
    if (!G.renderer || w <= 0 || h <= 0)
        return nullptr;
    SDL_Texture* t = SDL_CreateTexture(G.renderer, format, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!t) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << "\n";
        return nullptr;
    }
    SDL_SetTextureScaleMode(t, linear ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    return t;
}

// ---------------- Pixel format setters ----------------
inline void set_game_pixel_format(State::PixFmt fmt) { G.gameFmt = fmt; }
inline void set_overlay_pixel_format(State::PixFmt fmt) { G.overlayFmt = fmt; }
inline void set_game_pixel_format_abgr(bool isABGR) { G.gameFmt = isABGR ? State::PixFmt::ABGR : State::PixFmt::RGBA; }
inline void set_overlay_pixel_format_abgr(bool isABGR) { G.overlayFmt = isABGR ? State::PixFmt::ABGR : State::PixFmt::RGBA; }
inline void set_overlay_pixel_format_alpha8(bool enableA8) { G.overlayFmt = enableA8 ? State::PixFmt::A8 : State::PixFmt::RGBA; }
inline void set_overlay_pixel_format_a8() { G.overlayFmt = State::PixFmt::A8; }

inline State::PixFmt deduce_pixfmt_from_surface(const SDL_Surface* s)
{
    if (!s || !s->format) return State::PixFmt::RGBA;
    if (s->format->BitsPerPixel == 8)  return State::PixFmt::A8;
    if (s->format->BitsPerPixel == 16) return State::PixFmt::RGB555;
    return State::PixFmt::RGBA;
}
inline void auto_configure_pixel_formats_from_surfaces(const SDL_Surface* game, const SDL_Surface* overlay)
{
    if (game)    set_game_pixel_format(deduce_pixfmt_from_surface(game));
    if (overlay) set_overlay_pixel_format(deduce_pixfmt_from_surface(overlay));
}
inline void auto_configure_pixel_formats(const SDL_Surface* game, const SDL_Surface* overlay)
{
    auto_configure_pixel_formats_from_surfaces(game, overlay);
}

// ---------------- Init / shutdown ----------------
inline bool init(SDL_Window* window,
                 int gameW, int gameH,
                 int overlayW, int overlayH,
                 const char* vertexSrc = nullptr,
                 const char* fragmentSrc = nullptr,
                 bool /*createOffscreen*/ = false,
                 int /*offscreenW*/ = 0, int /*offscreenH*/ = 0)
{
    G.window   = window;
    G.gameW    = gameW;
    G.gameH    = gameH;
    G.overlayW = overlayW;
    G.overlayH = overlayH;
    // Match the GLES backend: linear filtering when a CRT shader was requested
    // (it would curve/resample the image), nearest otherwise.
    G.linearFilter = vertexSrc && fragmentSrc;
    if (G.linearFilter)
        std::cout << "CRT shaders are not available with the SDL renderer; using plain output.\n";

    if (!G.renderer) {
        std::cerr << "glb::init: no renderer (create_context failed)\n";
        return false;
    }
    drawable_size(window, &G.fbW, &G.fbH);

    G.texGame = make_texture(sdl_format(G.gameFmt), gameW, gameH, G.linearFilter);
    G.texOverlay = make_texture(sdl_format(G.overlayFmt), overlayW, overlayH, false);
    if (G.texOverlay)
        SDL_SetTextureBlendMode(G.texOverlay, SDL_BLENDMODE_MOD);
    G.overlayReady = false;
    return G.texGame != nullptr;
}

inline void shutdown()
{
    if (G.texGame)    SDL_DestroyTexture(G.texGame);
    if (G.texOverlay) SDL_DestroyTexture(G.texOverlay);
    SDL_Renderer* renderer = G.renderer;   // destroyed by destroy_context()
    G = State{};
    G.renderer = renderer;
}

// Recreate the game texture after set_game_pixel_format()/size changes
// (used by the pixel-scaler renderer).
inline void reallocate_game_texture(int width, int height)
{
    G.gameW = width;
    G.gameH = height;
    if (G.texGame) SDL_DestroyTexture(G.texGame);
    G.texGame = make_texture(sdl_format(G.gameFmt), width, height, G.linearFilter);
}

inline void reallocate_overlay_storage(int width = 0, int height = 0)
{
    if (width > 0)  G.overlayW = width;
    if (height > 0) G.overlayH = height;
    if (G.texOverlay) SDL_DestroyTexture(G.texOverlay);
    G.texOverlay = make_texture(sdl_format(G.overlayFmt), G.overlayW, G.overlayH, false);
    if (G.texOverlay)
        SDL_SetTextureBlendMode(G.texOverlay, SDL_BLENDMODE_MOD);
    G.overlayReady = false;
}

// ---------------- Uploads ----------------
// The engine's 16-bit pixels use the GL 5-5-5-1 layout: R in bits 15-11,
// G in 10-6, B in 5-1, alpha in bit 0 (ignored: the game image is opaque).
inline const uint32_t* rgb555_lut()
{
    if (G.rgb555_lut.empty()) {
        G.rgb555_lut.resize(65536);
        for (uint32_t v = 0; v < 65536; ++v) {
            const uint32_t r5 = (v >> 11) & 31, g5 = (v >> 6) & 31, b5 = (v >> 1) & 31;
            const uint32_t r = (r5 << 3) | (r5 >> 2);
            const uint32_t g = (g5 << 3) | (g5 >> 2);
            const uint32_t b = (b5 << 3) | (b5 >> 2);
            G.rgb555_lut[v] = 0xFF000000u | (b << 16) | (g << 8) | r;  // ABGR8888
        }
    }
    return G.rgb555_lut.data();
}

inline void update_game_texture(const void* pixels, int pitchBytes, int w, int h)
{
    if (!G.texGame || w != G.gameW || h != G.gameH)
        reallocate_game_texture(w, h);
    if (!G.texGame)
        return;

    if (G.gameFmt != State::PixFmt::RGB555) {
        SDL_UpdateTexture(G.texGame, nullptr, pixels, pitchBytes);
        return;
    }

    const uint32_t* lut = rgb555_lut();
    void* locked = nullptr;
    int lockedPitch = 0;
    if (SDL_LockTexture(G.texGame, nullptr, &locked, &lockedPitch) != 0)
        return;
    const uint8_t* src = static_cast<const uint8_t*>(pixels);
    uint8_t* dst = static_cast<uint8_t*>(locked);
    for (int y = 0; y < h; ++y) {
        const uint16_t* s = reinterpret_cast<const uint16_t*>(src + static_cast<size_t>(y) * pitchBytes);
        uint32_t* d = reinterpret_cast<uint32_t*>(dst + static_cast<size_t>(y) * lockedPitch);
        for (int x = 0; x < w; ++x)
            d[x] = lut[s[x]];
    }
    SDL_UnlockTexture(G.texGame);
}

inline void update_overlay_texture(const void* pixels, int pitchBytes, int w, int h)
{
    if (!G.texOverlay || w != G.overlayW || h != G.overlayH)
        reallocate_overlay_storage(w, h);
    if (!G.texOverlay)
        return;

    if (G.overlayFmt == State::PixFmt::A8) {
        // 8-bit luminance: 0xFF leaves the image untouched, 0x00 is black.
        G.scratch.resize(static_cast<size_t>(w) * h);
        const uint8_t* src = static_cast<const uint8_t*>(pixels);
        for (int y = 0; y < h; ++y) {
            const uint8_t* row = src + static_cast<size_t>(y) * pitchBytes;
            uint32_t* dst = G.scratch.data() + static_cast<size_t>(y) * w;
            for (int x = 0; x < w; ++x) {
                const uint32_t l = row[x];
                dst[x] = 0xFF000000u | (l << 16) | (l << 8) | l;  // ABGR8888: R,G,B,A bytes
            }
        }
        SDL_UpdateTexture(G.texOverlay, nullptr, G.scratch.data(), w * 4);
    } else {
        SDL_UpdateTexture(G.texOverlay, nullptr, pixels, pitchBytes);
    }
    G.overlayReady = true;
}

inline void clear_overlay_texture() { G.overlayReady = false; }

// ---------------- Shader uniforms (not supported) ----------------
inline void set_uniform(const char*, float) {}
inline void set_uniform2(const char*, float, float) {}
inline void set_uniform3(const char*, float, float, float) {}
inline void set_uniform4(const char*, float, float, float, float) {}

// ---------------- Destination rects ----------------
inline void set_present_rect_pixels(int x, int y, int w, int h)
{
    G.dstX = x; G.dstY = y; G.dstW = w; G.dstH = h; G.useDstRect = true;
}
inline void clear_present_rect() { G.useDstRect = false; }
inline void set_overlay_rect_pixels(int x, int y, int w, int h)
{
    G.ovDstX = x; G.ovDstY = y; G.ovDstW = w; G.ovDstH = h; G.useOverlayDstRect = true;
}
inline void clear_overlay_rect() { G.useOverlayDstRect = false; }

inline void refresh_drawable_size()
{
    if (!G.window) return;
    int w = 0, h = 0;
    drawable_size(G.window, &w, &h);
    if (w > 0 && h > 0) { G.fbW = w; G.fbH = h; }
}

inline void set_present_rect_pixels_top_left(int x, int y, int w, int h)
{
    refresh_drawable_size();
    set_present_rect_pixels(x, G.fbH - (y + h), w, h);
}
inline void set_overlay_rect_pixels_top_left(int x, int y, int w, int h)
{
    refresh_drawable_size();
    set_overlay_rect_pixels(x, G.fbH - (y + h), w, h);
}

inline void on_drawable_resized() { refresh_drawable_size(); }

// ---------------- Drawing ----------------
inline void clear(float r, float g, float b, float a)
{
    if (!G.renderer) return;
    SDL_SetRenderDrawColor(G.renderer,
        static_cast<Uint8>(r * 255.f), static_cast<Uint8>(g * 255.f),
        static_cast<Uint8>(b * 255.f), static_cast<Uint8>(a * 255.f));
    SDL_RenderClear(G.renderer);
}

inline SDL_Rect to_sdl_rect(bool use, int x, int yBottom, int w, int h)
{
    if (!use) return SDL_Rect{0, 0, G.fbW, G.fbH};
    return SDL_Rect{x, G.fbH - (yBottom + h), w, h};
}

inline void draw(bool /*useOffscreen*/, bool drawOverlay)
{
    if (!G.renderer || !G.texGame) return;
    refresh_drawable_size();

    // Metal back buffers are not preserved between frames: always clear the
    // letterbox area.
    clear(0.f, 0.f, 0.f, 1.f);

    const SDL_Rect dst = to_sdl_rect(G.useDstRect, G.dstX, G.dstY, G.dstW, G.dstH);
    SDL_SetTextureBlendMode(G.texGame, SDL_BLENDMODE_NONE);
    SDL_RenderCopy(G.renderer, G.texGame, nullptr, &dst);

    if (drawOverlay && G.overlayReady && G.texOverlay) {
        const SDL_Rect ov = G.useOverlayDstRect
            ? to_sdl_rect(true, G.ovDstX, G.ovDstY, G.ovDstW, G.ovDstH)
            : dst;
        SDL_RenderCopy(G.renderer, G.texOverlay, nullptr, &ov);
    }
}

inline void set_swap_interval(int interval)
{
    if (G.renderer)
        SDL_RenderSetVSync(G.renderer, interval != 0 ? 1 : 0);
}
inline int get_swap_interval()
{
    // SDL_Renderer presents with vsync on visionOS.
    return 1;
}

inline void present()
{
    if (G.renderer)
        SDL_RenderPresent(G.renderer);
}

} // namespace glb
