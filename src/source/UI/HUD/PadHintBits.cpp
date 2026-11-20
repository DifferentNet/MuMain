#include "UI/HUD/PadHintBits.h"

namespace UI::HUD::PadHints
{
namespace
{
int Bit(Hint hint)
{
    return 1 << static_cast<int>(hint);
}
} // namespace

bool Seen(int bits, Hint hint)
{
    return (bits & Bit(hint)) != 0;
}

int Marked(int bits, Hint hint)
{
    return bits | Bit(hint);
}

bool SmallScreenApplies(int windowHeight, int uiScalePercent, bool phoneView)
{
    return windowHeight >= 1 && windowHeight <= SmallScreenHeight && uiScalePercent <= SmallScreenMostScale
           && !phoneView;
}
} // namespace UI::HUD::PadHints
