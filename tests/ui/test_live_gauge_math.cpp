#include "doctest.h"

#include "UI/HUD/LiveGaugeMath.h"
#include "UI/HUD/LiveHudTest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace UI::HUD::LiveGauge;

namespace
{
constexpr float Frame = 1.f / 60.f;

bool SameOutline(const std::vector<OutlinePoint>& a, const std::vector<OutlinePoint>& b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (std::fabs(a[i].x - b[i].x) > 1e-6f || std::fabs(a[i].y - b[i].y) > 1e-6f)
            return false;
    }
    return true;
}
} // namespace

TEST_CASE("the shown level eases toward the hero's and lands on it [ui][live_gauge]")
{
    SUBCASE("rising and falling, it never passes the target")
    {
        for (const auto [from, to] : {std::pair{0.f, 1.f}, std::pair{1.f, 0.f}, std::pair{0.2f, 0.65f}})
        {
            CAPTURE(from);
            CAPTURE(to);
            float shown = from;
            float distance = std::fabs(to - from);
            int frames = 0;
            while (shown != to && frames < 600)
            {
                shown = EaseToward(shown, to, Frame);
                const float next = std::fabs(to - shown);
                CHECK(next < distance);
                CHECK(((to > from) ? shown <= to : shown >= to));
                distance = next;
                ++frames;
            }
            CHECK(shown == to);
            // About eight time constants of 0.12 s to come within 0.001.
            CHECK(frames > 30);
            CHECK(frames < 90);
        }
    }

    SUBCASE("a long frame is one step of 0.1 s, a negative one none")
    {
        CHECK(EaseToward(0.f, 1.f, 5.f) == doctest::Approx(EaseToward(0.f, 1.f, 0.1f)));
        CHECK(EaseToward(0.f, 1.f, 0.1f) < 1.f);
        CHECK(EaseToward(0.3f, 1.f, -1.f) == doctest::Approx(0.3f));
        CHECK(EaseToward(0.5f, 0.5f, Frame) == 0.5f);
    }

    SUBCASE("one time constant covers 63 % of the way")
    {
        CHECK(EaseToward(0.f, 1.f, 0.1f) == doctest::Approx(1.f - std::exp(-0.1f / 0.12f)));
    }
}

TEST_CASE("the damage ghost holds the level before a drop, then drains [ui][live_gauge]")
{
    SUBCASE("no ghost for a drop under 2 % or for a gain")
    {
        GhostTracker ghost;
        ghost.Update(1.f, 1.f, 0.0);
        ghost.Update(0.99f, 0.99f, 0.1);
        CHECK_FALSE(ghost.Active());
        CHECK(ghost.Level() == doctest::Approx(0.99f));
        ghost.Update(0.98f, 0.98f, 0.2);
        CHECK_FALSE(ghost.Active());
        // The hit's own frame, the gem not moved yet: the ghost waits above it.
        ghost.Update(0.5f, 0.98f, 0.3);
        CHECK(ghost.Active());
        ghost.Update(0.5f, 0.9f, 0.32);
        CHECK(ghost.Level() == doctest::Approx(0.98f));

        GhostTracker gain;
        gain.Update(0.5f, 0.5f, 0.0);
        gain.Update(0.9f, 0.6f, 0.1);
        CHECK_FALSE(gain.Active());
        CHECK(gain.Level() == doctest::Approx(0.6f));
    }

    SUBCASE("a hit: held 0.35 s, drained at 1.2 a second, never under the shown level")
    {
        GhostTracker ghost;
        ghost.Update(1.f, 1.f, 0.0);
        // The hit lands; the gem has started to ease down.
        ghost.Update(0.5f, 0.9f, 0.1);
        CHECK(ghost.Active());
        CHECK(ghost.Level() == doctest::Approx(1.f));
        ghost.Update(0.5f, 0.6f, 0.3);
        CHECK(ghost.Level() == doctest::Approx(1.f));
        ghost.Update(0.5f, 0.55f, 0.45);
        CHECK(ghost.Level() == doctest::Approx(1.f));
        // Drained 0.1 s past the hold's end (0.45).
        ghost.Update(0.5f, 0.5f, 0.55);
        CHECK(ghost.Active());
        CHECK(ghost.Level() == doctest::Approx(1.f - 1.2f * 0.1f));
        ghost.Update(0.5f, 0.5f, 0.75);
        CHECK(ghost.Level() == doctest::Approx(1.f - 1.2f * 0.3f));
        // It would pass the shown level: the ghost is gone.
        ghost.Update(0.5f, 0.5f, 2.0);
        CHECK_FALSE(ghost.Active());
        CHECK(ghost.Level() == doctest::Approx(0.5f));
    }

    SUBCASE("a second hit while the ghost shows keeps the higher ghost and holds again")
    {
        GhostTracker ghost;
        ghost.Update(1.f, 1.f, 0.0);
        ghost.Update(0.6f, 0.95f, 0.1);
        ghost.Update(0.6f, 0.6f, 0.3);
        ghost.Update(0.3f, 0.55f, 0.4);
        CHECK(ghost.Level() == doctest::Approx(1.f));
        ghost.Update(0.3f, 0.3f, 0.7);
        CHECK(ghost.Level() == doctest::Approx(1.f));
    }

    SUBCASE("the ghost never shows under the shown level")
    {
        GhostTracker ghost;
        ghost.Update(1.f, 1.f, 0.0);
        ghost.Update(0.4f, 1.f, 0.1);
        // Healed above the ghost while it holds.
        ghost.Update(1.f, 1.f, 0.2);
        CHECK(ghost.Level() >= 1.f);
    }
}

TEST_CASE("the liquid's wave fades at empty and full and stays within its amplitude [ui][live_gauge]")
{
    CHECK(WaveFade(0.f) == 0.f);
    CHECK(WaveFade(1.f) == 0.f);
    CHECK(WaveFade(0.02f) == doctest::Approx(0.5f));
    CHECK(WaveFade(0.98f) == doctest::Approx(0.5f));
    CHECK(WaveFade(0.5f) == 1.f);
    CHECK(WaveFade(0.04f) == doctest::Approx(1.f));

    constexpr float Period = 2.6f;
    constexpr float Amplitude = 1.2f;
    float largest = 0.f;
    for (int step = 0; step < 400; ++step)
    {
        const double t = step * 0.037;
        for (const float x : {0.f, 0.1f, 0.33f, 0.5f, 0.77f, 1.f})
        {
            CHECK(WaveOffset(x, t, Period, 0.f, Amplitude) == 0.f);
            CHECK(WaveOffset(x, t, Period, 1.f, Amplitude) == 0.f);
            const float offset = WaveOffset(x, t, Period, 0.5f, Amplitude);
            CHECK(std::fabs(offset) <= Amplitude + 1e-5f);
            largest = std::max(largest, std::fabs(offset));
        }
    }
    // It moves: not a flat line.
    CHECK(largest > 0.5f * Amplitude);
    CHECK(WaveOffset(0.3f, 1.0, 0.f, 0.5f, Amplitude) == 0.f);
}

TEST_CASE("the wave repeats after seven periods, where both sines do [ui][live_gauge]")
{
    constexpr float Period = 3.3f;
    for (const double t : {0.0, 1.234, 17.5, 3600.25})
    {
        CAPTURE(t);
        for (const float x : {0.f, 0.21f, 0.5f, 0.9f})
        {
            CAPTURE(x);
            CHECK(WaveOffset(x, t + 7.0 * Period, Period, 0.5f, 1.f) ==
                  doctest::Approx(WaveOffset(x, t, Period, 0.5f, 1.f)).epsilon(1e-3));
        }
    }
}

TEST_CASE("a change sloshes the wave for 0.6 s, a stronger change taking over [ui][live_gauge]")
{
    CHECK(SloshGain(0.f, 0.0) == 1.f);
    CHECK(SloshGain(0.05f, 0.0) == doctest::Approx(1.6f));
    CHECK(SloshGain(-0.05f, 0.0) == doctest::Approx(1.6f));
    CHECK(SloshGain(0.5f, 0.0) == doctest::Approx(2.5f));
    CHECK(SloshGain(0.05f, 0.3) == doctest::Approx(1.3f));
    CHECK(SloshGain(0.05f, 0.6) == doctest::Approx(1.f));
    CHECK(SloshGain(0.05f, 5.0) == doctest::Approx(1.f));
    CHECK(SloshGain(0.05f, -1.0) == doctest::Approx(1.6f));

    SloshTracker slosh;
    CHECK(slosh.Gain(10.0) == 1.f);
    slosh.Kick(0.1f, 0.0);
    CHECK(slosh.Gain(0.0) == doctest::Approx(2.2f));
    // A small tick while the big one still sloshes harder: ignored.
    slosh.Kick(0.01f, 0.1);
    CHECK(slosh.Gain(0.1) == doctest::Approx(1.f + 1.2f * (1.f - 0.1f / 0.6f)));
    // Nearly faded: the small one takes over.
    slosh.Kick(0.01f, 0.59);
    CHECK(slosh.Gain(0.59) == doctest::Approx(1.12f));
    CHECK(slosh.Gain(2.0) == doctest::Approx(1.f));
}

TEST_CASE("an arc spans its level's share of the sweep [ui][live_gauge]")
{
    const ArcRange empty = ArcSpan(0.f, 140.f, 260.f);
    CHECK(empty.startDegrees == 140.f);
    CHECK(empty.endDegrees == 140.f);
    CHECK(ArcSpan(0.5f, 140.f, 260.f).endDegrees == doctest::Approx(270.f));
    CHECK(ArcSpan(1.f, 140.f, 260.f).endDegrees == doctest::Approx(400.f));
    CHECK(ArcSpan(-0.5f, 140.f, 260.f).endDegrees == 140.f);
    CHECK(ArcSpan(3.f, 140.f, 260.f).endDegrees == doctest::Approx(400.f));
}

TEST_CASE("the life gem pulses only while life is low and not zero [ui][live_gauge]")
{
    constexpr float LowLife = 0.3f;
    constexpr float Period = 1.2f;
    CHECK(PulseLevel(0.5f, LowLife, Period, 0.3) == 0.f);
    CHECK(PulseLevel(0.3f, LowLife, Period, 0.3) == 0.f);
    CHECK(PulseLevel(0.f, LowLife, Period, 0.3) == 0.f);
    CHECK(PulseLevel(0.2f, LowLife, 0.f, 0.3) == 0.f);
    CHECK(PulseLevel(0.2f, LowLife, Period, 0.0) == doctest::Approx(0.5f));
    CHECK(PulseLevel(0.2f, LowLife, Period, 0.3) == doctest::Approx(1.f));
    CHECK(PulseLevel(0.2f, LowLife, Period, 0.9) == doctest::Approx(0.f).epsilon(1e-5));
    for (int step = 0; step < 100; ++step)
    {
        const float pulse = PulseLevel(0.1f, LowLife, Period, step * 0.071);
        CHECK(pulse >= 0.f);
        CHECK(pulse <= 1.f);
    }
}

TEST_CASE("low life is above zero and under the threshold [ui][live_gauge]")
{
    CHECK(IsLowLife(0.29f, 0.3f));
    CHECK(IsLowLife(0.01f, 0.3f));
    CHECK_FALSE(IsLowLife(0.3f, 0.3f));
    CHECK_FALSE(IsLowLife(0.31f, 0.3f));
    CHECK_FALSE(IsLowLife(0.f, 0.3f));
    CHECK_FALSE(IsLowLife(-0.1f, 0.3f));
    CHECK_FALSE(IsLowLife(0.2f, 0.f));
}

TEST_CASE("a halo glows with its gem's level, and from 0.25 to 0.60 while it pulses [ui][live_gauge]")
{
    CHECK(HaloStrength(0.f, false, 0.f) == doctest::Approx(0.10f));
    CHECK(HaloStrength(0.5f, false, 0.f) == doctest::Approx(0.225f));
    CHECK(HaloStrength(1.f, false, 0.f) == doctest::Approx(0.35f));
    CHECK(HaloStrength(2.f, false, 0.f) == doctest::Approx(0.35f));
    CHECK(HaloStrength(-1.f, false, 0.f) == doctest::Approx(0.10f));
    CHECK(HaloStrength(1.f, false, 1.f) == doctest::Approx(0.35f));
    // While it pulses the level doesn't matter.
    CHECK(HaloStrength(0.2f, true, 0.f) == doctest::Approx(0.25f));
    CHECK(HaloStrength(0.2f, true, 1.f) == doctest::Approx(0.60f));
    CHECK(HaloStrength(0.1f, true, 0.5f) == doctest::Approx(0.425f));
    CHECK(HaloStrength(0.1f, true, 3.f) == doctest::Approx(0.60f));
    CHECK(HaloStrength(0.1f, true, -1.f) == doctest::Approx(0.25f));
}

TEST_CASE("a skill's ready flash fires once when its cooldown ends [ui][live_gauge]")
{
    ReadyFlash flash;
    CHECK_FALSE(flash.Update(0.f));
    CHECK_FALSE(flash.Update(0.5f));
    CHECK_FALSE(flash.Update(0.2f));
    CHECK(flash.Update(0.f));
    CHECK_FALSE(flash.Update(0.f));
    CHECK_FALSE(flash.Update(-0.1f));
    CHECK_FALSE(flash.Update(1.f));
    CHECK(flash.Update(-0.1f));
}

TEST_CASE("a level-up counts only the same hero's level rising [ui][live_gauge]")
{
    LevelUpDetector levels;
    CHECK_FALSE(levels.Update(1, 10));
    CHECK_FALSE(levels.Update(1, 10));
    CHECK(levels.Update(1, 11));
    CHECK_FALSE(levels.Update(1, 11));
    // Another character: only recorded.
    CHECK_FALSE(levels.Update(2, 50));
    CHECK(levels.Update(2, 51));
    CHECK_FALSE(levels.Update(2, 40));
}

TEST_CASE("a gem outline is a convex polygon in percent, or the octagon [ui][live_gauge]")
{
    const std::vector<OutlinePoint> octagon = DefaultOutline();
    REQUIRE(octagon.size() == 8);
    CHECK(SameOutline(ParseOutline("25,3 75,3 96,32 96,70 68,97 32,97 5,70 5,32"), octagon));

    const std::vector<OutlinePoint> triangle = ParseOutline("  10,20   90,20 50,90 ");
    REQUIRE(triangle.size() == 3);
    CHECK(triangle[0].x == doctest::Approx(0.1f));
    CHECK(triangle[0].y == doctest::Approx(0.2f));
    CHECK(triangle[2].x == doctest::Approx(0.5f));
    CHECK(triangle[2].y == doctest::Approx(0.9f));
    // Either winding.
    CHECK(ParseOutline("50,90 90,20 10,20").size() == 3);

    for (const char* bad :
         {"", "10,10 90,10", "10,10 90,10 x,90", "10,10 190,10 50,90", "10,10 90,10 50,-5", "10;10 90,10 50,90",
          "10,10 90,10 50,90 junk", "0,0 50,50 100,100", "0,0 100,0 50,20 100,100 0,100"})
    {
        CAPTURE(bad);
        CHECK(SameOutline(ParseOutline(bad), octagon));
    }
}

TEST_CASE("theme colours are #rrggbb or #rrggbbaa [ui][live_gauge]")
{
    constexpr std::uint32_t Fallback = 0x12345678u;
    CHECK(ParseColour("#ff3418", Fallback) == 0xFF1834FFu);
    CHECK(ParseColour("#FF3418", Fallback) == 0xFF1834FFu);
    CHECK(ParseColour("#2870ff", Fallback) == 0xFFFF7028u);
    CHECK(ParseColour("#060403cc", Fallback) == 0xCC030406u);
    for (const char* bad : {"", "#", "ff3418", "#ff341", "#ff34180", "#gg3418", "#ff3418 ; red"})
    {
        CAPTURE(bad);
        CHECK(ParseColour(bad, Fallback) == Fallback);
    }
}

TEST_CASE("[LiveHud] keeps its defaults for missing and unreadable values [ui][live_gauge]")
{
    const LiveHudConfig defaults = ReadLiveHudConfig({});
    CHECK(SameOutline(defaults.gemOutline, DefaultOutline()));
    CHECK(defaults.gemZoom == doctest::Approx(1.32f));
    CHECK(defaults.emptyShade == doctest::Approx(0.30f));
    CHECK(defaults.waveAmplitude == doctest::Approx(1.2f));
    CHECK(defaults.wavePeriodLife == doctest::Approx(2.6f));
    CHECK(defaults.wavePeriodMana == doctest::Approx(3.3f));
    CHECK(defaults.arcStart == doctest::Approx(140.f));
    CHECK(defaults.arcSweep == doctest::Approx(260.f));
    CHECK(defaults.arcWidth == doctest::Approx(3.5f));
    CHECK(defaults.lowLife == doctest::Approx(0.30f));
    CHECK(defaults.pulsePeriod == doctest::Approx(1.2f));
    CHECK(defaults.pulseStrength == doctest::Approx(0.22f));
    CHECK(defaults.haloLife == 0xFF1834FFu);
    CHECK(defaults.haloMana == 0xFFFF7028u);
    CHECK(defaults.haloPoison == 0xFF40C030u);
    CHECK(defaults.sweepShade == 0xCC030406u);
    CHECK(defaults.rimOuter == 0xFF2A465Cu);
    CHECK(defaults.rimInner == 0xFF07090Bu);
    CHECK(defaults.rimWidth == doctest::Approx(1.5f));
    CHECK(defaults.crestTint == doctest::Approx(0.45f));

    const std::map<std::string, std::string> set = {{"gemoutline", "10,20 90,20 50,90"},
                                                    {"gemzoom", "1.3"},
                                                    {"emptyshade", "0.4"},
                                                    {"waveamplitude", "2"},
                                                    {"waveperiodlife", "1.5"},
                                                    {"waveperiodmana", " 4 "},
                                                    {"arcstart", "-90"},
                                                    {"arcsweep", "360"},
                                                    {"arcwidth", "5"},
                                                    {"lowlife", "0.25"},
                                                    {"pulseperiod", "0.8"},
                                                    {"pulsestrength", "0.5"},
                                                    {"halolife", "#00ff00"},
                                                    {"halomana", "#0000ff"},
                                                    {"halopoison", "#ffff00"},
                                                    {"sweepshade", "#00000080"},
                                                    {"rimouter", "#102030"},
                                                    {"riminner", "#40506070"},
                                                    {"rimwidth", "2.5"},
                                                    {"cresttint", "0.6"}};
    const LiveHudConfig read = ReadLiveHudConfig(set);
    CHECK(read.gemOutline.size() == 3);
    CHECK(read.gemZoom == doctest::Approx(1.3f));
    CHECK(read.emptyShade == doctest::Approx(0.4f));
    CHECK(read.waveAmplitude == doctest::Approx(2.f));
    CHECK(read.wavePeriodLife == doctest::Approx(1.5f));
    CHECK(read.wavePeriodMana == doctest::Approx(4.f));
    CHECK(read.arcStart == doctest::Approx(-90.f));
    CHECK(read.arcSweep == doctest::Approx(360.f));
    CHECK(read.arcWidth == doctest::Approx(5.f));
    CHECK(read.lowLife == doctest::Approx(0.25f));
    CHECK(read.pulsePeriod == doctest::Approx(0.8f));
    CHECK(read.pulseStrength == doctest::Approx(0.5f));
    CHECK(read.haloLife == 0xFF00FF00u);
    CHECK(read.haloMana == 0xFFFF0000u);
    CHECK(read.haloPoison == 0xFF00FFFFu);
    CHECK(read.sweepShade == 0x80000000u);
    CHECK(read.rimOuter == 0xFF302010u);
    CHECK(read.rimInner == 0x70605040u);
    CHECK(read.rimWidth == doctest::Approx(2.5f));
    CHECK(read.crestTint == doctest::Approx(0.6f));

    // Out of range, not a number, a trailing comment, an upper-case key (ThemeInheritance lower-cases them).
    const std::map<std::string, std::string> bad = {
        {"gemzoom", "0.9"},      {"emptyshade", "1.5"},  {"waveamplitude", "-1"},
        {"waveperiodlife", "0"}, {"arcsweep", "400"},    {"lowlife", "0.3 ; life"},
        {"pulseperiod", "fast"}, {"halolife", "red"},    {"GemOutline", "10,20 90,20 50,90"},
        {"rimouter", "#5c46"},   {"riminner", "bronze"}, {"rimwidth", "-1"},
        {"cresttint", "1.5"}};
    const LiveHudConfig kept = ReadLiveHudConfig(bad);
    CHECK(SameOutline(kept.gemOutline, DefaultOutline()));
    CHECK(kept.gemZoom == doctest::Approx(1.32f));
    CHECK(kept.emptyShade == doctest::Approx(0.30f));
    CHECK(kept.waveAmplitude == doctest::Approx(1.2f));
    CHECK(kept.wavePeriodLife == doctest::Approx(2.6f));
    CHECK(kept.arcSweep == doctest::Approx(260.f));
    CHECK(kept.lowLife == doctest::Approx(0.30f));
    CHECK(kept.pulsePeriod == doctest::Approx(1.2f));
    CHECK(kept.haloLife == 0xFF1834FFu);
    CHECK(kept.rimOuter == 0xFF2A465Cu);
    CHECK(kept.rimInner == 0xFF07090Bu);
    CHECK(kept.rimWidth == doctest::Approx(1.5f));
    CHECK(kept.crestTint == doctest::Approx(0.45f));
}

TEST_CASE("$hudtest sets a gauge, the cooldown, poison and the cues, and off clears them [ui][live_gauge]")
{
    using UI::HUD::LiveHudTest::Apply;
    using UI::HUD::LiveHudTest::Overrides;

    Overrides overrides;
    CHECK(Apply(overrides, L"hp 0.25"));
    CHECK(Apply(overrides, L"mp 1"));
    CHECK(Apply(overrides, L"  sd   0.5  "));
    CHECK(Apply(overrides, L"ag 0"));
    CHECK(Apply(overrides, L"cd 3.5"));
    CHECK(Apply(overrides, L"poison on"));
    CHECK(Apply(overrides, L"exp"));
    CHECK(Apply(overrides, L"exp"));
    CHECK(Apply(overrides, L"lvl"));
    REQUIRE(overrides.life.has_value());
    CHECK(*overrides.life == doctest::Approx(0.25f));
    CHECK(*overrides.mana == doctest::Approx(1.f));
    CHECK(*overrides.shield == doctest::Approx(0.5f));
    CHECK(*overrides.skillMana == doctest::Approx(0.f));
    CHECK(*overrides.cooldownSeconds == doctest::Approx(3.5f));
    CHECK(*overrides.poisoned);
    CHECK(overrides.expGains == 2);
    CHECK(overrides.levelUps == 1);

    CHECK(Apply(overrides, L"poison off"));
    CHECK_FALSE(*overrides.poisoned);

    // Refused, and nothing changed.
    int index = 0;
    for (const wchar_t* bad : {L"", L"hp", L"hp 1.5", L"hp -0.1", L"hp half", L"hp 0.5 more", L"xx 0.5", L"cd -1",
                               L"poison", L"poison maybe", L"off now"})
    {
        CAPTURE(index);
        CHECK_FALSE(Apply(overrides, bad));
        ++index;
    }
    CHECK(*overrides.life == doctest::Approx(0.25f));
    CHECK(*overrides.cooldownSeconds == doctest::Approx(3.5f));
    CHECK(overrides.expGains == 2);

    CHECK(Apply(overrides, L"off"));
    CHECK_FALSE(overrides.life.has_value());
    CHECK_FALSE(overrides.mana.has_value());
    CHECK_FALSE(overrides.shield.has_value());
    CHECK_FALSE(overrides.skillMana.has_value());
    CHECK_FALSE(overrides.poisoned.has_value());
    CHECK_FALSE(overrides.cooldownSeconds.has_value());
    CHECK(overrides.expGains == 0);
    CHECK(overrides.levelUps == 0);
}
