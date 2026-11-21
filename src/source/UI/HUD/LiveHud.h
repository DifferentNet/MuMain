#pragma once

#include "UI/HUD/LiveGaugeMath.h"
#include "UI/HUD/LiveGaugeMesh.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Rml
{
class Element;
class ElementDocument;
} // namespace Rml

namespace Render::RmlUi
{
class AdditiveMeshElement;
}

// The live gems of a theme that declares LiveHud=1 (UI::RmlBridge::ThemeHasLiveHud()). main_frame.rml, shared by
// every theme, carries hidden `live-only` elements that such a theme shows and places (its main_frame_live.rcss); this
// fills their meshes (<additive-mesh>) from the hero's values: liquid gems with an eased level, a damage ghost on the
// life gem, a wave and slosh, a crest, a glass highlight and a sheen, a halo round each gem that pulses with the life
// gem while life is low, and the SD and AG rings round them, the SD one with its own damage ghost
// (UI/HUD/LiveGaugeMesh.h). On the skill row each slot gets a dark sweep over what is left of its cooldown, the whole
// seconds left and a flash when it is ready again; on the EXP bar a spark breathes at the fill's end, flares on each
// EXP gain, and a level-up sends a band of light across the bar and the sheen over both gems. Shapes, shades and
// timings come from the theme's [LiveHud] section (LiveGauge::LiveHudConfig). CMainFrameWindow owns it.
namespace UI::HUD
{
class LiveHud
{
public:
    // The skill slots with a sweep: the hotkey row's five, then the current skill's.
    static constexpr std::size_t SkillSlotCount = 6;

    // The hero's values, 0..1 of each maximum.
    struct Inputs
    {
        float life = 0.f;
        float mana = 0.f;
        float shield = 0.f;
        float skillMana = 0.f;
        bool poisoned = false;
        // A skill slot's cooldown: the share of it left (0 = ready) and its milliseconds left, and the skill the slot
        // shows (-1: none). Another skill in the slot (the row's other set, another current skill) is no cooldown
        // ending, so it makes no ready flash.
        struct Cooldown
        {
            float fraction = 0.f;
            int remainingMs = 0;
            int skill = -1;
        };
        // main_frame.rml's skill_slot_0..4, then current_skill_slot.
        std::array<Cooldown, SkillSlotCount> cooldowns{};
        // The EXP bar's fill, 0..1 of the bar (main_frame.rml's exp_fraction).
        float expFill = 0.f;
        // How many EXP gains the hero has had: each new one flares the spark.
        std::uint32_t expGains = 0;
        // The hero (any value that differs between characters) and its level: the level rising for the same hero
        // plays the level-up cue.
        std::uint64_t hero = 0;
        std::int64_t level = 0;
        // Options > Interface "HUD animation" (config.ini [UI] HudEffects). Off: the liquid lies flat, and there is
        // no sheen, no low-life pulse, no ready flash and no level-up cue, and the EXP spark stands still without its
        // flare; the levels still ease, the ghosts still drain, and the cooldowns still show their sweep and seconds.
        bool effects = true;
    };

    // Every frame the PC HUD shows under a LiveHud theme; `now` in seconds. Looks the elements up again when
    // `document` is a new one (a reload or a theme switch: [LiveHud] is read again then), and does nothing for a
    // document without them. A mesh is rebuilt at most every 1/30 s, and only when what it shows changed.
    void Sync(Rml::ElementDocument* document, const Inputs& inputs, double now);
    // Forgets the document and the gems' motion: the phone HUD, a theme without LiveHud, a reload. Cheap when
    // nothing is bound.
    void Unbind();

private:
    // What a gem's mesh was last built from.
    struct MeshKey
    {
        LiveGauge::Box box;
        float level = -1.f;
        float ghost = -1.f;
        double waveTime = -1.0;
        float waveAmplitude = -1.f;
        float sheenPhase = -2.f;
        float brighten = -1.f;
        bool poisoned = false;

        bool operator==(const MeshKey&) const = default;
    };
    // The socket and the ring's track: rebuilt only when the layout or the dp ratio changes.
    struct UnderKey
    {
        LiveGauge::Box box;
        LiveGauge::Box ring;
        float rimWidth = -1.f;
        float trackWidth = -1.f;

        bool operator==(const UnderKey&) const = default;
    };
    struct HaloKey
    {
        LiveGauge::Box box;
        LiveGauge::Box ring;
        float strength = -1.f;
        std::uint32_t colour = 0u;

        bool operator==(const HaloKey&) const = default;
    };
    struct ArcKey
    {
        LiveGauge::Box box;
        LiveGauge::Box ring;
        float level = -1.f;
        float ghost = -1.f;
        float dpRatio = -1.f;

        bool operator==(const ArcKey&) const = default;
    };

    // A level on screen: eased toward the hero's, with a damage ghost when `ghosted`, and the latest slosh.
    struct Motion
    {
        bool ghosted = false;
        bool started = false;
        float shown = 0.f;
        float lastTarget = 0.f;
        LiveGauge::GhostTracker ghost;
        LiveGauge::SloshTracker slosh;

        // The ghost's level while one is up; 0 otherwise, which shows none.
        [[nodiscard]] float GhostLevel() const
        {
            return ghosted && ghost.Active() ? ghost.Level() : 0.f;
        }
    };

    // The SD or AG ring round a gem (main_frame.rml's orb_<name>_arc and orb_<name>_cap): its value arc, drawn with
    // the bar's art, and the bright cap added at the value's end. Its track is the gem's `under`.
    struct Arc
    {
        Render::RmlUi::AdditiveMeshElement* mesh = nullptr;
        Render::RmlUi::AdditiveMeshElement* cap = nullptr;
        std::string source;
        Motion motion;
        std::vector<LiveGauge::Vertex> quads;
        std::vector<LiveGauge::Vertex> capQuads;
        ArcKey key;
    };

    // How a gem looks this tick, beside its level.
    struct Look
    {
        bool poisoned = false;
        bool effects = true;
        // The low-life pulse, 0..1, while `pulsing`.
        bool pulsing = false;
        float pulse = 0.f;
        // The level-up's sheen, 0..1 while it crosses the gem in place of the usual one; -1 otherwise.
        float levelUp = -1.f;
    };

    // One gem: its elements (main_frame.rml's orb_<name>_*), its motion and its reused geometry.
    struct Gem
    {
        Rml::Element* box = nullptr;
        Render::RmlUi::AdditiveMeshElement* under = nullptr;
        Render::RmlUi::AdditiveMeshElement* halo = nullptr;
        Render::RmlUi::AdditiveMeshElement* body = nullptr;
        Render::RmlUi::AdditiveMeshElement* shine = nullptr;
        // The body's data-src and data-src-alt (the art while poisoned).
        std::string source;
        std::string poisonedSource;
        // The halo's colour, and while poisoned.
        std::uint32_t haloColour = 0u;
        std::uint32_t poisonedHaloColour = 0u;

        float wavePeriod = 1.f;
        double sheenOffset = 0.0;
        Motion motion;
        Arc arc;

        LiveGauge::GemShape shape;
        std::vector<float> surface;
        std::vector<LiveGauge::Vertex> underQuads;
        std::vector<LiveGauge::Vertex> haloQuads;
        std::vector<LiveGauge::Vertex> bodyQuads;
        std::vector<LiveGauge::Vertex> shineQuads;
        UnderKey underKey;
        HaloKey haloKey;
        MeshKey bodyKey;
        MeshKey shineKey;
    };

    enum GemIndex
    {
        LifeGem,
        ManaGem,
        GemCount,
    };

    // A skill slot's live parts (main_frame.rml's .skill-sweep, .skill-ready and .skill-cd-secs in the slot): the
    // sweep, the ready flash (the slot's class `ready`, which main_frame_live.rcss animates on .skill-ready) and the
    // seconds left.
    struct SkillSlot
    {
        Rml::Element* slot = nullptr;
        Render::RmlUi::AdditiveMeshElement* sweep = nullptr;
        Rml::Element* seconds = nullptr;
        // The skill the flash follows: it starts again with another one.
        int skill = -1;
        LiveGauge::ReadyFlash flash;
        // When the `ready` class comes off again, while it is on.
        std::optional<double> readyUntil;
        // The number the slot shows (0: none); -1 until it is first set.
        int shownSeconds = -1;
        // What the sweep was last built from; -1 until it is first built.
        LiveGauge::Box sweepBox;
        float sweepFraction = -1.f;
        std::vector<LiveGauge::Vertex> quads;
    };

    // What the EXP spark (main_frame.rml's exp_spark) was last built from.
    struct SparkKey
    {
        LiveGauge::Box box;
        float fill = -1.f;
        float radius = -1.f;
        float strength = -1.f;
        float bandPhase = -2.f;

        bool operator==(const SparkKey&) const = default;
    };

    void Bind(Rml::ElementDocument* document);
    bool BindGem(Rml::ElementDocument* document, const char* name, const char* arcName, Gem& gem);
    void BindSkills(Rml::ElementDocument* document);
    void ReadConfig();
    static void Advance(Motion& motion, float target, double now, float dt);
    void Draw(Gem& gem, const Look& look, double tick);
    void DrawUnder(Gem& gem);
    void DrawHalo(Gem& gem, const Look& look);
    void DrawArc(Gem& gem);
    // Notices this tick's EXP gain and level-up.
    void Cue(const Inputs& inputs, double tick);
    void DrawSkill(SkillSlot& slot, const Inputs::Cooldown& cooldown, bool effects, double tick);
    void DrawSpark(const Inputs& inputs, float levelUp, double tick);
    [[nodiscard]] LiveGauge::Ring TrackRing(const LiveGauge::Box& gem, const LiveGauge::Box& ring) const;

    Rml::ElementDocument* m_document = nullptr;
    bool m_bound = false;
    float m_dpRatio = 1.f;
    double m_lastNow = -1.0;
    double m_lastTick = -1.0;
    LiveGauge::LiveHudConfig m_config;
    std::array<Gem, GemCount> m_gems;
    std::array<SkillSlot, SkillSlotCount> m_slots;

    Render::RmlUi::AdditiveMeshElement* m_spark = nullptr;
    std::vector<LiveGauge::Vertex> m_sparkQuads;
    SparkKey m_sparkKey;
    // The EXP gains and the level seen so far, and when the latest flare and level-up cue started.
    bool m_cuesStarted = false;
    std::uint32_t m_expGains = 0;
    LiveGauge::LevelUpDetector m_levelUp;
    std::optional<double> m_flareStart;
    std::optional<double> m_levelUpStart;
};
} // namespace UI::HUD
