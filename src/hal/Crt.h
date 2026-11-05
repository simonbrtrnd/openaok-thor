#pragma once

#include <SDL2/SDL.h>

namespace HAL::Crt {

// A CRT monitor's look for the frame as it is shown: Timothy Lottes's public
// domain scan-line shader (crt-lottes), with its scan lines, shadow mask and
// bloom, the screen kept flat so that a touch lands where it looks. It runs
// on OpenGL ES 2, beside SDL's own renderer (Android); elsewhere it is never
// available.

// Whether the look is wanted (Profile::Options::crt_filter); on at start.
void SetWanted(bool wanted);

// Draws `frame`, `frame_w` by `frame_h` pixels, into `dest` of the window's
// drawable, `drawable_h` pixels high, through the shader. False when it could
// not, the shader being unavailable, and the frame must be shown as it is.
bool Present(SDL_Renderer* renderer, SDL_Texture* frame, int frame_w, int frame_h, const SDL_Rect& dest,
             int drawable_h);

} // namespace HAL::Crt
