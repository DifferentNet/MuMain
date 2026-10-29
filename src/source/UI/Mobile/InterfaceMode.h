// The phone or PC layout, and touch controls or the mouse: the player's choice in Options -> Interface
// (config.ini [UI] InterfaceView, Controls). "Auto" follows the device: a phone or tablet in the
// browser gets the phone layout and the touch controls, a computer the PC layout and the mouse.
//
// The phone layout is a style flag every RmlUi document sees: rules under `@media (theme: mobile)`
// apply while it is on, live, whichever theme (legacy or modern) is picked.
#pragma once

namespace UI::Mobile
{
enum class ViewChoice
{
    Auto,
    Pc,
    Phone,
};

enum class ControlChoice
{
    Auto,
    MouseKeyboard,
    Touch,
};

inline constexpr int ViewChoiceCount = 3;
inline constexpr int ControlChoiceCount = 3;

[[nodiscard]] ViewChoice GetViewChoice();
[[nodiscard]] ControlChoice GetControlChoice();

// Save the choice and apply it at once. Not from inside an RmlUi event: touch off lets go of the
// fingers, which may be the one clicking.
void SetViewChoice(ViewChoice choice);
void SetControlChoice(ControlChoice choice);

// What the choices come to on this device (as last applied).
[[nodiscard]] bool IsPhoneView();
[[nodiscard]] bool IsTouchControls();
// The phone layout with the touch controls: their buttons and the hero frame take the place of the
// PC's bottom bar.
[[nodiscard]] bool ReplacesPcHud();

// Applies the saved choices: the `mobile` and `mobile-hud` style flags and whether fingers drive the
// touch controls.
// Once the RmlUi runtime exists (Winmain.cpp), and again by the setters.
void Apply();
} // namespace UI::Mobile
