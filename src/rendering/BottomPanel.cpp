#include "rendering/BottomPanel.h"

#include "hal/SecondScreen.h"
#include "rendering/UIRenderer.h"

namespace Rendering::BottomPanel {

namespace {

struct State {
    ECS::Simulation* sim = nullptr;
    Renderer* renderer = nullptr;
};
State g;

bool Paint(SDL_Renderer* r, int w, int h) {
    if (g.sim == nullptr || g.renderer == nullptr) return false;
    UIRenderer::DrawSecondScreen(*g.sim, *g.renderer, r, w, h);
    return true;
}

} // namespace

void Install() { HAL::SecondScreen::SetPainter(Paint); }

void SetGame(ECS::Simulation* sim, Renderer* renderer) {
    g.sim = sim;
    g.renderer = renderer;
}

} // namespace Rendering::BottomPanel
