#pragma once

#include <string>

namespace UI::Combat::HealthBar
{
    bool ShouldRenderSelected(float healthStatus);

    // The selected monster's health as the server sends it (1/250 steps; below 0 = not known yet), for the text next
    // to its name: "37%", one decimal below 10 % ("4.8%"); empty when it is not known or 0.
    std::wstring PercentText(float healthStatus);
}
