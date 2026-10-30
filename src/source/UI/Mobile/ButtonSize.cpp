#include "UI/Mobile/ButtonSize.h"

#include <algorithm>
#include <cstdlib>

namespace
{
// A phone held sideways is 360-430 CSS pixels tall: on up to this short side a unit is one CSS pixel,
// so the attack button (84 units) is about 13 mm and a skill (50 units) about 8 mm.
constexpr float PhoneShortSide = 412.f;
// A tablet's buttons grow at most this much: a thumb is no wider there.
constexpr float MaxScreenGrowth = 1.15f;
// The height the controls need, in units: the top row and the hero frame over the potions, the
// potions over the joystick.
constexpr float ColumnUnits = 345.f;
} // namespace

int UI::Mobile::ButtonSize::NearestIndex(int percent)
{
    int best = 0;
    for (int index = 1; index < static_cast<int>(Percents.size()); ++index)
    {
        if (std::abs(Percents[index] - percent) < std::abs(Percents[best] - percent))
            best = index;
    }
    return best;
}

float UI::Mobile::ButtonSize::UnitCssPixels(float areaWidth, float areaHeight, int percent)
{
    if (areaWidth <= 0.f || areaHeight <= 0.f)
        return 1.f;
    const float shortSide = std::min(areaWidth, areaHeight);
    const float growth = std::clamp(shortSide / PhoneShortSide, 1.f, MaxScreenGrowth);
    const float wanted = growth * static_cast<float>(percent) / 100.f;
    return std::min(wanted, areaHeight / ColumnUnits);
}
