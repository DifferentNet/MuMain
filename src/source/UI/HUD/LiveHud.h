#pragma once

#include "UI/HUD/LiveGaugeMath.h"
#include "UI/HUD/LiveGaugeMesh.h"

#include <array>
#include <cstdint>
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
// (UI/HUD/LiveGaugeMesh.h). Shapes, shades and timings come from the theme's [LiveHud] section
// (LiveGauge::LiveHudConfig). CMainFrameWindow owns it.
namespace UI::HUD
{
class LiveHud
{
public:
    // The hero's values, 0..1 of each maximum.
    struct Inputs
    {
        float life = 0.f;
        float mana = 0.f;
        float shield = 0.f;
        float skillMana = 0.f;
        bool poisoned = false;
        // Options > Interface "HUD animation" (config.ini [UI] HudEffects). Off: the liquid lies flat, and there is
        // no sheen and no low-life pulse; the levels still ease and the ghosts still drain.
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

    void Bind(Rml::ElementDocument* document);
    bool BindGem(Rml::ElementDocument* document, const char* name, const char* arcName, Gem& gem);
    void ReadConfig();
    static void Advance(Motion& motion, float target, double now, float dt);
    void Draw(Gem& gem, const Look& look, double tick);
    void DrawUnder(Gem& gem);
    void DrawHalo(Gem& gem, const Look& look);
    void DrawArc(Gem& gem);
    [[nodiscard]] LiveGauge::Ring TrackRing(const LiveGauge::Box& gem, const LiveGauge::Box& ring) const;

    Rml::ElementDocument* m_document = nullptr;
    bool m_bound = false;
    float m_dpRatio = 1.f;
    double m_lastNow = -1.0;
    double m_lastTick = -1.0;
    LiveGauge::LiveHudConfig m_config;
    std::array<Gem, GemCount> m_gems;
};
} // namespace UI::HUD
