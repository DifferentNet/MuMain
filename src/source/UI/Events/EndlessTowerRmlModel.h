#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// A line of the Endless Tower window's own text: what it says and the size the native renderer would have shrunk it
// to for its box. Where it sits and its colour are the theme's.
struct EndlessTowerLine
{
    Rml::String text;
    float textPx = 0.f;

    bool operator==(const EndlessTowerLine&) const = default;
};

// A tab of the window's pane: its label and whether it is the one shown.
struct EndlessTowerTab
{
    int tab = 0;
    EndlessTowerLine label;
    bool selected = false;

    bool operator==(const EndlessTowerTab&) const = default;
};

// A line of the shown tab. Its kind picks the look: 0 plain, 1 a section title, 2 good (green), 3 bad (red),
// 4 a top entry's name (bold), 5 a line that continues the one above (indented).
struct EndlessTowerRow
{
    EndlessTowerLine text;
    int kind = 0;

    bool operator==(const EndlessTowerRow&) const = default;
};

struct EndlessTowerRmlModel
{
    EndlessTowerLine title;
    // The entry price and runs a day, the party's size, when the runs come back, the own best and the top's.
    EndlessTowerLine entry;
    EndlessTowerLine party;
    bool partyOk = false;
    EndlessTowerLine reset;
    EndlessTowerLine best;
    std::vector<EndlessTowerTab> tabs;
    std::vector<EndlessTowerRow> rows;
    // Why the button is off, or the server's answer to the last Enter, on as many lines as it needs (3 at most).
    std::vector<EndlessTowerLine> hint;
    bool hintBad = false;
    bool canEnter = false;
    EndlessTowerLine enterLabel;

    Rml::String exitTooltip;
};
} // namespace mu::ui::window
