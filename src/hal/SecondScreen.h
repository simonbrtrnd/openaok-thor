#pragma once

#include <SDL2/SDL.h>
#include <functional>
#include <vector>

namespace HAL::SecondScreen {

// A second display, such as the AYN Thor's lower screen, on Android: a window
// of its own (android/.../BottomScreen.java). The game draws its picture into
// a texture of the screen's size and copies it into that window, when
// something has changed or a fifteenth of a second has passed. Elsewhere there
// is none, and every call here does nothing.

// A finger on the screen, in its pixels.
struct Touch {
    enum Kind { Down, Move, Up };
    Kind kind = Down;
    float x = 0.0f;
    float y = 0.0f;
};

// Whether the screen is there, and its size in pixels.
bool Active();

// Whether the player wants the interface on this screen rather than over the
// map (the controller's L1 and Start together switch it), as long as the
// screen is there. On at start.
bool InterfaceBelow();
void ToggleInterface();
int Width();
int Height();

// What draws the screen's picture into the current render target, `w` by `h`
// pixels, and says whether it drew one: with none, the screen is left black
// and nothing is read back. It is free to change the renderer's scale and
// clip, which are reset after it.
using Painter = std::function<bool(SDL_Renderer* renderer, int w, int h)>;
void SetPainter(Painter painter);

// The picture is drawn again at the next frame, rather than when it is due.
void Invalidate();

// Once a frame, from Window::Present, before the main frame is shown: draws
// the picture and copies it to the screen when due.
void Present(SDL_Renderer* renderer);

// The touches since the last call, oldest first.
void TakeTouches(std::vector<Touch>& out);

} // namespace HAL::SecondScreen
