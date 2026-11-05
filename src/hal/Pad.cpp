#include "hal/Pad.h"

#include <algorithm>
#include <cmath>

#include "hal/Pointer.h"
#include "hal/SecondScreen.h"
#include "hal/TextInput.h"
#include "hal/Window.h"

#ifdef __SWITCH__
// SDL's own way in for a key (events/SDL_keyboard_c.h), which its Switch port
// uses for a USB keyboard. SDL is linked in whole on the Switch.
extern "C" int SDL_SendKeyboardKey(Uint8 state, SDL_Scancode scancode);
#endif

#ifdef __ANDROID__
#include <android/keycodes.h>
#include <jni.h>
// SDL's own way in for a key on Android (SDL_android.c): the calls a keyboard
// plugged into the phone makes. The JNI arguments go unused.
extern "C" JNIEXPORT void JNICALL Java_org_libsdl_app_SDLActivity_onNativeKeyDown(
    JNIEnv* env, jclass cls, jint keycode);
extern "C" JNIEXPORT void JNICALL Java_org_libsdl_app_SDLActivity_onNativeKeyUp(
    JNIEnv* env, jclass cls, jint keycode);
#endif

namespace HAL::Pad {

namespace {

// The buttons as SDL's Switch port numbers them (joystick/switch).
enum Button : int {
    kA = 0,
    kB = 1,
    kX = 2,
    kY = 3,
    kL = 6,
    kR = 7,
    kZL = 8,
    kZR = 9,
    kPlus = 10,
    kMinus = 11,
    kLeft = 12,
    kUp = 13,
    kRight = 14,
    kDown = 15,
};

// How far a stick leans before it counts, of its full reach: a stick at rest
// is seldom quite at its middle.
constexpr float kDeadZone = 0.15f;
// The pointer's speed with the stick pushed all the way, in screen heights a
// second.
constexpr float kPointerSpeed = 1.4f;
constexpr Uint32 kRumbleMs = 30;

struct State {
    bool enabled = false;
    bool opened = false;
    SDL_Joystick* joystick = nullptr;
    SDL_GameController* controller = nullptr;   // Android: a gamepad by SDL's standard layout
    float x = -1.0f;   // the pointer, window pixels; below zero until placed
    float y = -1.0f;
    bool zl = false;   // held: the right stick zooms
    Uint64 last = 0;   // SDL's performance counter at the last Update
};
State g;

#ifdef __ANDROID__
// Android's key for each of the game's keys a button presses.
int AndroidKey(SDL_Scancode key) {
    switch (key) {
    case SDL_SCANCODE_PERIOD: return AKEYCODE_PERIOD;
    case SDL_SCANCODE_ESCAPE: return AKEYCODE_ESCAPE;
    case SDL_SCANCODE_H: return AKEYCODE_H;
    case SDL_SCANCODE_COMMA: return AKEYCODE_COMMA;
    case SDL_SCANCODE_LSHIFT: return AKEYCODE_SHIFT_LEFT;
    case SDL_SCANCODE_HOME: return AKEYCODE_MOVE_HOME;
    case SDL_SCANCODE_F10: return AKEYCODE_F10;
    case SDL_SCANCODE_RETURN: return AKEYCODE_ENTER;
    case SDL_SCANCODE_F3: return AKEYCODE_F3;
    case SDL_SCANCODE_SPACE: return AKEYCODE_SPACE;
    case SDL_SCANCODE_KP_MINUS: return AKEYCODE_NUMPAD_SUBTRACT;
    case SDL_SCANCODE_KP_PLUS: return AKEYCODE_NUMPAD_ADD;
    default: return AKEYCODE_UNKNOWN;
    }
}
#endif

void Key(SDL_Scancode key, bool down) {
#if defined(__SWITCH__)
    SDL_SendKeyboardKey(down ? SDL_PRESSED : SDL_RELEASED, key);
#elif defined(__ANDROID__)
    const int code = AndroidKey(key);
    if (code == AKEYCODE_UNKNOWN) return;
    if (down) {
        Java_org_libsdl_app_SDLActivity_onNativeKeyDown(nullptr, nullptr, code);
    } else {
        Java_org_libsdl_app_SDLActivity_onNativeKeyUp(nullptr, nullptr, code);
    }
#else
    (void)key;
    (void)down;
#endif
}

// The key a button presses, or SDL_SCANCODE_UNKNOWN.
SDL_Scancode KeyOf(int button) {
    switch (button) {
    case kX: return SDL_SCANCODE_PERIOD;
    case kY: return SDL_SCANCODE_ESCAPE;
    case kL: return SDL_SCANCODE_H;
    case kR: return SDL_SCANCODE_COMMA;
    case kZL: return SDL_SCANCODE_LSHIFT;
    case kZR: return SDL_SCANCODE_HOME;
    case kPlus: return SDL_SCANCODE_F10;
    case kMinus: return SDL_SCANCODE_RETURN;
    case kUp: return SDL_SCANCODE_F3;
    case kDown: return SDL_SCANCODE_SPACE;
    case kLeft: return SDL_SCANCODE_KP_MINUS;
    case kRight: return SDL_SCANCODE_KP_PLUS;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

#ifdef __ANDROID__
// The key a gamepad's button presses (SDL's standard layout, an Xbox
// controller's), or SDL_SCANCODE_UNKNOWN.
SDL_Scancode KeyOfGamepad(int button) {
    switch (button) {
    case SDL_CONTROLLER_BUTTON_X: return SDL_SCANCODE_F3;
    case SDL_CONTROLLER_BUTTON_Y: return SDL_SCANCODE_ESCAPE;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return SDL_SCANCODE_LSHIFT;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return SDL_SCANCODE_PERIOD;
    case SDL_CONTROLLER_BUTTON_START: return SDL_SCANCODE_F10;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return SDL_SCANCODE_SPACE;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return SDL_SCANCODE_HOME;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return SDL_SCANCODE_KP_PLUS;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return SDL_SCANCODE_KP_MINUS;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return SDL_SCANCODE_H;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return SDL_SCANCODE_COMMA;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

// The first gamepad SDL knows the layout of, now or once plugged in.
void OpenGamepad() {
    if (g.controller != nullptr) return;
    for (int i = 0; i < SDL_NumJoysticks() && g.controller == nullptr; ++i) {
        if (SDL_IsGameController(i)) g.controller = SDL_GameControllerOpen(i);
    }
}
#endif

void Open() {
    if (g.opened) return;
    g.opened = true;
#ifdef __ANDROID__
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0) OpenGamepad();
#else
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0 || SDL_NumJoysticks() < 1) return;
    g.joystick = SDL_JoystickOpen(0);
#endif
}

// An axis's position, -1 to 1; 0 with no controller.
float Axis(int axis) {
#ifdef __ANDROID__
    if (g.controller == nullptr) return 0.0f;
    return SDL_GameControllerGetAxis(g.controller, static_cast<SDL_GameControllerAxis>(axis)) / 32767.0f;
#else
    if (g.joystick == nullptr) return 0.0f;
    return SDL_JoystickGetAxis(g.joystick, axis) / 32767.0f;
#endif
}

// A stick's lean, -1 to 1 each way: nothing inside the dead zone, and beyond
// it the square of how far, so that a small push moves finely. The axes are
// numbered as on the Switch, which SDL's standard layout shares: 0 and 1 the
// left stick, 2 and 3 the right.
void Lean(int axis_x, int axis_y, float& x, float& y) {
    x = 0.0f;
    y = 0.0f;
    const float ax = Axis(axis_x);
    const float ay = Axis(axis_y);
    const float length = std::hypot(ax, ay);
    if (length <= kDeadZone) return;
    const float reach = std::min(1.0f, (length - kDeadZone) / (1.0f - kDeadZone));
    const float scale = reach * reach / length;
    x = std::clamp(ax * scale, -1.0f, 1.0f);
    y = std::clamp(ay * scale, -1.0f, 1.0f);
}

} // namespace

void Enable() { g.enabled = true; }

bool Enabled() { return g.enabled; }

bool GamepadConnected() { return g.controller != nullptr; }

bool Handle(const SDL_Event& event) {
#ifdef __ANDROID__
    if (!g.enabled) return false;
    if (event.type == SDL_CONTROLLERDEVICEADDED) {
        OpenGamepad();
        return false;
    }
    if (event.type != SDL_CONTROLLERBUTTONDOWN && event.type != SDL_CONTROLLERBUTTONUP) return false;
    const bool pressed = event.type == SDL_CONTROLLERBUTTONDOWN;
    const int pad_button = event.cbutton.button;
    if (pad_button == SDL_CONTROLLER_BUTTON_A || pad_button == SDL_CONTROLLER_BUTTON_B) {
        const Uint8 mouse = pad_button == SDL_CONTROLLER_BUTTON_A ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
        if (pressed) {
            Pointer::Press(mouse);
        } else {
            Pointer::Release(mouse);
        }
        return true;
    }
    // Back puts the interface over the map, or back on the second screen
    // (hal/SecondScreen), should that screen be no use.
    if (pad_button == SDL_CONTROLLER_BUTTON_BACK) {
        if (pressed) SecondScreen::ToggleInterface();
        return true;
    }
    const SDL_Scancode pad_key = KeyOfGamepad(pad_button);
    if (pad_key == SDL_SCANCODE_LSHIFT) {
        Key(pad_key, pressed);
    } else if (pad_key != SDL_SCANCODE_UNKNOWN && pressed) {
        Key(pad_key, true);
        Key(pad_key, false);
    }
    return true;
#endif
    if (!g.enabled || (event.type != SDL_JOYBUTTONDOWN && event.type != SDL_JOYBUTTONUP)) return false;
    const bool down = event.type == SDL_JOYBUTTONDOWN;
    const int button = event.jbutton.button;
    if (button == kA || button == kB) {
        const Uint8 mouse = button == kA ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
        if (down) {
            Pointer::Press(mouse);
        } else {
            Pointer::Release(mouse);
        }
        return true;
    }
    // On a screen with a name to type, - brings up the keyboard; elsewhere it
    // is Enter, which opens the chat line.
    if (button == kMinus && down && TextInput::ShowForFields()) return true;
    if (button == kZL) g.zl = down;
    const SDL_Scancode key = KeyOf(button);
    if (key == SDL_SCANCODE_UNKNOWN) return true;
    // Shift is held as long as ZL is; every other key is pressed and let go
    // as its button goes down.
    if (key == SDL_SCANCODE_LSHIFT) {
        Key(key, down);
    } else if (down) {
        Key(key, true);
        Key(key, false);
    }
    return true;
}

void Update() {
    if (!g.enabled) return;
    Open();
    const Uint64 now = SDL_GetPerformanceCounter();
    const float dt = g.last == 0 ? 0.0f
        : std::min(0.1f, static_cast<float>(now - g.last) / static_cast<float>(SDL_GetPerformanceFrequency()));
    g.last = now;
    Window* window = MainWindow();
    if (window == nullptr || window->GetSDLWindow() == nullptr) return;
    int w = 0, h = 0;
    SDL_GetWindowSize(window->GetSDLWindow(), &w, &h);
    if (w <= 0 || h <= 0) return;
    // The pointer starts in the middle of the screen, and goes on from wherever
    // a finger or a mouse has since put it.
    if (g.x < 0.0f) {
        g.x = w / 2.0f;
        g.y = h / 2.0f;
        Pointer::Move(g.x, g.y, false);
    }
    int mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);
    if (static_cast<int>(g.x) != mx || static_cast<int>(g.y) != my) {
        g.x = static_cast<float>(mx);
        g.y = static_cast<float>(my);
    }
    float lx = 0.0f, ly = 0.0f;
    Lean(0, 1, lx, ly);
    if (lx == 0.0f && ly == 0.0f) return;
    const float step = kPointerSpeed * static_cast<float>(h) * dt;
    g.x = std::clamp(g.x + lx * step, 0.0f, static_cast<float>(w - 1));
    g.y = std::clamp(g.y + ly * step, 0.0f, static_cast<float>(h - 1));
    Pointer::Move(g.x, g.y, false);
}

void Scroll(float& x, float& y) {
    x = 0.0f;
    y = 0.0f;
    if (g.enabled && !g.zl) Lean(2, 3, x, y);
}

float Zoom() {
#ifdef __ANDROID__
    // The right trigger zooms in and the left out, the further the faster.
    if (!g.enabled) return 0.0f;
    const float lean = Axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) - Axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    return std::abs(lean) <= kDeadZone ? 0.0f : lean;
#endif
    if (!g.enabled || !g.zl) return 0.0f;
    float x = 0.0f, y = 0.0f;
    Lean(2, 3, x, y);
    return -y;
}

void Rumble() {
    if (g.joystick != nullptr) SDL_JoystickRumble(g.joystick, 0x6000, 0x6000, kRumbleMs);
    if (g.controller != nullptr) SDL_GameControllerRumble(g.controller, 0x6000, 0x6000, kRumbleMs);
}

} // namespace HAL::Pad
