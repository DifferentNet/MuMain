#pragma once

// Web build only. A phone's own keyboard for the game's text fields: a canvas can't bring it up, so
// the page (index.html, Module.muKeyboard) focuses a hidden <input> of its own and passes what is
// typed there to the game as key presses, which SDL turns into text as from a real keyboard.
namespace mu::web
{
// The keyboard a game's text field gets (the page numbers them the same way: FIELD_KEYS).
enum class FieldKeys : int
{
    None = 0, // no text field
    Text = 1,
    Password = 2, // without suggestions
    Digits = 3,   // a number field (UI::RmlBridge::NumericFieldClass: zen, counts): the number keys
};

// A text field took the keys (RmlUi's ActivateKeyboard, again as its caret moves): the keyboard comes
// up, the page keeps the field's line, lineTop to lineBottom as fractions of the window's height,
// above it. A password field gets its keys at once; any other keeps the keys the keyboard has until
// SyncKeyboard() gives the field's own at the end of the frame (a number field's number keys).
void ShowKeyboard(bool password, float lineTop, float lineBottom);

// No text field has the keys any more.
void HideKeyboard();

// Once a frame, with the field RmlUiRuntime's GetFocusedTextField() finds (None: no field): the
// keyboard goes away when its field did without a blur (that function says how that happens), and
// it changes to the field's keys. ShowKeyboard can't always tell them: RmlUi activates the keyboard
// while it still moves the focus to the field.
void SyncKeyboard(FieldKeys focused);

// Once a frame: the text field the finger on the screen would tap when it lifts (None: no field).
// An iPhone brings its keyboard up only for a focus made as the finger lifts, and the game's frame
// sees the tap after that, so the page focuses its own field then.
void SyncTapOnTextField(FieldKeys tapped);

// While a gamepad is in use its own on-screen keyboard types (UI/HUD/PadKeyboard): the phone's stays
// down, and comes up for the field that has the keys once the pad is put down.
void HoldBackKeyboard(bool heldBack);
} // namespace mu::web
