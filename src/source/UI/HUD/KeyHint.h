#pragma once

#include "Core/Input/KeyBindings.h"

#include <string>

// Texts that name a key ("Character (C)") follow the player's key bindings (Core/Input/KeyBindingsRuntime.h).
namespace UI::KeyHint
{
// The key bound to the action, as a label: L"C", L"7", L"Home".
[[nodiscard]] std::wstring KeyLabel(Core::Input::KeyBindings::Action action);

// The text with its key hint replaced by the bound key(s): one trailing parenthesised group (ASCII or full-width
// brackets) is dropped and " (KEY)" or " (KEY, KEY2)" appended; a text without such a group just gets the hint.
// With the action(s) on their default keys the text comes back unchanged.
[[nodiscard]] std::wstring WithKey(const wchar_t* text, Core::Input::KeyBindings::Action action,
                                   Core::Input::KeyBindings::Action alt = Core::Input::KeyBindings::Action::None);

// A window's close tooltip ("Close (C)", I18N::Game::CloseS) with the key bound to the action that opens it.
[[nodiscard]] std::wstring CloseTooltip(Core::Input::KeyBindings::Action action);

// Tells a window when the key labels it built are stale: true on the first call and after every binding change.
struct LabelWatch
{
    [[nodiscard]] bool Changed();

private:
    bool m_built = false;
    unsigned m_revision = 0;
};
} // namespace UI::KeyHint
