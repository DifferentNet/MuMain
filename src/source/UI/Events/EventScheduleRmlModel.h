#pragma once

#include <RmlUi/Core/Types.h>

#include <vector>

namespace mu::ui::window
{
// One line of the event timer window's own text: what it says and the size the native renderer would have
// shrunk it to for its box. Where it sits and its colour are the theme's.
struct EventScheduleLine
{
    Rml::String text;
    float textPx = 0.f;

    bool operator==(const EventScheduleLine&) const = default;
};

// One event: its name, and on the line below what it does and for how long, or when it starts.
struct EventScheduleRow
{
    Rml::String name;
    float nameTextPx = 0.f;
    Rml::String time;
    float timeTextPx = 0.f;
    // Running now or its entrance is open; the theme colours it.
    bool active = false;
    // Starts within a few minutes.
    bool soon = false;

    bool operator==(const EventScheduleRow&) const = default;
};

struct EventScheduleRmlModel
{
    EventScheduleLine title;
    // Shown in place of the rows while there are none: asking, no answer, nothing scheduled.
    EventScheduleLine message;
    std::vector<EventScheduleRow> rows;

    Rml::String exitTooltip;
};
} // namespace mu::ui::window
