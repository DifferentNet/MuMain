#include "stdafx.h"
#include "UI/HUD/KeyHint.h"

#include "Core/Input/KeyBindingsRuntime.h"

#include <string_view>

namespace UI::KeyHint
{
namespace
{
namespace KeyBindings = Core::Input::KeyBindings;

constexpr wchar_t OpenAscii = L'(';
constexpr wchar_t CloseAscii = L')';
constexpr wchar_t OpenFullWidth = L'\xFF08';
constexpr wchar_t CloseFullWidth = L'\xFF09';

// The text without its trailing "(...)" group and the spaces before it; the text itself when it has none.
std::wstring_view WithoutKeyGroup(std::wstring_view text)
{
    if (text.empty())
        return text;

    const wchar_t last = text.back();
    wchar_t open = 0;
    if (last == CloseAscii)
        open = OpenAscii;
    else if (last == CloseFullWidth)
        open = OpenFullWidth;
    else
        return text;

    const size_t start = text.rfind(open);
    if (start == std::wstring_view::npos)
        return text;

    std::wstring_view kept = text.substr(0, start);
    while (!kept.empty() && kept.back() == L' ')
        kept.remove_suffix(1);
    return kept;
}

bool IsOnDefaultKey(KeyBindings::Action action)
{
    return KeyBindings::Runtime::KeyOf(action) == KeyBindings::InfoOf(action).defaultKey;
}
} // namespace

std::wstring KeyLabel(KeyBindings::Action action)
{
    const std::string name = KeyBindings::KeyName(KeyBindings::Runtime::KeyOf(action));
    return std::wstring(name.begin(), name.end());
}

std::wstring WithKey(const wchar_t* text, KeyBindings::Action action, KeyBindings::Action alt)
{
    if (text == nullptr)
        return {};
    // On the default keys the translation's own hint stays, as it is written there.
    if (IsOnDefaultKey(action) && (alt == KeyBindings::Action::None || IsOnDefaultKey(alt)))
        return text;

    std::wstring result(WithoutKeyGroup(text));
    result += L" (";
    result += KeyLabel(action);
    if (alt != KeyBindings::Action::None)
    {
        result += L", ";
        result += KeyLabel(alt);
    }
    result += L")";
    return result;
}
} // namespace UI::KeyHint
