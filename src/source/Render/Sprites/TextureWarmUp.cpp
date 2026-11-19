#include "stdafx.h"
#include "Render/Sprites/TextureWarmUp.h"

#ifdef __EMSCRIPTEN__
#include "Core/Platform/web/WebFileSystem.h"
#include "Core/Utilities/FirstOpenTiming.h"
#include "Core/Utilities/Log/MuLogger.h"
#include "Core/Utilities/RecentNames.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Sprites/GlobalBitmap.h"
#include "Scenes/SceneCore.h"

#include <emscripten.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
// The learned names (UTF-8, one a line) in the browser's storage; null without any. Off with ?nowarmtextures.
EM_JS(char*, MuWeb_LoadWarmTextures, (), {
    try {
        const text = localStorage.getItem("mu-web/warm-textures");
        return text ? stringToNewUTF8(text) : 0;
    } catch (e) {
        return 0;
    }
});
EM_JS(void, MuWeb_SaveWarmTextures, (const char* text), {
    try { localStorage.setItem("mu-web/warm-textures", UTF8ToString(text)); } catch (e) { /* no storage */ }
});
EM_JS(int, MuWeb_WarmTexturesOff, (), { return new URLSearchParams(location.search).has("nowarmtextures") ? 1 : 0; });
} // namespace

namespace Render::TextureWarmUp
{
namespace
{
// After entering the world: the map's own loads, the late pack and the window warm-up's start come first.
constexpr double kStartDelayMs = 4000.0;
// Only after a frame this short; after a very short one, a second texture if the first was quick.
constexpr double kLightFrameMs = 33.0;
constexpr double kVeryLightFrameMs = 20.0;
constexpr double kSecondTextureWithinMs = 4.0;
// One texture that took longer (its file downloaded alone, a slow device): no more warm-up this session.
constexpr double kStopAfterMs = 50.0;
// The learned list is written to the browser's storage at most this often while it changes.
constexpr double kSaveEveryMs = 15000.0;

struct State
{
    bool loaded = false;
    bool off = false;
    Core::Utilities::RecentNames learned{kCapacity};
    bool dirty = false;
    double savedAt = 0.0;
    // Set while the warm-up itself loads a texture: that isn't the player needing it.
    bool warming = false;

    // The names learned in earlier sessions, the most recent first, and how far the warm-up got.
    std::vector<std::string> toWarm;
    std::size_t next = 0;
    bool indexed = false;
    std::unordered_map<std::string, GLuint> pendingByName;
    double inWorldSince = 0.0;

    bool stopped = false;
    int warmed = 0;
    int skipped = 0;
    double totalMs = 0.0;
    double longestMs = 0.0;
    bool logged = false;
};

State& Warm()
{
    static State state;
    return state;
}

void EnsureLoaded(State& state)
{
    if (state.loaded)
        return;
    state.loaded = true;
    state.off = MuWeb_WarmTexturesOff() != 0;
    if (char* text = MuWeb_LoadWarmTextures())
    {
        state.learned.Load(text);
        std::free(text);
    }
    const auto& names = state.learned.Names();
    state.toWarm.assign(names.rbegin(), names.rend());
}

void SaveIfDue(State& state, double now)
{
    if (!state.dirty || now - state.savedAt < kSaveEveryMs)
        return;
    MuWeb_SaveWarmTextures(state.learned.Join().c_str());
    state.dirty = false;
    state.savedAt = now;
}

void LogDone(State& state)
{
    if (state.logged)
        return;
    state.logged = true;
    if (state.toWarm.empty())
        return;
    mu::log::Get("render")->info("Texture warm-up: {} of {} learned textures in {:.0f} ms (at most {:.1f} ms a frame), "
                                 "{} loaded already or gone{}",
                                 state.warmed, state.toWarm.size(), state.totalMs, state.longestMs, state.skipped,
                                 state.stopped ? "; stopped after a texture over 50 ms" : "");
}

// Restarts the wait on entering the world; false outside it.
bool InWorldLongEnough(State& state, double now)
{
    if (SceneFlag != MAIN_SCENE || Hero == nullptr)
    {
        state.inWorldSince = 0.0;
        return false;
    }
    if (state.inWorldSince == 0.0)
        state.inWorldSince = now;
    return now - state.inWorldSince >= kStartDelayMs;
}

// The numbers of the textures still recorded, by file name, once (the first light frame).
void IndexPending(State& state)
{
    state.indexed = true;
    for (const auto& [index, fileName] : Bitmaps.PendingLoads())
    {
        state.pendingByName.emplace(mu_wchar_to_utf8(fileName.c_str()), index);
    }
}

// Loads the next learned texture still recorded; false when none is left. `ms`: the time it took.
bool WarmNext(State& state, double& ms)
{
    while (state.next < state.toWarm.size())
    {
        const auto found = state.pendingByName.find(state.toWarm[state.next++]);
        if (found == state.pendingByName.end())
        {
            ++state.skipped;
            continue;
        }
        const double start = Core::FirstOpenTiming::NowMs();
        state.warming = true;
        const bool loaded = Bitmaps.ResolvePendingLoad(found->second);
        state.warming = false;
        state.pendingByName.erase(found);
        if (!loaded)
        {
            ++state.skipped;
            continue;
        }
        ms = Core::FirstOpenTiming::NowMs() - start;
        ++state.warmed;
        return true;
    }
    return false;
}
} // namespace

void NoteResolved(const std::wstring& fileName)
{
    State& state = Warm();
    // Only what the game needed in the world, not the login and character screens' own.
    if (state.warming || SceneFlag != MAIN_SCENE)
        return;
    EnsureLoaded(state);
    if (state.learned.Note(mu_wchar_to_utf8(fileName.c_str())))
        state.dirty = true;
}

void Tick(double lastFrameMs)
{
    State& state = Warm();
    EnsureLoaded(state);
    const double now = Core::FirstOpenTiming::NowMs();
    SaveIfDue(state, now);

    if (state.off || state.stopped || state.next >= state.toWarm.size())
    {
        if (state.indexed)
            LogDone(state);
        return;
    }
    if (!InWorldLongEnough(state, now) || !(lastFrameMs < kLightFrameMs) || !mu::web::LatePackInstalled())
        return;
    if (!state.indexed)
    {
        IndexPending(state);
        return;
    }

    double frameMs = 0.0;
    double ms = 0.0;
    if (!WarmNext(state, ms))
        return;
    frameMs += ms;
    state.stopped = ms > kStopAfterMs;
    if (!state.stopped && lastFrameMs < kVeryLightFrameMs && ms < kSecondTextureWithinMs && WarmNext(state, ms))
    {
        frameMs += ms;
        state.stopped = ms > kStopAfterMs;
    }
    state.totalMs += frameMs;
    state.longestMs = std::max(state.longestMs, frameMs);
}
} // namespace Render::TextureWarmUp
#endif
