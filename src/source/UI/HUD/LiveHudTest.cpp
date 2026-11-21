#include "UI/HUD/LiveHudTest.h"

#include <cmath>
#include <cwchar>
#include <string>

namespace UI::HUD::LiveHudTest
{
namespace
{
// The value after a gauge's or the cooldown's name: a finite number, nothing after it but spaces.
std::optional<float> ParseNumber(std::wstring_view text)
{
    const std::wstring copy(text);
    const wchar_t* begin = copy.c_str();
    wchar_t* end = nullptr;
    const float value = std::wcstof(begin, &end);
    if (end == begin || !std::isfinite(value))
        return std::nullopt;
    while (*end == L' ')
        ++end;
    if (*end != L'\0')
        return std::nullopt;
    return value;
}

std::wstring_view Trimmed(std::wstring_view text)
{
    const std::size_t first = text.find_first_not_of(L' ');
    if (first == std::wstring_view::npos)
        return {};
    const std::size_t last = text.find_last_not_of(L' ');
    return text.substr(first, last - first + 1);
}

std::optional<float>* GaugeByName(Overrides& overrides, std::wstring_view name)
{
    if (name == L"hp")
        return &overrides.life;
    if (name == L"mp")
        return &overrides.mana;
    if (name == L"sd")
        return &overrides.shield;
    if (name == L"ag")
        return &overrides.skillMana;
    return nullptr;
}
} // namespace

Overrides& Current()
{
    static Overrides overrides;
    return overrides;
}

bool Apply(Overrides& overrides, std::wstring_view arguments)
{
    const std::wstring_view text = Trimmed(arguments);
    const std::size_t space = text.find(L' ');
    const std::wstring_view name = text.substr(0, space);
    const std::wstring_view value =
        space == std::wstring_view::npos ? std::wstring_view() : Trimmed(text.substr(space));

    if (value.empty())
    {
        if (name == L"off")
            overrides = {};
        else if (name == L"exp")
            ++overrides.expGains;
        else if (name == L"lvl")
            ++overrides.levelUps;
        else
            return false;
        return true;
    }

    if (name == L"poison")
    {
        if (value != L"on" && value != L"off")
            return false;
        overrides.poisoned = value == L"on";
        return true;
    }

    const std::optional<float> number = ParseNumber(value);
    if (!number)
        return false;
    if (name == L"cd")
    {
        if (*number < 0.f)
            return false;
        overrides.cooldownSeconds = *number;
        return true;
    }
    std::optional<float>* gauge = GaugeByName(overrides, name);
    if (gauge == nullptr || *number < 0.f || *number > 1.f)
        return false;
    *gauge = *number;
    return true;
}
} // namespace UI::HUD::LiveHudTest
