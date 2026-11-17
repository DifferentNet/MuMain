#pragma once

#include "Data/GameConfig/GameConfigValidation.h"

// The low frame rate hint's toast (graphics_hint.rml): a small panel near the top of the screen that offers a lower
// graphics preset, with "Lower to X", "Keep" and "Don't ask again". Non-modal: it never takes the keyboard focus, so
// Enter and Esc keep working for chat and the windows, and the world stays playable around it. It only shows and
// reports the click; what each answer does is Scenes/LowFpsHint's.
namespace UI::GraphicsHint
{

enum class Answer
{
    None,
    Lower,
    Keep,
    Never,
};

// Shows the toast offering `target`, with the frame rate the game ran at (`fps`).
void Show(GraphicsPreset target, int fps);
// Hides it without an answer.
void Hide();
[[nodiscard]] bool IsShown();
// The button the player clicked since the last call (None if none); the toast is hidden then.
[[nodiscard]] Answer TakeAnswer();
// The player's name of a preset (Low, Medium, ...), as the Options row shows it.
[[nodiscard]] const wchar_t* PresetLabel(GraphicsPreset preset);

} // namespace UI::GraphicsHint
