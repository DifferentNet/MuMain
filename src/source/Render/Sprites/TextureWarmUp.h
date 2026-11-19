#pragma once

// The browser client's learned texture warm list. The item, skill and effect textures load when first
// drawn (CGlobalBitmap::BeginDeferredLoads): the first cast of a skill decoded and uploaded its 5-10
// textures inside that frame, a 20-80 ms hitch on a phone. The file names of the deferred textures the
// player needed are kept in the browser's storage (at most kCapacity, the most recent last, saved every
// few seconds while they change; only those needed in the world). In the next session, a few seconds after
// the hero is in the world and once the late pack is installed (not where it failed), the names learned
// before are loaded between frames, the most recent first, one or two a frame and only after a short
// frame, the way UI::WarmUp paces windows. One texture over 50 ms stops the warm-up for the session.
//
// Only textures this player needed before take video memory early, and at most kCapacity of them. A
// warmed texture isn't learned again, so a name leaves the list once kCapacity newer ones were needed.
// Off with index.html?nowarmtextures.
#ifdef __EMSCRIPTEN__
#include <cstddef>
#include <string>

namespace Render::TextureWarmUp
{
inline constexpr std::size_t kCapacity = 150;

// CGlobalBitmap::ResolvePendingLoad, once a recorded file loaded (`fileName` as recorded).
void NoteResolved(const std::wstring& fileName);
// Once a game frame, before it runs: saves the list when due, and loads the next learned textures.
void Tick(double lastFrameMs);
} // namespace Render::TextureWarmUp
#endif
