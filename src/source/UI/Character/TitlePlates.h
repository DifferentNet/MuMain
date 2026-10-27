#pragma once

#include "GameLogic/Events/Titles.h"

class CHARACTER;

// MU Moldova: the title plate over a character's name (ghinzuro, 2026-10-27). Every title is a ribbon in its
// rarity's colour with a gem at its left end; gold and crimson ones (the weekly tops, the castle's lord) are a
// banner with wings and a gem on top instead. Drawn by UI::OverheadNames over the name line and by the hover plate
// (UI/Chat/Chat.cpp) over its box, in the name window's 640 x 480 space. The pieces come from
// Data/Interface/newui_titles.OZT (tools/make_title_plates.ps1).
namespace UI::TitlePlates
{
// Titles over the players: Options, Interface tab; config.ini [UI] OverheadTitles.
bool Enabled();

// True for the rarities that get the winged banner.
bool IsBanner(GameLogic::Titles::Rarity rarity);

// Draws the title of owner centred on centerX, its lowest point on bottomY, or over the owner's party HP bar
// (Engine/Object/ZzzInterface.cpp RenderPartyHP) when that is higher. Returns how high it is (0: nothing drawn).
int Render(const GameLogic::Titles::Worn& title, const CHARACTER& owner, int centerX, int bottomY);
} // namespace UI::TitlePlates
