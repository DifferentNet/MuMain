#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// A line of the Tier Maps window's own text: what it says and the size the native renderer would have shrunk it to
// for its box. Where it sits and its colour are the theme's.
struct TierMapsLine
{
    Rml::String text;
    float textPx = 0.f;

    bool operator==(const TierMapsLine&) const = default;
};

// A tier: its name, its Combat Power and boss, whether the party may go in, and whether it is the one picked.
struct TierMapsRow
{
    int tier = 0;
    TierMapsLine title;
    TierMapsLine detail;
    bool open = false;
    bool selected = false;

    bool operator==(const TierMapsRow&) const = default;
};

// A party member: name, Combat Power, HP and runs left; ready when nothing keeps them out.
struct TierMapsMember
{
    TierMapsLine text;
    bool ready = false;

    bool operator==(const TierMapsMember&) const = default;
};

struct TierMapsRmlModel
{
    TierMapsLine title;
    // Today's fortress and its boss, the party's Combat Power, the ticket, when the runs come back.
    TierMapsLine today;
    TierMapsLine power;
    TierMapsLine ticket;
    bool ticketOk = false;
    TierMapsLine reset;
    std::vector<TierMapsRow> tiers;
    // For a party below the size the tiers are tuned for: how much more Combat Power it needs.
    TierMapsLine note;
    TierMapsLine membersTitle;
    std::vector<TierMapsMember> members;
    // Why the button is off, or the server's answer to the last Enter.
    TierMapsLine hint;
    bool hintBad = false;
    bool canEnter = false;
    TierMapsLine enterLabel;

    Rml::String exitTooltip;
};
} // namespace mu::ui::window
