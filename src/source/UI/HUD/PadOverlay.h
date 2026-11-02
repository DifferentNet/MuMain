// What the gamepad shows over the world and its windows (pad_overlay.rml): the frame on the target
// it is on in a window (UI/HUD/PadFrame), the hero hub's tabs and what the pad's buttons do there
// (UI/HUD/PadHub), and under the drop d-pad down would pick up, a prompt to do it.
#pragma once

#include "UI/HUD/PadTargetPick.h"

namespace UI::HUD::PadOverlay
{
// What to show this frame; positions in the RmlUi context's pixels.
struct View
{
    bool hub = false;
    // The legend of the pad's buttons in a window.
    bool legend = false;
    // What the legend says: on an item's place Take, Equip, Card and Close (on worn gear Take off for
    // Equip, on a bag item Sell while a shop is open, Move while the storage or another window takes
    // it); on an item in the crafting box Take, Move and Close; on a shop's goods Buy and Close; on an
    // empty cell Close alone; with an item on the cursor Put down, Drop and Put back; on a button
    // Press and Close.
    enum class Legend
    {
        Item,
        // A bag item X uses (a potion), one X does nothing to (a jewel: A takes it onto an item).
        Use,
        Keep,
        Worn,
        Sell,
        Move,
        Crafting,
        Goods,
        Empty,
        Holding,
        Button,
    };
    Legend legendSays = Legend::Item;
    bool frame = false;
    PadTargetPick::Box frameBox;
    // The prompt under the drop d-pad down picks up, or under the NPC or player the right stick chose:
    // A talks to the NPC, opens the player's menu.
    enum class Prompt
    {
        PickUp,
        Talk,
        Menu,
    };
    Prompt promptSays = Prompt::PickUp;
    bool prompt = false;
    float promptX = 0.f;
    float promptY = 0.f;
    // How far from the right the tabs and the legend end, so they stay off the windows; 0 = the whole width.
    float topRight = 0.f;
};

// Once a frame from UI::HUD::GamepadControls; an empty view (the pad put down) hides it.
void Update(const View& view);
} // namespace UI::HUD::PadOverlay
