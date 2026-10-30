#include "Core/Input/GamepadStick.h"

#include <algorithm>
#include <cmath>

namespace Core::Input::Gamepad
{
namespace
{
// A push starts this far past the dead zone (about 0.34 of the whole throw) and ends under the
// second share (about 0.29): a drifting stick rests below both.
constexpr float PushStartLean = 0.12f;
constexpr float PushEndLean = 0.05f;
} // namespace

StickPosition PastDeadZone(float x, float y)
{
    const float length = std::hypot(x, y);
    if (length <= StickDeadZone)
        return {};
    const float scaled = std::min(1.f, (length - StickDeadZone) / (1.f - StickDeadZone));
    return {x * scaled / length, y * scaled / length};
}

float Lean(StickPosition stick)
{
    return std::hypot(stick.x, stick.y);
}

bool IsPushed(StickPosition stick, bool wasPushed)
{
    return Lean(stick) >= (wasPushed ? PushEndLean : PushStartLean);
}
} // namespace Core::Input::Gamepad
