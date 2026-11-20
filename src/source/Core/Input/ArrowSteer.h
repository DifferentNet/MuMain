// The arrow keys' walk (pad round 7, C1): the way the arrows held point, on the screen (x right, y
// down, as the pad's stick). Opposite keys cancel each other; a diagonal is as long as a straight line,
// so the hero walks as fast either way.
#pragma once

namespace Core::Input::ArrowSteer
{
struct Direction
{
    float x = 0.f;
    float y = 0.f;
};

// The arrows held now: (0, 0) with none, or with only opposite ones.
[[nodiscard]] Direction FromKeys(bool left, bool right, bool up, bool down);

// Whether the direction walks at all.
[[nodiscard]] bool Any(Direction direction);
} // namespace Core::Input::ArrowSteer
