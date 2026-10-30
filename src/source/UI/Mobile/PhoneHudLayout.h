// The phone's HUD (UI/HUD/TouchControls, sized in finger units: UI/Mobile/TouchUnit) and the HUD
// pieces of other documents around it, which their themes size in the PC's dp: the buffs and the pets'
// frames under the top row, the system messages under the hero frame, the touch menu's skill list
// beside the skill buttons, the party's cards under the top row's right end and the chat left of the
// skill buttons. A finger unit is not a dp (one follows the screen's size on the page, the other the
// game's resolution), so the themes' `@media (theme: mobile-hud)` places are a first guess only: this
// moves those pieces to the touch layer's edges as it lays them out, and gives them their themes'
// places back when the phone's HUD goes. It also tells the world labels where the HUD lies over them,
// and puts the notices under the open touch menu (UI/Mobile/NoticesUnderPanel).
#pragma once

#include "UI/Mobile/TouchTarget.h"

#include <array>
#include <span>

namespace UI::Mobile::PhoneHudLayout
{
// The touch layer's edges, in window pixels.
struct Edges
{
    // Room for the controls the pieces keep clear of.
    static constexpr int MaxControls = 16;

    // The bottom of the top row of buttons, and its right end (the page's own menu button is right
    // of it). 0 leaves the party's cards at their theme's place.
    float topRowBottom = 0.f;
    float topRowRight = 0.f;
    // The hero frame.
    float heroLeft = 0.f;
    float heroRight = 0.f;
    float heroBottom = 0.f;
    // The potion buttons Q and W, under the hero frame.
    float potionsTop = 0.f;
    float potionsRight = 0.f;
    // The left edge of the skill buttons around Attack.
    float skillsLeft = 0.f;
    // The buttons the party's cards and the chat must not cover (Attack, the skill buttons, Pick
    // up, Auto, Skills, the potions), as laid out: the first controlCount of controls.
    std::array<TouchTarget::Box, MaxControls> controls{};
    int controlCount = 0;

    // Adds a control the pieces keep clear of; a hidden one, or one past MaxControls, is left out.
    void KeepClearOf(const TouchTarget::Box& box)
    {
        if (box.shown && controlCount < MaxControls)
            controls[static_cast<std::size_t>(controlCount++)] = box;
    }
    [[nodiscard]] std::span<const TouchTarget::Box> Controls() const
    {
        return {controls.data(), static_cast<std::size_t>(controlCount)};
    }
};

// Once a frame while the phone's HUD shows, with the edges as the touch layer was last laid out. A
// piece is only placed again when something it is placed by changed.
void Follow(const Edges& edges);
// When it hides: the pieces go back to their themes' places.
void Release();

// Whether a world label (UI/Character/WorldLabelLayer, window pixels) lies under the phone's HUD:
// the band of the top row across the screen, the hero frame with the location bar above it, or one of
// the controls the pieces keep clear of. The labels draw under the HUD, so their text would mix with
// the buttons'. Always false while the PC's HUD shows.
[[nodiscard]] bool CoversLabel(const TouchTarget::Box& label);
} // namespace UI::Mobile::PhoneHudLayout
