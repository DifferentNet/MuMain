#pragma once

#include "UI/HUD/LiveGaugeMath.h"
#include "UI/HUD/LiveGaugeMesh.h"

#include <array>
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
// life gem, a wave and slosh, a crest, a glass highlight and a sheen (UI/HUD/LiveGaugeMesh.h). Shapes, shades and
// timings come from the theme's [LiveHud] section (LiveGauge::LiveHudConfig). CMainFrameWindow owns it.
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
        bool poisoned = false;
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
        bool poisoned = false;

        bool operator==(const MeshKey&) const = default;
    };
    struct UnderKey
    {
        LiveGauge::Box box;
        float rimWidth = -1.f;

        bool operator==(const UnderKey&) const = default;
    };

    // One gem: its elements (main_frame.rml's orb_<name>_*), its motion and its reused geometry.
    struct Gem
    {
        Rml::Element* box = nullptr;
        Render::RmlUi::AdditiveMeshElement* under = nullptr;
        Render::RmlUi::AdditiveMeshElement* body = nullptr;
        Render::RmlUi::AdditiveMeshElement* shine = nullptr;
        // The body's data-src and data-src-alt (the art while poisoned).
        std::string source;
        std::string poisonedSource;

        float wavePeriod = 1.f;
        double sheenOffset = 0.0;
        bool ghosted = false;

        bool started = false;
        float shown = 0.f;
        float lastTarget = 0.f;
        LiveGauge::GhostTracker ghost;
        LiveGauge::SloshTracker slosh;

        LiveGauge::GemShape shape;
        std::vector<float> surface;
        std::vector<LiveGauge::Vertex> underQuads;
        std::vector<LiveGauge::Vertex> bodyQuads;
        std::vector<LiveGauge::Vertex> shineQuads;
        UnderKey underKey;
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
    bool BindGem(Rml::ElementDocument* document, const char* name, Gem& gem);
    void ReadConfig();
    static void Advance(Gem& gem, float target, double now, float dt);
    void Draw(Gem& gem, bool poisoned, double tick);
    void DrawUnder(Gem& gem, const LiveGauge::Box& box);

    Rml::ElementDocument* m_document = nullptr;
    bool m_bound = false;
    float m_dpRatio = 1.f;
    double m_lastNow = -1.0;
    double m_lastTick = -1.0;
    LiveGauge::LiveHudConfig m_config;
    std::array<Gem, GemCount> m_gems;
};
} // namespace UI::HUD
