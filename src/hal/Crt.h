#pragma once

#include <SDL2/SDL.h>

namespace HAL::Crt {

// A CRT monitor's look for both screens: Timothy Lottes's public domain
// scan-line shader (crt-lottes), with its scan lines, shadow mask and bloom,
// the screen kept flat so that a touch lands where it looks. It runs on
// OpenGL ES 2, beside SDL's own renderer (Android); elsewhere it is never
// available and the pictures are shown as they are.
//
// Each mode is a monitor whose scan lines are some screen pixels tall, a
// coarser one in each: Soft 1.5, Arcade 2, TV 3. Its intensity mixes the
// monitor's picture with the plain one.
enum Mode : int { kOff, kSoft, kArcade, kTv, kModeCount };
constexpr int kIntensityStep = 10;

void Set(int mode, int intensity_percent);
int CurrentMode();
int Intensity();
const char* ModeName(int mode);

// The controller's quick settings (hal/Pad): the next or the previous mode,
// and the intensity a step up or down. When the last of these was made, for
// the interface to show the setting a moment (0 for never), and whether one
// has been made since the last call, for the profile to keep it.
void StepMode(int by);
void StepIntensity(int by);
Uint32 ChangedMs();
bool TakeChange();

// Draws `frame`, `frame_w` by `frame_h` pixels, into `dest` of the window's
// drawable, `drawable_h` pixels high, through the shader. False when it did
// not, the mode being Off or the shader unavailable: the frame is then to be
// shown as it is.
bool Present(SDL_Renderer* renderer, SDL_Texture* frame, int frame_w, int frame_h, const SDL_Rect& dest,
             int drawable_h);

// Draws `source` through the shader into `target`, a render target of the
// same `w` by `h`, for a picture copied out rather than shown (the second
// screen's). False when it did not; `target` is then untouched.
bool Apply(SDL_Renderer* renderer, SDL_Texture* source, SDL_Texture* target, int w, int h);

} // namespace HAL::Crt
