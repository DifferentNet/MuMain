// The names on the pad's button glyphs, for the maker whose buttons the player holds (Options ->
// Interface -> Button glyphs, Auto by the pad itself): Xbox A B X Y and LB RB LT RT, PlayStation's
// shapes and L1 R1 L2 R2, Nintendo's letters and L R ZL ZR. The game reads every pad by place, so the
// bottom button confirms on all three: Nintendo's B sits where Xbox's A does (ghinzuro 2026-11-13:
// letters only, confirm stays on the bottom button).
//
// touch_controls.rml, pad_overlay.rml and pad_keyboard.rml bind the same fields: the body takes the
// classes glyph-ps / glyph-nin from glyph_family, a face glyph shows {{glyph_a}} .. {{glyph_y}} (empty
// for PlayStation, whose shapes gamepad_glyphs.rcss draws from the glyph's .pad-ps boxes), a bumper or
// trigger {{glyph_lb}} .. {{glyph_rt}}, a stick {{glyph_ls}} / {{glyph_rs}} and the left stick's click
// {{glyph_l3}}.
//
// The glyphs follow the player's button layout (Core/Input/GamepadLayout.h): a glyph stands where the
// game's control is and names the physical button that plays it. With the shoulders swapped {{glyph_lb}}
// says LT and the body takes the class swap-shoulders from glyph_swap (gamepad_glyphs.rcss draws a
// .pad-bumper as a trigger then, and the other way round); with the sticks swapped {{glyph_ls}} says RS.
#pragma once

#include "Core/Input/GamepadKind.h"
#include "UI/HUD/PadButtonNames.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include <functional>
#include <string>

namespace UI::HUD::PadGlyphs
{
struct Model
{
    // 0 Xbox, 1 PlayStation, 2 Nintendo (Core::Input::Gamepad::PadKind).
    int family = 0;
    Rml::String a = "A";
    Rml::String b = "B";
    Rml::String x = "X";
    Rml::String y = "Y";
    Rml::String lb = "LB";
    Rml::String rb = "RB";
    Rml::String lt = "LT";
    Rml::String rt = "RT";
    Rml::String ls = "LS";
    Rml::String rs = "RS";
    Rml::String l3 = "L3";
    // The bumpers and the triggers trade places (the layout): their glyphs trade their shapes.
    bool swapShoulders = false;
};

void Bind(Rml::DataModelConstructor& constructor, Model& model);

// Brings the fields to the glyphs shown now (Core::Input::Gamepad::ShownKind()), calling markDirty
// with each changed field's name.
void Sync(Model& model, const std::function<void(const char*)>& markDirty);

// The buttons' names in words for a text (UI/HUD/PadButtonNames): Xbox A .. LB .. LS, Menu, View;
// PlayStation Cross, Circle, Square, Triangle (in the game's language), L1 .. L3, Options, Create;
// Nintendo B A Y X by place, L R ZL ZR, LS RS, + and -. Each by the game's button it plays, through the
// player's layout: with the shoulders swapped {LB} names LT.
[[nodiscard]] PadButtonNames::Names TextNames(Core::Input::Gamepad::PadKind kind);
// The text with the names of the glyphs shown now (Core::Input::Gamepad::ShownKind()).
[[nodiscard]] std::wstring Named(const wchar_t* text);
} // namespace UI::HUD::PadGlyphs
