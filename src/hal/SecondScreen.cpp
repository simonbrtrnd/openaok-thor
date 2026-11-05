#include "hal/SecondScreen.h"

#ifdef __ANDROID__
#include <android/input.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#endif

namespace HAL::SecondScreen {

#ifdef __ANDROID__

namespace {

// How often the picture is drawn again when nothing asks for it sooner.
constexpr Uint32 kRefreshMs = 66;
// Touches kept between two frames; moves are merged, so few are needed.
constexpr size_t kMaxPendingTouches = 64;

// What the window's thread hands over: the surface and the touches on it.
struct Shared {
    std::mutex lock;
    ANativeWindow* window = nullptr;
    int width = 0;
    int height = 0;
    bool new_window = false;
    std::vector<Touch> touches;
};
Shared shared;

// The game thread's own.
struct State {
    Painter painter;
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
    std::vector<Uint32> pixels;
    Uint32 last_drawn = 0;
    bool due = true;
    bool black = false;   // the screen shows black, the painter having had nothing
    std::vector<Touch> touches;
};
State g;

// The window, held, and whether anything happened to it since the last frame.
ANativeWindow* AcquireWindow(int& width, int& height, bool& is_new, bool& touched) {
    std::lock_guard<std::mutex> hold(shared.lock);
    if (shared.window != nullptr) ANativeWindow_acquire(shared.window);
    width = shared.width;
    height = shared.height;
    is_new = shared.new_window;
    shared.new_window = false;
    touched = !shared.touches.empty();
    g.touches.insert(g.touches.end(), shared.touches.begin(), shared.touches.end());
    shared.touches.clear();
    if (g.touches.size() > kMaxPendingTouches) {
        g.touches.erase(g.touches.begin(), g.touches.end() - kMaxPendingTouches);
    }
    return shared.window;
}

bool Resize(SDL_Renderer* renderer, int width, int height) {
    if (g.texture != nullptr && width == g.width && height == g.height) return true;
    if (g.texture != nullptr) SDL_DestroyTexture(g.texture);
    g.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, width, height);
    g.width = g.texture != nullptr ? width : 0;
    g.height = g.texture != nullptr ? height : 0;
    g.pixels.assign(static_cast<size_t>(g.width) * g.height, 0);
    return g.texture != nullptr;
}

// Draws the picture and reads it back; false when the painter had none.
bool Draw(SDL_Renderer* renderer) {
    SDL_Texture* previous = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, g.texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    const bool drawn = g.painter && g.painter(renderer, g.width, g.height);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    SDL_RenderSetClipRect(renderer, nullptr);
    const bool read = drawn && SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                                                    g.pixels.data(), g.width * 4) == 0;
    SDL_SetRenderTarget(renderer, previous);
    return read;
}

// Copies the picture to the window, or fills it with black.
void Show(ANativeWindow* window, bool picture) {
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(window, &buffer, nullptr) != 0) return;
    auto* rows = static_cast<Uint32*>(buffer.bits);
    if (picture && buffer.width == g.width && buffer.height == g.height) {
        for (int y = 0; y < g.height; ++y) {
            std::memcpy(rows + static_cast<size_t>(y) * buffer.stride,
                        g.pixels.data() + static_cast<size_t>(y) * g.width, g.width * sizeof(Uint32));
        }
    } else {
        for (int y = 0; y < buffer.height; ++y) {
            std::fill_n(rows + static_cast<size_t>(y) * buffer.stride, buffer.width, 0xff000000u);
        }
    }
    ANativeWindow_unlockAndPost(window);
}

} // namespace

bool Active() {
    std::lock_guard<std::mutex> hold(shared.lock);
    return shared.window != nullptr;
}

int Width() { return g.width; }
int Height() { return g.height; }

void SetPainter(Painter painter) {
    g.painter = std::move(painter);
    g.due = true;
}

void Invalidate() { g.due = true; }

void Present(SDL_Renderer* renderer) {
    int width = 0, height = 0;
    bool is_new = false, touched = false;
    ANativeWindow* window = AcquireWindow(width, height, is_new, touched);
    if (window == nullptr) return;
    const Uint32 now = SDL_GetTicks();
    const bool due = g.due || is_new || touched || now - g.last_drawn >= kRefreshMs;
    if (due && width > 0 && height > 0 && Resize(renderer, width, height)) {
        const bool picture = Draw(renderer);
        if (picture || !g.black || is_new) Show(window, picture);
        g.black = !picture;
        g.due = false;
        g.last_drawn = now;
    }
    ANativeWindow_release(window);
}

void TakeTouches(std::vector<Touch>& out) {
    out.insert(out.end(), g.touches.begin(), g.touches.end());
    g.touches.clear();
}

#else

bool Active() { return false; }
int Width() { return 0; }
int Height() { return 0; }
void SetPainter(Painter) {}
void Invalidate() {}
void Present(SDL_Renderer*) {}
void TakeTouches(std::vector<Touch>&) {}

#endif

} // namespace HAL::SecondScreen

#ifdef __ANDROID__

// BottomScreen.java, as its surface comes, changes size or goes.
extern "C" JNIEXPORT void JNICALL Java_org_openaok_game_BottomScreen_nativeSetSurface(
    JNIEnv* env, jclass, jobject surface, jint width, jint height) {
    using namespace HAL::SecondScreen;
    ANativeWindow* window = surface != nullptr ? ANativeWindow_fromSurface(env, surface) : nullptr;
    if (window != nullptr) ANativeWindow_setBuffersGeometry(window, width, height, WINDOW_FORMAT_RGBA_8888);
    std::lock_guard<std::mutex> hold(shared.lock);
    if (shared.window != nullptr) ANativeWindow_release(shared.window);
    shared.window = window;
    shared.width = window != nullptr ? width : 0;
    shared.height = window != nullptr ? height : 0;
    shared.new_window = true;
    shared.touches.clear();
}

// BottomScreen.java, for each touch: Android's action and where, from 0 to 1
// across the screen.
extern "C" JNIEXPORT void JNICALL Java_org_openaok_game_BottomScreen_nativeTouch(
    JNIEnv*, jclass, jint action, jfloat x, jfloat y) {
    using namespace HAL::SecondScreen;
    Touch touch;
    switch (action) {
    case AMOTION_EVENT_ACTION_DOWN: touch.kind = Touch::Down; break;
    case AMOTION_EVENT_ACTION_MOVE: touch.kind = Touch::Move; break;
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_CANCEL: touch.kind = Touch::Up; break;
    default: return;
    }
    std::lock_guard<std::mutex> hold(shared.lock);
    touch.x = x * static_cast<float>(shared.width);
    touch.y = y * static_cast<float>(shared.height);
    if (touch.kind == Touch::Move && !shared.touches.empty() && shared.touches.back().kind == Touch::Move) {
        shared.touches.back() = touch;
    } else if (shared.touches.size() < kMaxPendingTouches) {
        shared.touches.push_back(touch);
    }
}

#endif
