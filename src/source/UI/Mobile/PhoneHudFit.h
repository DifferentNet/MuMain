// MU Moldova: where the phone's HUD pieces of other documents fit between the touch controls
// (UI/Mobile/PhoneHudLayout places them): the party's cards under the top row of buttons, in one or
// two columns above the controls round Attack, as big as they fit; the chat moved left until it
// clears the skill buttons beside it; whether a world label lies under the HUD.
//
// The sums only, in window pixels; the layout measures the boxes from the documents.
#pragma once

#include "UI/Mobile/TouchTarget.h"

#include <span>

namespace UI::Mobile::PhoneHudFit
{
using TouchTarget::Box;

// The lowest a piece across [left, right] that starts at `top` may reach: `gap` above the first
// shown box under it, else `floor`. A box beside `top` leaves no room at all.
[[nodiscard]] float FreeBottom(std::span<const Box> boxes, float left, float right, float top, float gap, float floor);

// The party's cards are drawn bigger than the dock's scale on a phone, by this much at most, so a
// finger can pick one; on a screen too short for that, less, down to the dock's own scale.
inline constexpr float MaxCardScale = 1.4f;
inline constexpr float CardScaleStep = 0.1f;
inline constexpr int CardScaleSteps = 4;
// Columns of cards side by side at most: five members never fit in one column on a phone.
inline constexpr int MaxColumns = 2;

// Where the cards may go and how big one is at the dock's own scale, in window pixels.
struct PartyArea
{
    // The first column's right edge (the next one goes left of it) and the cards' top.
    float right = 0.f;
    float top = 0.f;
    // No column reaches left of this, nor below this.
    float left = 0.f;
    float floor = 0.f;
    // Kept between the cards and the controls.
    float gap = 0.f;
    float cardWidth = 0.f;
    float cardHeight = 0.f;
    // From one card's top to the next one's.
    float cardPitch = 0.f;
    float columnGap = 0.f;
};

struct PartyFit
{
    // The cards' scale on top of the dock's.
    float scale = 1.f;
    // Cards in a column before the next column starts.
    int rows = 0;
    int columns = 0;
    // False when even the dock's own scale in MaxColumns columns runs into a control: the cards
    // are then laid out that way all the same.
    bool fits = false;
};

// The biggest scale, then the fewest columns, at which `cards` cards fit in `area` clear of
// `controls`. A column fills top down before the next one starts left of it.
[[nodiscard]] PartyFit FitParty(const PartyArea& area, int cards, std::span<const Box> controls);

// How far to move a piece at [left, right] x [top, bottom] to the left (0 or less) so that it ends
// `gap` short of the shown boxes beside it on the right; never left of `minLeft` or of `gap` past a
// box beside it on the left. 0 when it is clear already.
[[nodiscard]] float ShiftLeft(std::span<const Box> boxes, float left, float right, float top, float bottom, float gap,
                              float minLeft);

// Whether a shown zone overlaps the box; boxes that only touch do not.
[[nodiscard]] bool Overlaps(std::span<const Box> zones, const Box& box);
} // namespace UI::Mobile::PhoneHudFit
