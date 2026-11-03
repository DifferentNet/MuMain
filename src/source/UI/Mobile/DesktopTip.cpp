#include "UI/Mobile/DesktopTip.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <string>

namespace UI::Mobile::DesktopTip
{
namespace
{
// Lower case. "click" covers "clicking", "key" covers "hotkey" and "shortcut key". The client also ships
// Portuguese and Spanish tips (slide_por/slide_spn): "press" already covers "pressione", the rest are
// plain ASCII roots so the accented letters don't matter.
constexpr std::array<std::wstring_view, 13> MouseOrKeyWords = {
    L"click", L"press", L"key", L"ctrl", L"alt+", L"cursor", L"space bar", L"print screen",
    L"clic",    // es "haz clic", pt "clicando"
    L"cliqu",   // pt "clique"
    L"presion", // es "presiona"
    L"tecla",   // pt/es "key"
    L"barra de espa", // pt "barra de espa\u00e7o", es "barra espaciadora" is caught by "presion"
};
} // namespace

bool NeedsMouseOrKeys(std::wstring_view tip)
{
    std::wstring lower(tip);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return std::any_of(MouseOrKeyWords.begin(), MouseOrKeyWords.end(),
                       [&lower](std::wstring_view word) { return lower.find(word) != std::wstring::npos; });
}
} // namespace UI::Mobile::DesktopTip
