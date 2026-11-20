#include "Core/Input/ArrowSteer.h"

#include <cmath>

namespace Core::Input::ArrowSteer
{
Direction FromKeys(bool left, bool right, bool up, bool down)
{
    Direction direction;
    direction.x = static_cast<float>(static_cast<int>(right) - static_cast<int>(left));
    direction.y = static_cast<float>(static_cast<int>(down) - static_cast<int>(up));
    if (direction.x != 0.f && direction.y != 0.f)
    {
        const float scale = 1.f / std::sqrt(2.f);
        direction.x *= scale;
        direction.y *= scale;
    }
    return direction;
}

bool Any(Direction direction)
{
    return direction.x != 0.f || direction.y != 0.f;
}
} // namespace Core::Input::ArrowSteer
