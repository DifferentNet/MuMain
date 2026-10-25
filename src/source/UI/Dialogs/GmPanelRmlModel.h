#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// A line of the GM window's own text: what it says and the size the native renderer would have drawn (or shrunk)
// it at for its box. Where it sits and its colour are the theme's.
struct GmPanelLine
{
    Rml::String text;
    float textPx = 0.f;

    bool operator==(const GmPanelLine&) const = default;
};

// A button: its id (the click hands it back), its label, and whether it is the active tab / switched on, or off.
struct GmPanelButton
{
    int id = 0;
    GmPanelLine label;
    bool on = false;
    bool off = false;

    bool operator==(const GmPanelButton&) const = default;
};

// A row of one of the lists: two lines, a click picks it.
struct GmPanelRow
{
    int index = 0;
    GmPanelLine title;
    GmPanelLine detail;
    bool selected = false;
    // An offline character or an ended entry; a GM, or a banned or muted player.
    bool dim = false;
    bool alert = false;

    bool operator==(const GmPanelRow&) const = default;
};

struct GmPanelRmlModel
{
    GmPanelLine title;
    // 0 players, 1 world, 2 log.
    int tab = 0;
    std::vector<GmPanelButton> tabs;
    // The font size of the text fields.
    float fieldPx = 0.f;

    // Players: online, search or bans, the list, the picked player and what to do with them.
    int mode = 0;
    std::vector<GmPanelButton> modes;
    // Typed by the player: the model owns it, C++ only reads it.
    Rml::String searchText;
    GmPanelButton find;
    GmPanelLine listInfo;
    std::vector<GmPanelRow> rows;
    GmPanelLine selName;
    GmPanelLine selStats;
    GmPanelLine selWhere;
    std::vector<GmPanelButton> actions;

    // World: the teleport places, a map and coordinates, and the GM's own switches.
    GmPanelLine warpsTitle;
    std::vector<GmPanelRow> warps;
    GmPanelLine mapLabel;
    Rml::String mapText;
    Rml::String xText;
    Rml::String yText;
    GmPanelButton go;
    std::vector<GmPanelButton> switches;
    GmPanelLine here;

    // Log: the last actions of every GM.
    GmPanelButton logRefresh;
    GmPanelLine logInfo;
    std::vector<GmPanelRow> log;

    // The server's answer to the last action.
    GmPanelLine status;
    bool statusBad = false;

    Rml::String exitTooltip;
};
} // namespace mu::ui::window
