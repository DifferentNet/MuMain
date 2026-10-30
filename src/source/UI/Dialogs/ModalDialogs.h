// The dialogs that wait for an answer and hold the game until they get one: a message box, a
// confirmation (a trade or party request, a question before a sale) and the system menu. The touch
// menu shuts while one is up (UI/HUD/TouchControls), and the pad answers it with its own buttons
// (UI/HUD/GamepadControls).
#pragma once

namespace UI::Dialogs::Modal
{
// Whether one of those dialogs is up.
[[nodiscard]] bool IsOpen();
// Whether the pointer is on a button RmlUi draws (an element with a click action, or styled as a
// button): the dialog's own answers, the HUD's buttons. A press there clicks it.
[[nodiscard]] bool IsPointerOnButton();
} // namespace UI::Dialogs::Modal
