#pragma once

#include "GameLogic/Events/Titles.h"

class CHARACTER;

// MU Moldova: the title plate over a character's name (ghinzuro, 2026-10-27). Each rarity has its own drawn plate
// (ghinzuro and his wife, 2026-10-29), richer with each rank: silver plain; ice purple with an amethyst; jade green
// with bronze wings and an emerald; gold blue with silver wings and three sapphires; crimson red with golden wings
// and a crown; the unique violet titles (tester, streamer) blue with golden laurels and three sapphires. Each plate is
// drawn whole, never squeezed or stretched, and the central fields of all six are the same size whatever the title
// (his wife, 2026-10-29); a title too long for the field in bold gets the regular font. Drawn by
// UI::OverheadNames over the name line and by the hover plate (UI/Chat/Chat.cpp) over its box, in the name
// window's 640 x 480 space. The art comes from Data/Interface/newui_titles.OZT (tools/make_title_plates.ps1).
namespace UI::TitlePlates
{
// Titles over the players: Options, Interface tab; config.ini [UI] OverheadTitles.
bool Enabled();

// The plates' size, the text's with them (as far as the font may shrink): 1 is a field as high as the font and 5 more,
// 0.8 all of it a fifth smaller. The game uses 0.7 (ghinzuro, 2026-10-29). Dev builds try other sizes with the
// console's "$titlescale <percent>".
float Scale();
void SetScale(float scale);

// Draws the title of owner centred on centerX, its lowest point on bottomY, or over the owner's party HP bar
// (Engine/Object/ZzzInterface.cpp RenderPartyHP) when that is higher. Returns how high it is (0: nothing drawn).
int Render(const GameLogic::Titles::Worn& title, const CHARACTER& owner, int centerX, int bottomY);
} // namespace UI::TitlePlates
