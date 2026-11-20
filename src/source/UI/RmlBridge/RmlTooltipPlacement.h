#pragma once

// The shared tooltip's placement rules, free of RmlUi so they can be tested on their own.
namespace UI::RmlBridge::TooltipPlacement
{
// CNewUIButton::Render()'s hint around a button drawn at (left, top, width, height) in window
// pixels, `unit` pixels per original unit: its centre 3 units right of the button's, its top 2 units
// below the button, or its bottom 2 units above it.
struct ButtonHintAnchor
{
    float x = 0.f;
    float belowY = 0.f;
    float aboveY = 0.f;
};
ButtonHintAnchor ForButton(float left, float top, float width, float height, float unit);

// The top of a box `height` tall: below `anchorY` (its top there) or above it (its bottom there),
// as asked. When that leaves the viewport and a `flipAnchorY` is given, the other way from that
// anchor, if it fits there; then clamped into the viewport either way.
float Top(float anchorY, bool above, const float* flipAnchorY, float height, float viewportHeight);

// The left edge of a box `height` tall at `left`, growing from `anchorY` (above: its bottom there; below: its top
// there). When it does not fit on that side without being moved and a `besideLeft` is given (the right edge of the
// window the anchor lies in), that instead: the box goes beside the window rather than over it. Not yet clamped.
float LeftOrBeside(float left, float anchorY, bool above, const float* besideLeft, float height, float viewportHeight);

// The scale a box `height` tall is drawn at to fit a viewport `viewportHeight` tall: 1 when it fits,
// else the viewport's share of it, but never below `minimum` (smaller text no longer reads; the rest
// stays off screen then).
float FitScale(float height, float viewportHeight, float minimum);
} // namespace UI::RmlBridge::TooltipPlacement
