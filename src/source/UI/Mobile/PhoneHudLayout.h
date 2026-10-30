// The phone's HUD (UI/HUD/TouchControls, sized in finger units: UI/Mobile/TouchUnit) and the HUD
// pieces of other documents around it, which their themes size in the PC's dp: the buffs and the pets'
// frames under the top row, the system messages under the hero frame, the touch menu's skill list
// beside the skill buttons. A finger unit is not a dp (one follows the screen's size on the page, the
// other the game's resolution), so the themes' `@media (theme: mobile-hud)` places are a first guess
// only: this moves those pieces to the touch layer's edges as it lays them out, and gives them their
// themes' places back when the phone's HUD goes.
#pragma once

namespace UI::Mobile::PhoneHudLayout
{
// The touch layer's edges, in window pixels.
struct Edges
{
    // The bottom of the top row of buttons.
    float topRowBottom = 0.f;
    // The hero frame.
    float heroLeft = 0.f;
    float heroRight = 0.f;
    float heroBottom = 0.f;
    // The potion buttons Q and W, under the hero frame.
    float potionsTop = 0.f;
    float potionsRight = 0.f;
    // The left edge of the skill buttons around Attack.
    float skillsLeft = 0.f;
};

// Once a frame while the phone's HUD shows, with the edges as the touch layer was last laid out.
void Follow(const Edges& edges);
// When it hides: the pieces go back to their themes' places.
void Release();
} // namespace UI::Mobile::PhoneHudLayout
