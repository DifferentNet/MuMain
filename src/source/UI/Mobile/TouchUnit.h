// The on-screen controls' finger unit (UI::Mobile::ButtonSize) in the game's window pixels, and the
// player's Button size choice it follows (config.ini [UI] TouchButtonSize).
//
// A document sized in finger units sets its font size to one unit (Apply) and writes its sizes in
// rem: touch_controls.rml and the pad glyphs it shows (gamepad_glyphs.rcss).
#pragma once

namespace Rml
{
class ElementDocument;
}

namespace UI::Mobile::TouchUnit
{
// One finger unit in window pixels. In the browser it follows the canvas's size on the page;
// elsewhere it is RmlUi's dp at the player's percent.
[[nodiscard]] float Pixels();

// A text size in window pixels, raised in the phone view to the smallest a phone shows readably (12
// finger units at 100 %, whatever the Button size): for the small texts a PC screen gets away with,
// the chat's and the system messages'. Unchanged in the PC view.
[[nodiscard]] float ReadableTextPx(float px);

// Sets the document's font size to one unit, when it differs, so its rem sizes follow.
void Apply(Rml::ElementDocument* document);

// The player's choice, in percent, and saving a new one (in effect at once).
[[nodiscard]] int GetPercent();
void SetPercent(int percent);
} // namespace UI::Mobile::TouchUnit
