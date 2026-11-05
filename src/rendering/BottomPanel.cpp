#include "rendering/BottomPanel.h"

#include <string>
#include <vector>

#include "hal/GraphicCache.h"
#include "hal/SecondScreen.h"
#include "rendering/Text.h"

namespace Rendering::BottomPanel {

namespace {

// The 1024x768 interface art, and the plain stone behind its command grid,
// which the screen is paved with, twice its size.
constexpr uint32_t kPanelArt = 51121;
constexpr SDL_Rect kStone = {45, 628, 190, 120};
constexpr int kStoneScale = 2;

constexpr SDL_Color kParchment = {243, 231, 201, 255};
constexpr SDL_Color kShadow = {0, 0, 0, 255};

struct State {
    bool touched = false;
    float x = 0.0f;
    float y = 0.0f;
    std::vector<HAL::SecondScreen::Touch> touches;
};
State g;

void PaveWithStone(SDL_Renderer* r, int w, int h) {
    const HAL::GraphicAtlas* atlas = HAL::GraphicCache::Get().GetInterfaceAtlas(kPanelArt);
    if (atlas == nullptr || atlas->base == nullptr || atlas->frames.empty()) return;
    const SDL_Rect& art = atlas->frames[0].rect;
    const SDL_Rect src = {art.x + kStone.x, art.y + kStone.y, kStone.w, kStone.h};
    for (int y = 0; y < h; y += kStone.h * kStoneScale) {
        for (int x = 0; x < w; x += kStone.w * kStoneScale) {
            const SDL_Rect dst = {x, y, kStone.w * kStoneScale, kStone.h * kStoneScale};
            SDL_RenderCopy(r, atlas->base, &src, &dst);
        }
    }
}

void Centred(SDL_Renderer* r, int font, int w, int y, const std::string& text) {
    Text::DrawShadowed(r, font, (w - Text::Width(font, text)) / 2, y, text, kParchment, kShadow);
}

void Paint(SDL_Renderer* r, int w, int h) {
    g.touches.clear();
    HAL::SecondScreen::TakeTouches(g.touches);
    for (const HAL::SecondScreen::Touch& t : g.touches) {
        g.touched = true;
        g.x = t.x;
        g.y = t.y;
    }

    PaveWithStone(r, w, h);
    const int title = Text::Scaled(Text::kBlackletter21B, 300);
    const int line = Text::Scaled(Text::kGeorgia9B, 300);
    Centred(r, title, w, h / 3, "Age of Kings");
    Centred(r, line, w, h / 3 + Text::Height(title) + 24,
            std::to_string(w) + " x " + std::to_string(h));
    if (!g.touched) return;

    SDL_SetRenderDrawColor(r, kParchment.r, kParchment.g, kParchment.b, 255);
    const int x = static_cast<int>(g.x);
    const int y = static_cast<int>(g.y);
    const SDL_Rect across = {x - 40, y - 3, 80, 6};
    const SDL_Rect down = {x - 3, y - 40, 6, 80};
    SDL_RenderFillRect(r, &across);
    SDL_RenderFillRect(r, &down);
    Centred(r, line, w, h - Text::Height(line) - 40,
            "Touch " + std::to_string(x) + ", " + std::to_string(y));
}

} // namespace

void Install() { HAL::SecondScreen::SetPainter(Paint); }

} // namespace Rendering::BottomPanel
