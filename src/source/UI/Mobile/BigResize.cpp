#include "UI/Mobile/BigResize.h"

#include <cstdlib>

namespace UI::Mobile
{
bool IsBigResize(int oldWidth, int oldHeight, int newWidth, int newHeight)
{
    if (oldWidth <= 0 || oldHeight <= 0 || newWidth <= 0 || newHeight <= 0)
        return true;
    if ((oldWidth > oldHeight) != (newWidth > newHeight))
        return true;
    // More than a fifth of a side.
    const auto bigChange = [](int before, int after) { return std::abs(after - before) * 5 > before; };
    return bigChange(oldWidth, newWidth) || bigChange(oldHeight, newHeight);
}
} // namespace UI::Mobile
