#include "UI/Mobile/ButtonSize.h"

#include <algorithm>
#include <cstdlib>

namespace
{
// A phone held sideways is 360-430 CSS pixels tall: on up to this short side a unit is one CSS pixel,
// so the attack button (72 units) is about 11 mm and a skill (44 units) about 7 mm.
constexpr float PhoneShortSide = 412.f;
// A tablet's buttons grow at most this much: a thumb is no wider there.
constexpr float MaxScreenGrowth = 1.15f;
// The height the controls need, in units: the top row and the hero frame over the potions, the
// potions over the joystick, and the skills' arc with Pick up under the top row.
constexpr float ColumnUnits = 320.f;
// The mini map's corner the row keeps clear, in dp.
constexpr float MapCornerDp = 214.f;
// The width cap never takes the unit below this (a window too narrow for any row).
constexpr float MinWidthUnit = 0.5f;
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

float UI::Mobile::ButtonSize::UnitCssPixels(float areaWidth, float areaHeight, int percent, float topRowUnits,
                                            float dpCssPixels)
{
    if (areaWidth <= 0.f || areaHeight <= 0.f)
        return 1.f;
    const float shortSide = std::min(areaWidth, areaHeight);
    const float growth = std::clamp(shortSide / PhoneShortSide, 1.f, MaxScreenGrowth);
    const float wanted = growth * static_cast<float>(percent) / 100.f;
    const float heightCap = areaHeight / ColumnUnits;
    if (topRowUnits <= 0.f)
        return std::min(wanted, heightCap);
    // Below 100 % the row is drawn at its 100 % size (body.size-60..90), so its room shrinks with the
    // percent.
    const float rowScale = std::min(1.f, static_cast<float>(percent) / 100.f);
    const float dp = dpCssPixels > 0.f ? dpCssPixels : 1.f;
    const float widthCap = std::max(MinWidthUnit * rowScale, (areaWidth - MapCornerDp * dp) / topRowUnits * rowScale);
    return std::min({wanted, heightCap, widthCap});
}
