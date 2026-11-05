#pragma once

namespace ECS {
class Simulation;
}

namespace Rendering {
class Renderer;
}

namespace Rendering::BottomPanel {

// The picture of the second screen (hal/SecondScreen), where the battlefield
// interface goes on a device that has one (UIRenderer::DrawSecondScreen).
// Outside a game, in the movies and the menus, the screen stays black.
void Install();

// The game the picture is of, while its frame is being shown; none outside
// a game.
void SetGame(ECS::Simulation* sim, Renderer* renderer);

} // namespace Rendering::BottomPanel
