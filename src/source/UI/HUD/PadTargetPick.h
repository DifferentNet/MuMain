// The gamepad's frame over an open window (UI/HUD/GamepadControls) rests on one of its targets: an item
// cell, an equipment slot, a button. These pick which: the one under the pointer, the nearest one for
// the pointer to snap onto, and the next one the d-pad points at. Boxes are in any one unit, the same
// for all.
#pragma once

#include <span>

namespace UI::HUD::PadTargetPick
{
inline constexpr int None = -1;

struct Box
{
    float left = 0.f;
    float top = 0.f;
    float right = 0.f;
    float bottom = 0.f;

    [[nodiscard]] float CentreX() const { return (left + right) * 0.5f; }
    [[nodiscard]] float CentreY() const { return (top + bottom) * 0.5f; }
    [[nodiscard]] bool Contains(float x, float y) const { return x >= left && x < right && y >= top && y < bottom; }
};

struct Target
{
    Box box;
    // The window it belongs to: the d-pad stays in a window while it has targets that way.
    int window = 0;
    // The list that scrolls it (None outside one), and whether that list can scroll up or down: past
    // the list's last row on view, the d-pad scrolls it instead of leaving it.
    int list = None;
    bool listScrollsUp = false;
    bool listScrollsDown = false;
    // An item cell (a bag's, a shop's): a window opening starts on its first one.
    bool cell = false;
    // An equipment slot: with the cells, an item's place (the legend says Take, Equip, Card there).
    bool slot = false;
    // A cell or a worn slot with no item in it: the legend offers nothing to take there.
    bool empty = false;
};

// The smallest target holding the point (a button over its panel), None for none.
[[nodiscard]] int At(std::span<const Target> targets, float x, float y);

// The target whose box is nearest the point, if within `reach` of it, None for none.
[[nodiscard]] int Nearest(std::span<const Target> targets, float x, float y, float reach);

// Where the frame starts in a window just opened: the item cell nearest the cells' top-left corner, or
// without cells the target nearest the targets' top-left corner; None for none.
[[nodiscard]] int First(std::span<const Target> targets);

// The next target from `from` the way (dx, dy) points (one of -1, 0, 1 on one axis): the nearest in
// that direction, one in line before one aside; one in the same window before one in another. None
// when nothing lies that way.
[[nodiscard]] int NextThatWay(std::span<const Target> targets, int from, int dx, int dy);
} // namespace UI::HUD::PadTargetPick
