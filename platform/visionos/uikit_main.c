// main() for UIKit platforms built on sdl2-compat (visionOS).
//
// With SDL2's headers, the game's main() is renamed SDL_main on UIKit
// platforms and SDL2main normally supplies the real main(). sdl2-compat has no
// UIKit SDL2main, so this does the same job: hand SDL_main to SDL3's UIKit
// application loop.
#define SDL_MAIN_HANDLED
#include <SDL.h>

extern int SDL_main(int argc, char* argv[]);

int main(int argc, char* argv[])
{
    return SDL_UIKitRunApp(argc, argv, SDL_main);
}
