#pragma once

#include "Render/Textures/SpriteReforge.h"

#include <map>
#include <string>
#include <string_view>

// A theme's [Reforge] list (theme.ini): which interface sprites RmlUi recolours as it loads them, and with which
// preset (Render/Textures/SpriteReforge.h). The sprite files stay as they are: the decoded pixels, original or HD,
// are changed before the upload, so a theme needs no copies of the art.
//
//   [Reforge]
//   newui_msgbox_back=stone
//   partcharge1/newui_menu03=metal
//
// A key is the file's path under Data/Interface/, lower-case, without its extension. The list is merged down the
// theme's inheritance chain like any other section: a child replaces a parent's entry, and `none` turns one off.
namespace UI::RmlBridge
{
class ThemeInheritance;

// Every sprite a theme reforges, with its preset; a sprite missing from the list is drawn as it is.
using ReforgeList = std::map<std::string, Render::Textures::Reforge::Preset>;

// The [Reforge] key of a texture path, e.g. "Data\\Interface\\PartCharge1\\newui_Menu03.OZJ" ->
// "partcharge1/newui_menu03". Empty (never reforged) for a path outside Data/Interface/, for a theme's own art under
// Data/Interface/RmlUi/themes/, and for a file that is not .tga, .jpg, .ozt or .ozj.
[[nodiscard]] std::string NormalizeReforgeKey(std::string_view path);

// `theme`'s [Reforge] list. An unknown preset name is reported once (g_ErrorReport) and counts as none.
[[nodiscard]] ReforgeList ReadReforgeList(const ThemeInheritance& theme);

// The preset `theme` gives the texture file at `path`: None when its list does not name it.
[[nodiscard]] Render::Textures::Reforge::Preset LookupReforge(const ThemeInheritance& theme, std::string_view path);

// The active theme's (GetActiveThemeName()) list, read on first use and kept until InvalidateActiveReforge().
[[nodiscard]] const ReforgeList& ActiveReforgeList();

// The preset the active theme gives the texture file at `path`.
[[nodiscard]] Render::Textures::Reforge::Preset ActiveReforgePreset(std::string_view path);

// Drops the kept list, so the next use reads the theme again. SetActiveThemeName() calls it.
void InvalidateActiveReforge();

// A live theme switch. RmlUi keeps a file texture by its source, which is the same in every theme, so a sprite
// whose preset changed must be loaded again. Pass the list from before the switch (ActiveReforgeList() then), right
// after SetActiveThemeName(); nothing is queued when the two lists agree.
void QueueReforgeRelease(ReforgeList previous);

// Releases each texture RmlUi holds whose preset the queued switch changed; RmlUi loads it again, with the new
// preset, the next time it draws it. UI::Theme::Select() calls it right after queuing; every frame calls it again
// before RmlUi updates, as a safety net.
void ReleaseQueuedReforgeTextures();
} // namespace UI::RmlBridge
