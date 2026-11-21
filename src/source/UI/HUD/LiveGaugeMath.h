#pragma once

// The live HUD's numbers (a theme with LiveHud=1, UI/HUD/LiveHud.h): how a gem's shown level follows the hero's,
// the damage ghost, the liquid's wave and slosh, the SD/AG arcs' span, the low-life pulse, the skills' ready flash,
// the level-up cue, and the theme's [LiveHud] settings. Pure: no RmlUi, no game state.

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace UI::HUD::LiveGauge
{
// A point of a gem's outline, 0..1 of the gem's box (x to the right, y down).
struct OutlinePoint
{
    float x = 0.f;
    float y = 0.f;
};

// The octagon the gems are cut to: `25,3 75,3 96,32 96,70 68,97 32,97 5,70 5,32` in percent of the gem's box.
[[nodiscard]] std::vector<OutlinePoint> DefaultOutline();

// A [LiveHud] GemOutline value: at least three `x,y` pairs in percent (0..100) of the gem's box, separated by spaces,
// forming a convex polygon (each column of the gem is one span of it). Anything else gives DefaultOutline().
[[nodiscard]] std::vector<OutlinePoint> ParseOutline(std::string_view text);

// themes/<name>/theme.ini's [LiveHud] section; every key is optional and these are the defaults. Sizes are dp,
// times seconds, angles degrees (screen angles, clockwise from 3 o'clock), colours 0xAABBGGRR as the meshes take them.
struct LiveHudConfig
{
    std::vector<OutlinePoint> gemOutline = DefaultOutline();
    // The gem art is zoomed this much about its centre, which cuts off the art's corner curls.
    float gemZoom = 1.32f;
    // The art's brightness above the liquid.
    float emptyShade = 0.30f;
    float waveAmplitude = 1.2f;
    float wavePeriodLife = 2.6f;
    float wavePeriodMana = 3.3f;
    float arcStart = 140.f;
    float arcSweep = 260.f;
    float arcWidth = 3.5f;
    // The life fraction under which the life gem pulses.
    float lowLife = 0.30f;
    float pulsePeriod = 1.2f;
    float pulseStrength = 0.22f;
    std::uint32_t haloLife = 0xFF1834FFu;   // #ff3418
    std::uint32_t haloMana = 0xFFFF7028u;   // #2870ff
    std::uint32_t haloPoison = 0xFF40C030u; // #30c040
    std::uint32_t sweepShade = 0xCC030406u; // #060403cc
    // The socket's rim round the gem: bronze outside, almost black at the gem.
    std::uint32_t rimOuter = 0xFF2A465Cu; // #5c462a
    std::uint32_t rimInner = 0xFF07090Bu; // #0b0907
    float rimWidth = 1.5f;
    // The crest's brightness at the liquid's surface (UI/HUD/LiveGaugeMesh.h, ShineInputs).
    float crestTint = 0.45f;
};

// The section's values by lower-case key, as UI::RmlBridge::ThemeInheritance reads them; a missing or unreadable
// value keeps its default.
[[nodiscard]] LiveHudConfig ReadLiveHudConfig(const std::map<std::string, std::string>& section);

// `#rrggbb` or `#rrggbbaa` as 0xAABBGGRR; `fallback` for anything else.
[[nodiscard]] std::uint32_t ParseColour(std::string_view text, std::uint32_t fallback);

// The shown level one frame later: it eases toward `target` with a 0.12 s time constant, `dt` (seconds since the
// last frame) clamped to 0.1 s, and lands on it once within 0.001.
[[nodiscard]] float EaseToward(float shown, float target, float dt);

// The damage ghost of the life and SD gems: after a drop of at least 2 % of the maximum it keeps the level shown
// before the drop for 0.35 s, then drains at 1.2 levels a second, never below the shown level. Gains and smaller drops
// (MU ticks life all the time) make none.
class GhostTracker
{
public:
    // target: the hero's level now; shown: the gem's eased level; now: seconds.
    void Update(float target, float shown, double now);
    // The ghost's level; the shown level while there is no ghost.
    [[nodiscard]] float Level() const;
    // Whether a ghost is up (held or draining); it shows above the shown level once the gem eases down.
    [[nodiscard]] bool Active() const
    {
        return m_active;
    }

private:
    float m_level = 0.f;
    float m_shown = 0.f;
    float m_lastTarget = 0.f;
    double m_lastTime = 0.0;
    double m_holdUntil = 0.0;
    bool m_started = false;
    bool m_active = false;
};

// How much of the wave shows at `level`: none at empty and full, all of it from 4 % away from either.
[[nodiscard]] float WaveFade(float level);

// The liquid surface's offset (down is positive) at `x`, 0..1 of the gem's width, at time `t`: two sines with
// wavelengths 0.55 and 0.31 widths and periods `period` and 0.7 `period`, weighted 0.65 and 0.35, times WaveFade(level)
// and `amplitude`. Its own period is 7 `period`, where both sines repeat.
[[nodiscard]] float WaveOffset(float x, double t, float period, float level, float amplitude);

// The wave's amplitude factor `age` seconds after a value change of `delta` levels: 1 + min(1.5, 12 |delta|),
// the extra fading out linearly over 0.6 s.
[[nodiscard]] float SloshGain(float delta, double age);

// The latest value change a gem sloshes from.
class SloshTracker
{
public:
    // A change of `delta` levels at `now`; it replaces the running slosh when it sloshes harder from now on.
    void Kick(float delta, double now);
    [[nodiscard]] float Gain(double now) const;

private:
    float m_delta = 0.f;
    double m_time = 0.0;
};

// The angles an SD/AG arc fills at `level` (clamped to 0..1): from `start` over `level` of `sweep`.
struct ArcRange
{
    float startDegrees = 0.f;
    float endDegrees = 0.f;
};
[[nodiscard]] ArcRange ArcSpan(float level, float startDegrees, float sweepDegrees);

// Whether the life gem pulses: 0 < `life` < `lowLife` (a dead hero gets none).
[[nodiscard]] bool IsLowLife(float life, float lowLife);

// The low-life pulse at time `t`, 0..1 on a sine of `period` seconds, while IsLowLife(); 0 otherwise.
[[nodiscard]] float PulseLevel(float life, float lowLife, float period, double t);

// How strong a gem's halo glows: 0.10 + 0.25 `level` (the gem's shown level, clamped to 0..1); while the low-life
// pulse runs (`pulsing`), 0.25 at its dimmest to 0.60 at its brightest instead (`pulse` 0..1, PulseLevel()).
[[nodiscard]] float HaloStrength(float level, bool pulsing, float pulse);

// A skill slot's cooldown ending: Update() is true once when the fraction left falls to 0 from above it.
class ReadyFlash
{
public:
    [[nodiscard]] bool Update(float cooldownFraction);

private:
    float m_last = 0.f;
};

// A level-up: Update() is true when the same hero's level rises. A new hero (or the first call) only records the
// level; the EXP bar wrapping within a level never counts.
class LevelUpDetector
{
public:
    [[nodiscard]] bool Update(std::uint64_t hero, std::int64_t level);

private:
    std::uint64_t m_hero = 0;
    std::int64_t m_level = 0;
    bool m_started = false;
};
} // namespace UI::HUD::LiveGauge
