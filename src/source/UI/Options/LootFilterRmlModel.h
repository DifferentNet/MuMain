#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// A row of the item types: its name, its place in LootFilter::Category (the click carries it back), its action
// (LootFilter::Verdict: 0 normal, 1 hide, 2 highlight) and whether Hide may be picked for it.
struct LootFilterCategoryRow
{
    Rml::String label;
    int index = 0;
    int verdict = 0;
    bool canHide = true;

    bool operator==(const LootFilterCategoryRow&) const = default;
};

// A row of the "always highlight" options: its name, its bit's place in LootFilter::RarityFlag and whether it is on.
struct LootFilterRarityRow
{
    Rml::String label;
    int index = 0;
    bool on = false;

    bool operator==(const LootFilterRarityRow&) const = default;
};

// A name the player highlights, with its place in the list for the remove click.
struct LootFilterNameChip
{
    Rml::String text;
    int index = 0;

    bool operator==(const LootFilterNameChip&) const = default;
};

// The window's own texts, in the client's language.
struct LootFilterLabels
{
    Rml::String enabled;
    Rml::String presets;
    Rml::String presetDefault;
    Rml::String presetStrict;
    Rml::String presetOff;
    Rml::String hide;
    Rml::String normal;
    Rml::String highlight;
    Rml::String categories;
    Rml::String rarity;
    Rml::String level;
    Rml::String zen;
    Rml::String names;
    Rml::String nameHint;
    Rml::String nameAdd;
    Rml::String marker;
    Rml::String sound;
    Rml::String close;

    bool operator==(const LootFilterLabels&) const = default;
};

struct LootFilterRmlModel
{
    // window_shell.rml's: the title over the frame; the window is never placed by C++ (centred, the player drags it).
    bool hasTitle = true;
    Rml::String title;
    bool positioned = false;
    float rootX = 0.f;
    float rootY = 0.f;

    LootFilterLabels labels;
    bool enabled = true;
    bool marker = true;
    bool sound = false;
    std::vector<LootFilterCategoryRow> categories;
    std::vector<LootFilterRarityRow> rarities;
    // The level threshold ("+9") and the zen threshold ("1,000,000" or "off").
    Rml::String levelText;
    Rml::String zenText;
    std::vector<LootFilterNameChip> names;
    // What the name field holds (bound both ways), and whether Add can take it.
    Rml::String nameInput;
    bool canAddName = false;
};
} // namespace mu::ui::window
