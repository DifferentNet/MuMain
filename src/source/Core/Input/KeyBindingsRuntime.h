#pragma once

#include "Core/Input/KeyBindings.h"

// The player's key bindings in the running game: loaded from config.ini's [KeyBindings] once at start, read by the
// hot keys every frame, changed by the key bindings window (UI/Options/KeyBindingsWindow.h), which saves at once.
namespace Core::Input::KeyBindings::Runtime
{
// The current table; the defaults until Load() ran.
[[nodiscard]] const Table& Current();
[[nodiscard]] int KeyOf(Action action);

// For code that presses a key the way the keyboard would (the touch buttons and menu): the key now bound to the
// action whose default key this is, or the key itself when it is no action's default (Escape, Tab, F1...).
[[nodiscard]] int RemapDefaultKey(int key);

// Reads [KeyBindings] (after GameConfig::Load). A saved table that needed repairing is written back repaired.
void Load();

// Gives the action the key (a swap when another action held it, Table::Assign) and saves the changed lines.
Table::Change Assign(Action action, int key);
// Back to the default keys; the [KeyBindings] lines are removed.
void ResetToDefaults();

// Counts every change, so labels that show keys know when to rebuild.
[[nodiscard]] unsigned Revision();
} // namespace Core::Input::KeyBindings::Runtime
