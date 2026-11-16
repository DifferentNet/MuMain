#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// One action's row: its name, its key ("I", "Home"; "..." while it waits for a key), its place in
// Core::Input::KeyBindings::Action (the click carries it back), whether it waits for a key and whether it just
// took the other action's key in a swap (flashes for a moment).
struct KeyBindingsRow
{
    Rml::String label;
    Rml::String keyText;
    int index = 0;
    // The row element's id ("kb_row_<index>"), for scrolling it into view.
    Rml::String id;
    bool capturing = false;
    bool flash = false;

    bool operator==(const KeyBindingsRow&) const = default;
};

// The window's own texts, in the client's language.
struct KeyBindingsLabels
{
    Rml::String windows;
    Rml::String potions;
    Rml::String skills;
    Rml::String reset;
    Rml::String close;

    bool operator==(const KeyBindingsLabels&) const = default;
};

struct KeyBindingsRmlModel
{
    // window_shell.rml's: the title over the frame; the window is never placed by C++ (centred, the player drags it).
    bool hasTitle = true;
    Rml::String title;
    bool positioned = false;
    float rootX = 0.f;
    float rootY = 0.f;

    KeyBindingsLabels labels;
    std::vector<KeyBindingsRow> windows;
    std::vector<KeyBindingsRow> potions;
    std::vector<KeyBindingsRow> skills;
    // The line under the rows: how it works, "press a key" while waiting, or why the key pressed was refused.
    Rml::String hint;
    // The hint shows a refusal (drawn as a warning).
    bool refused = false;
};
} // namespace mu::ui::window
