#pragma once

#include <optional>

// Web build only. The game's size in the browser. The page (index.html, muDisplay) works out the
// game area in the screen's real pixels, at most the picture quality's pixel budget, and the game
// follows it while Options has "Auto", so the picture is drawn 1:1 instead of stretched. A size
// picked in Options turns that off; the page remembers the choice.
namespace mu::web
{
struct PageDisplay
{
    int width = 0; // in device pixels
    int height = 0;
    float pixelRatio = 1.0f; // device pixels per CSS pixel
    int serial = 0;          // a new number for every new size or ratio
};

// The size the page wants while "Auto" is on; nothing while the player has a size from Options.
std::optional<PageDisplay> GetAutoDisplay();
bool IsAutoDisplay();
// Options: "Auto" (true) or a size of the player's own (false). The page remembers it.
void SetAutoDisplay(bool enabled);
} // namespace mu::web
