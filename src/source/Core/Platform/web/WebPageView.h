#pragma once

// Web build only. The page's own View item (index.html, in its menu: ⋮ on a phone, the footer on a
// computer) picks the same Auto / PC / Phone choice as Options -> Interface. The page keeps its
// request in window.muView; the game takes it between frames and tells the page what it shows, for
// the item's label.
namespace mu::web
{
// The choice the page asked for since the last call (UI::Mobile::ViewChoice's order), or -1.
int TakePageViewRequest();

// The choice in force and whether it came to the phone view.
void ShowPageView(int choice, bool phoneView);

// Whether a character is in the world. While one is, the page asks before it is closed or left
// (window.muInWorld): Ctrl+W closes a tab in browsers on Windows. Call it every frame; the page hears
// only the changes.
void ShowInWorld(bool inWorld);
} // namespace mu::web
