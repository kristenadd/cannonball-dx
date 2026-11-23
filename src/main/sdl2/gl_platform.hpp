#pragma once
// Platform selection for the OpenGL ES 2.0 style renderer.
//
// The renderer is written against the OpenGL ES 2.0 API. On Linux it uses a
// native GLES2 driver, on Windows it uses ANGLE. macOS has no native GLES, so
// by default it uses the system OpenGL framework with a legacy (2.1) context,
// whose API is a superset of what the renderer needs. GLSL ES 1.00 shaders are
// translated to GLSL 1.20 at compile time (see glb::adapt_shader_source).
//
// Define CANNONBALL_USE_GLES on Apple platforms to use a real GLES2 driver
// instead (e.g. ANGLE's libEGL/libGLESv2, which SDL can load on macOS).

#include <SDL.h>
#include <string>

#if defined(__APPLE__) && !defined(CANNONBALL_USE_GLES)
    #define CANNONBALL_DESKTOP_GL 1
    #ifndef GL_SILENCE_DEPRECATION
        #define GL_SILENCE_DEPRECATION 1
    #endif
    #include <OpenGL/gl.h>
    #include <OpenGL/glext.h>
#else
    #define CANNONBALL_DESKTOP_GL 0
    #include <SDL_opengles2.h>
#endif

namespace glb {

// Set SDL GL attributes for the context type this build renders with.
// Must be called before SDL_CreateWindow / SDL_GL_CreateContext.
inline void request_context_attributes()
{
#if CANNONBALL_DESKTOP_GL
    // A legacy (compatibility) profile keeps GL_LUMINANCE, client-side vertex
    // arrays and GLSL 1.20 (attribute/varying/gl_FragColor) available.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");   // tell SDL to prefer ANGLE/GLES
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
}

// Convert GLSL ES 1.00 source to something the active driver accepts.
// On GLES this is a no-op. On desktop GL it drops any #version line and
// precision statements and prepends a GLSL 1.20 prelude that defines the
// precision qualifiers away.
inline std::string adapt_shader_source(const char* src)
{
    if (!src) return {};
#if CANNONBALL_DESKTOP_GL
    std::string out =
        "#version 120\n"
        "#define lowp\n"
        "#define mediump\n"
        "#define highp\n";
    const char* p = src;
    while (*p) {
        const char* eol = p;
        while (*eol && *eol != '\n') ++eol;
        std::string line(p, eol);

        size_t first = line.find_first_not_of(" \t\r");
        bool drop = false;
        if (first != std::string::npos) {
            if (line.compare(first, 8, "#version") == 0)
                drop = true;
            else if (line.compare(first, 9, "precision") == 0 &&
                     (first + 9 >= line.size() || line[first + 9] == ' ' || line[first + 9] == '\t'))
                drop = true;
        }
        // Keep line numbering stable for compile errors.
        if (!drop) out += line;
        out += '\n';
        p = *eol ? eol + 1 : eol;
    }
    return out;
#else
    return std::string(src);
#endif
}

} // namespace glb
