#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// A line of Kundun's Lair window's own text: what it says and the size the native renderer would have shrunk it to
// for its box. Where it sits and its colour are the theme's.
struct SoloKalimaLine
{
    Rml::String text;
    float textPx = 0.f;

    bool operator==(const SoloKalimaLine&) const = default;
};

// A tier: its number and name with the Combat Power it needs in thousands at the right; only the picked one also has
// its Kalima line and the detail line (the Combat Power and HP it needs, the character's best time). Whether the character may go in, and
// whether it is the one picked.
struct SoloKalimaRow
{
    int tier = 0;
    SoloKalimaLine title;
    SoloKalimaLine cp;
    SoloKalimaLine kalima;
    SoloKalimaLine detail;
    bool open = false;
    bool selected = false;

    bool operator==(const SoloKalimaRow&) const = default;
};

// A curse: a checkbox with its name and what it does (the whole text also as the row's hint); locked while as many
// curses as the server allows are picked.
struct SoloKalimaCurse
{
    int index = 0;
    SoloKalimaLine text;
    Rml::String hint;
    bool checked = false;
    bool locked = false;

    bool operator==(const SoloKalimaCurse&) const = default;
};

struct SoloKalimaRmlModel
{
    SoloKalimaLine title;
    // The character's counted Combat Power and HP, the Mirrors of Dimensions, the runs left and when they come back.
    SoloKalimaLine power;
    SoloKalimaLine ticket;
    bool ticketOk = false;
    SoloKalimaLine runs;
    bool runsOk = false;
    std::vector<SoloKalimaRow> tiers;
    // The picked tier's server record and whose it is.
    SoloKalimaLine record;
    SoloKalimaLine cursesTitle;
    std::vector<SoloKalimaCurse> curses;
    // Why the button is off, or the server's answer to the last Enter.
    SoloKalimaLine hint;
    bool hintBad = false;
    bool canEnter = false;
    SoloKalimaLine enterLabel;

    Rml::String exitTooltip;
};
} // namespace mu::ui::window
