// The one-time pad hints' moments and their bits in [UI] GamepadHintsSeen, and when the small screen
// hint applies: apart from the hints themselves (UI/HUD/PadHints.h), so the tests reach them without
// the game.
#pragma once

namespace UI::HUD::PadHints
{
// The moments, one bit each in [UI] GamepadHintsSeen: keep their values, the config file remembers them.
enum class Hint
{
    PickUp = 0,
    Window = 1,
    BagFull = 2,
    Death = 3,
    // A skill on the pad's RB page (hotkeys 7-9, pad round 6): hold RB and press X, Y or B.
    SkillsThird = 4,
    // A small screen with the interface at its normal size or smaller (pad round 7): the UI scale row.
    SmallScreen = 5,
};

// Whether `bits` (GamepadHintsSeen) says the hint showed already, and `bits` with it marked shown.
[[nodiscard]] bool Seen(int bits, Hint hint);
[[nodiscard]] int Marked(int bits, Hint hint);

// The small screen hint's window: 1 to SmallScreenHeight pixels high (a handheld, a small laptop
// window), the interface at 100 % or less, and not the phone view (it lays the screen out for a phone).
inline constexpr int SmallScreenHeight = 800;
inline constexpr int SmallScreenMostScale = 100;
[[nodiscard]] bool SmallScreenApplies(int windowHeight, int uiScalePercent, bool phoneView);
} // namespace UI::HUD::PadHints
