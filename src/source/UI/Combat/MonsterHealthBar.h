#pragma once

#include <string>

namespace UI::Combat::HealthBar
{
    bool ShouldRenderSelected(float healthStatus);

    // The selected monster's health as the server sends it (1/250 steps; below 0 = not known yet), for the text next
    // to its name: "37%", one decimal below 10 % ("4.8%"); empty when it is not known or 0.
    std::wstring PercentText(float healthStatus);

    // The selected monster's name line: "Name  37%", or the name alone while PercentText() is empty. Built again only
    // when the name or the health differs from the last call's, so a selected monster costs no string work each frame.
    // The reference stays valid until the next call.
    const std::wstring& NameLine(const wchar_t* name, float healthStatus);
}
