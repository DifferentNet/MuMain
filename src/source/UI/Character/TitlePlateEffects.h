#pragma once

#include "GameLogic/Events/Titles.h"
#include "Render/Renderer/Overlay2DRecorder.h"

#include <array>
#include <vector>

class CHARACTER;

// MU Moldova: the title plates' effects (ghinzuro, 2026-11-03, variant D of the effects page): a light running over
// the plate every few seconds, oftener and brighter for the higher rarities (silver faintly, every 12 s); sparkles on
// the drawn gems (all but silver); a breathing glow around gold, crimson and violet; crimson's text shimmering to
// gold; and per title embers over the crown (cp-first, castle-lord) and a rainbow light (streamer). Every plate starts
// at its own moment, from its owner's name. The layers are white shapes in Data/Interface/newui_titles_fx.OZT
// (tools/make_title_plates.ps1), added and tinted. Options, Interface tab: config.ini [UI] TitleEffects.
namespace UI::TitlePlates::Effects
{
bool Enabled();

// Where a plate is drawn, in the name window's 640 x 480 space: its top left corner and how many of those pixels one
// of the plates' atlas pixels takes.
struct Placement
{
    float left = 0.f;
    float top = 0.f;
    float scale = 1.f;
};

// The glow, drawn before the plate.
void RenderUnder(const GameLogic::Titles::Worn& title, const CHARACTER& owner, const Placement& at);
// The light, the sparkles and the embers, drawn after the plate and before its text.
void RenderOver(const GameLogic::Titles::Worn& title, const CHARACTER& owner, const Placement& at);
// The text's colour now: base, or for crimson on its way to gold and back.
std::array<unsigned char, 3> TextColor(const GameLogic::Titles::Worn& title, const CHARACTER& owner,
                                       const unsigned char (&base)[3]);

// The same for a window that draws a plate itself (the titles window's preview): the layers' quads (four corners each:
// top left, bottom left, bottom right, top right) in the window's own pixels, at in them too, u and v 0 to 1 in
// FxAtlasSource; added to out, nothing drawn. phase is PhaseOf() the owner's name, so the plate keeps its moments.
inline constexpr const char* FxAtlasSource = "/Data/Interface/newui_titles_fx.tga";
float PhaseOf(const wchar_t* name);
void BuildUnder(const GameLogic::Titles::Worn& title, float phase, const Placement& at,
                std::vector<Render::Renderer::RecordedVertex>& out);
void BuildOver(const GameLogic::Titles::Worn& title, float phase, const Placement& at,
               std::vector<Render::Renderer::RecordedVertex>& out);
std::array<unsigned char, 3> TextColor(const GameLogic::Titles::Worn& title, float phase,
                                       const unsigned char (&base)[3]);
} // namespace UI::TitlePlates::Effects
