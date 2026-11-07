#include "stdafx.h"

#include "Render/PostProcessing/ScreenFilterSetting.h"

#include <algorithm>

#include "Core/Utilities/Log/MuLogger.h"
#include "Render/PostProcessing/PostProcessingSetting.h"

namespace Render::PostProcessing
{
namespace
{
ScreenFilters s_filters;
} // namespace

void SetScreenFilters(const ScreenFilters& filters)
{
    ScreenFilters clamped = filters;
    clamped.sharpen = std::clamp(filters.sharpen, 0.0f, MaxSharpen);
    if (clamped == s_filters)
    {
        return;
    }
    s_filters = clamped;
    mu::log::Get("render")->info("Screen filters: FXAA {}, sharpen {:.2f}", clamped.fxaa ? "on" : "off",
                                 clamped.sharpen);
    RefreshPostProcessing();
}

ScreenFilters GetScreenFilters()
{
    return s_filters;
}

bool ScreenFiltersOn()
{
    return s_filters.fxaa || s_filters.sharpen > 0.0f;
}
} // namespace Render::PostProcessing
