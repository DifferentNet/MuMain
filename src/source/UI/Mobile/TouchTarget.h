// Which on-screen control a finger lands on (UI/HUD/TouchControls): the boxes the controls take, and
// the one a finger at a point is on when their margins reach over each other.
//
// The sums only; the touch controls measure the boxes from their document.
#pragma once

#include <span>

namespace UI::Mobile::TouchTarget
{
// A control's box in window pixels; not shown takes no finger.
struct Box
{
    bool shown = false;
    float left = 0.f;
    float top = 0.f;
    float right = 0.f;
    float bottom = 0.f;
};

inline constexpr int NoBox = -1;

// How far a point lies outside a box; 0 inside it or on its edge.
[[nodiscard]] float DistanceOutside(const Box& box, float x, float y);

// Of `boxes`, in order of preference, the index of the one a finger at (x, y) is on: one it lands
// inside, the first of those, else the closest within `slop`, the first of equals; NoBox when none
// is. A finger on a button's edge never goes to a neighbour whose margin reaches over it.
[[nodiscard]] int Closest(std::span<const Box> boxes, float x, float y, float slop);
} // namespace UI::Mobile::TouchTarget
