// Web build only: see WebDisplay.h.

#include "Core/Platform/web/WebDisplay.h"

#include <emscripten.h>

namespace
{
// index.html's muDisplay: serial, or -1 without a size to follow ("Auto" off, or the page not laid
// out yet).
EM_JS(int, MuWeb_DisplaySerial, (), {
    const display = window.muDisplay;
    return display && display.auto && display.width > 0 ? display.serial : -1;
});

EM_JS(int, MuWeb_DisplayAuto, (), { return window.muDisplay && window.muDisplay.auto ? 1 : 0; });
EM_JS(int, MuWeb_DisplayWidth, (), { return window.muDisplay.width; });
EM_JS(int, MuWeb_DisplayHeight, (), { return window.muDisplay.height; });
EM_JS(double, MuWeb_DisplayRatio, (), { return window.muDisplay.ratio; });

EM_JS(void, MuWeb_SetDisplayAuto, (int enabled), {
    if (window.muDisplay)
    {
        window.muDisplay.setAuto(!!enabled);
    }
});

// (Loose ==: clang-format splits "===".)
EM_JS(int, MuWeb_DisplayLowest, (), { return window.muDisplay && window.muDisplay.quality == "lowest" ? 1 : 0; });
EM_JS(int, MuWeb_EffectsBeforeLowest, (), { return window.muDisplay ? window.muDisplay.effectsBeforeLowest : -1; });

EM_JS(void, MuWeb_SetEffectsBeforeLowest, (int bits), {
    if (window.muDisplay)
    {
        window.muDisplay.setEffectsBeforeLowest(bits);
    }
});
} // namespace

std::optional<mu::web::PageDisplay> mu::web::GetAutoDisplay()
{
    const int serial = MuWeb_DisplaySerial();
    if (serial < 0)
        return std::nullopt;

    PageDisplay display;
    display.serial = serial;
    display.width = MuWeb_DisplayWidth();
    display.height = MuWeb_DisplayHeight();
    display.pixelRatio = static_cast<float>(MuWeb_DisplayRatio());
    return display;
}

bool mu::web::IsAutoDisplay()
{
    return MuWeb_DisplayAuto() != 0;
}

void mu::web::SetAutoDisplay(bool enabled)
{
    MuWeb_SetDisplayAuto(enabled ? 1 : 0);
}

bool mu::web::IsLowestQuality()
{
    return MuWeb_DisplayLowest() != 0;
}

int mu::web::GetEffectsBeforeLowest()
{
    return MuWeb_EffectsBeforeLowest();
}

void mu::web::SetEffectsBeforeLowest(int bits)
{
    MuWeb_SetEffectsBeforeLowest(bits);
}
