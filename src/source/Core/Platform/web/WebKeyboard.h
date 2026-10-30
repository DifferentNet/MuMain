#pragma once

// Web build only. A phone's own keyboard for the game's text fields: a canvas can't bring it up, so
// the page (index.html, Module.muKeyboard) focuses a hidden <input> of its own and passes what is
// typed there to the game as key presses, which SDL turns into text as from a real keyboard.
namespace mu::web
{
// A text field took the keys (RmlUi's ActivateKeyboard): the keyboard comes up, the page keeps the
// field's line, lineTop to lineBottom as fractions of the window's height, above it. A password
// field gets one without suggestions.
void ShowKeyboard(bool password, float lineTop, float lineBottom);

// No text field has the keys any more.
void HideKeyboard();

// Once a frame, with the field RmlUiRuntime's GetFocusedTextField() finds: the keyboard goes away
// when its field did without a blur (that function says how that happens), and it turns into a
// password keyboard when the field is one. ShowKeyboard can't always tell: RmlUi activates the
// keyboard while it still moves the focus to the field.
void SyncKeyboard(bool textFieldFocused, bool password);

// Once a frame: whether the finger on the screen would tap a text field when it lifts, and whether
// that is a password field. An iPhone brings its keyboard up only for a focus made as the finger
// lifts, and the game's frame sees the tap after that, so the page focuses its own field then.
void SyncTapOnTextField(bool onTextField, bool password);

// While a gamepad is in use its own on-screen keyboard types (UI/HUD/PadKeyboard): the phone's stays
// down, and comes up for the field that has the keys once the pad is put down.
void HoldBackKeyboard(bool heldBack);
} // namespace mu::web
