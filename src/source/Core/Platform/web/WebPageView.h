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

// Whether a character is in the world, and the tab's title (UTF-8). While one is, the page asks before
// it is closed or left (window.muInWorld): Ctrl+W closes a tab in browsers on Windows. Call it on a
// change only (Winmain.cpp FollowWindowTitle).
void ShowInWorld(bool inWorld, const char* title);

// The game's loop ended (Exit game, or OK on a lost connection): the page shows its own panel with
// Play again, and closing the tab no longer asks (window.muGameEnded). Call it once.
void ShowGameEnded();

// Whether the page's Screenshot item (index.html, window.muShotRequest) asked for a picture since the
// last call.
bool TakePageShotRequest();

// Hands a saved picture (a JPEG among the game's own files, `path` in UTF-8) to the page, which offers it
// as a download, and removes the file.
void OfferShot(const char* path);

// Tells the page a screenshot it asked for was not taken (another capture pending, or no picture came).
void ShowShotFailed();
} // namespace mu::web
