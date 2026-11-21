#include "stdafx.h"

#include "UI/HUD/LiveHud.h"

#include "Render/RmlUi/RmlAdditiveMeshElement.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "UI/RmlBridge/ThemeInheritance.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>

namespace
{
namespace LiveGauge = UI::HUD::LiveGauge;

// Meshes follow the motion at this rate at most.
constexpr double TicksPerSecond = 30.0;
// The sheen crosses a gem in SheenRun seconds every SheenPeriod; the mana gem's runs half a period after the life
// gem's.
constexpr double SheenPeriod = 8.0;
constexpr double SheenRun = 0.9;
constexpr double ManaSheenOffset = 4.0;
constexpr float CrestHeightDp = 3.f;
constexpr float CrestTint = 0.45f;
constexpr float HighlightPeak = 0.12f;
constexpr float GhostShade = 0.60f;
constexpr float FillShade = 1.f;

constexpr const char* ThemesRoot = "Data/Interface/RmlUi/themes";
constexpr const char* LiveHudSection = "LiveHud";

Render::RmlUi::AdditiveMeshElement* MeshById(Rml::ElementDocument* document, const Rml::String& id)
{
    return rmlui_dynamic_cast<Render::RmlUi::AdditiveMeshElement*>(document->GetElementById(id));
}

// `inner`'s border box in pixels of `outer`'s.
LiveGauge::Box BoxWithin(Rml::Element* inner, Rml::Element* outer)
{
    const Rml::Vector2f offset =
        inner->GetAbsoluteOffset(Rml::BoxArea::Border) - outer->GetAbsoluteOffset(Rml::BoxArea::Border);
    const Rml::Vector2f size = inner->GetBox().GetSize(Rml::BoxArea::Border);
    return {offset.x, offset.y, size.x, size.y};
}

// 0..1 while the sheen crosses the gem, -1 between runs.
float SheenPhase(double tick, double offset)
{
    const double local = std::fmod(tick + offset, SheenPeriod);
    return local < SheenRun ? static_cast<float>(local / SheenRun) : -1.f;
}
} // namespace

void UI::HUD::LiveHud::Sync(Rml::ElementDocument* document, const Inputs& inputs, double now)
{
    if (document != m_document)
        Bind(document);
    if (!m_bound)
        return;

    const float dt = m_lastNow >= 0.0 ? static_cast<float>(now - m_lastNow) : 0.f;
    m_lastNow = now;
    Advance(m_gems[LifeGem], inputs.life, now, dt);
    Advance(m_gems[ManaGem], inputs.mana, now, dt);

    const double tick = std::floor(now * TicksPerSecond) / TicksPerSecond;
    if (tick == m_lastTick)
        return;
    m_lastTick = tick;
    if (const Rml::Context* context = document->GetContext())
        m_dpRatio = context->GetDensityIndependentPixelRatio();
    Draw(m_gems[LifeGem], inputs.poisoned, tick);
    Draw(m_gems[ManaGem], false, tick);
}

void UI::HUD::LiveHud::Unbind()
{
    if (m_document == nullptr && !m_bound)
        return;
    m_document = nullptr;
    m_bound = false;
    m_lastNow = -1.0;
    m_lastTick = -1.0;
    for (Gem& gem : m_gems)
    {
        gem.box = nullptr;
        gem.under = nullptr;
        gem.body = nullptr;
        gem.shine = nullptr;
        gem.started = false;
        gem.ghost = {};
        gem.slosh = {};
        gem.shape.xs.clear();
        gem.underKey = {};
        gem.bodyKey = {};
        gem.shineKey = {};
    }
}

void UI::HUD::LiveHud::Bind(Rml::ElementDocument* document)
{
    Unbind();
    m_document = document;
    if (document == nullptr)
        return;

    ReadConfig();
    Gem& life = m_gems[LifeGem];
    life.wavePeriod = m_config.wavePeriodLife;
    life.sheenOffset = 0.0;
    life.ghosted = true;
    Gem& mana = m_gems[ManaGem];
    mana.wavePeriod = m_config.wavePeriodMana;
    mana.sheenOffset = ManaSheenOffset;
    mana.ghosted = false;
    m_bound = BindGem(document, "hp", life) && BindGem(document, "mp", mana);
}

bool UI::HUD::LiveHud::BindGem(Rml::ElementDocument* document, const char* name, Gem& gem)
{
    const Rml::String prefix = Rml::String("orb_") + name;
    gem.box = document->GetElementById(prefix + "_box");
    gem.under = MeshById(document, prefix + "_under");
    gem.body = MeshById(document, prefix + "_body");
    gem.shine = MeshById(document, prefix + "_shine");
    if (gem.box == nullptr || gem.under == nullptr || gem.body == nullptr || gem.shine == nullptr)
        return false;
    gem.source = gem.body->GetAttribute<Rml::String>("data-src", "");
    gem.poisonedSource = gem.body->GetAttribute<Rml::String>("data-src-alt", "");
    return !gem.source.empty();
}

void UI::HUD::LiveHud::ReadConfig()
{
    const UI::RmlBridge::ThemeInheritance theme(std::filesystem::path(ThemesRoot) /
                                                UI::RmlBridge::GetActiveThemeName());
    std::map<std::string, std::string> values;
    for (const auto& [key, setting] : theme.Section(LiveHudSection))
        values.emplace(key, setting.value);
    m_config = LiveGauge::ReadLiveHudConfig(values);
}

void UI::HUD::LiveHud::Advance(Gem& gem, float target, double now, float dt)
{
    const float level = std::clamp(target, 0.f, 1.f);
    if (!gem.started)
    {
        gem.started = true;
        gem.shown = level;
        gem.lastTarget = level;
        gem.ghost.Update(level, level, now);
        return;
    }
    if (level != gem.lastTarget)
        gem.slosh.Kick(level - gem.lastTarget, now);
    gem.lastTarget = level;
    gem.shown = LiveGauge::EaseToward(gem.shown, level, dt);
    if (gem.ghosted)
        gem.ghost.Update(level, gem.shown, now);
}

void UI::HUD::LiveHud::Draw(Gem& gem, bool poisoned, double tick)
{
    DrawUnder(gem, BoxWithin(gem.box, gem.under));
    // The body and the shine share the ring's box (main_frame.rml), so one shape serves both.
    const LiveGauge::Box box = BoxWithin(gem.box, gem.body);
    if (box.width <= 0.f || box.height <= 0.f)
        return;

    const bool waving = LiveGauge::WaveFade(gem.shown) > 0.f && m_config.waveAmplitude > 0.f;
    MeshKey bodyKey;
    bodyKey.box = box;
    bodyKey.level = gem.shown;
    // Without a ghost its line sits at the bottom, so the wave's troughs show the empty art.
    bodyKey.ghost = gem.ghosted && gem.ghost.Active() ? gem.ghost.Level() : 0.f;
    bodyKey.waveTime = waving ? tick : 0.0;
    bodyKey.waveAmplitude = waving ? m_config.waveAmplitude * m_dpRatio * gem.slosh.Gain(tick) : 0.f;
    bodyKey.poisoned = poisoned;
    MeshKey shineKey = bodyKey;
    shineKey.ghost = 0.f;
    shineKey.sheenPhase = SheenPhase(tick, gem.sheenOffset);
    const bool bodyDirty = !(bodyKey == gem.bodyKey);
    const bool shineDirty = !(shineKey == gem.shineKey);
    if (!bodyDirty && !shineDirty)
        return;

    if (gem.shape.xs.empty() || !(gem.shape.box == box))
        LiveGauge::ShapeGem(box, m_config.gemOutline, m_config.gemZoom, gem.shape);
    LiveGauge::ShapeSurface(gem.shape, gem.shown, {bodyKey.waveTime, gem.wavePeriod, bodyKey.waveAmplitude},
                            gem.surface);
    const std::string& source = poisoned && !gem.poisonedSource.empty() ? gem.poisonedSource : gem.source;

    if (bodyDirty)
    {
        const LiveGauge::BodyShades shades{m_config.emptyShade, GhostShade, FillShade};
        LiveGauge::BuildOrbBody(gem.shape, gem.surface, bodyKey.ghost, shades, gem.bodyQuads);
        gem.body->SetMesh(source, gem.bodyQuads, 1.f, false);
        gem.bodyKey = bodyKey;
    }
    if (shineDirty)
    {
        LiveGauge::ShineInputs shine;
        shine.crestHeight = CrestHeightDp * m_dpRatio;
        // The crest goes with the surface: none on a full or empty gem.
        shine.crestTint = CrestTint * LiveGauge::WaveFade(gem.shown);
        shine.highlightPeak = HighlightPeak;
        shine.sheenPhase = shineKey.sheenPhase;
        LiveGauge::BuildOrbShine(gem.shape, gem.surface, shine, gem.shineQuads);
        gem.shine->SetMesh(source, gem.shineQuads, 1.f, true);
        gem.shineKey = shineKey;
    }
}

void UI::HUD::LiveHud::DrawUnder(Gem& gem, const LiveGauge::Box& box)
{
    const UnderKey key{box, m_config.rimWidth * m_dpRatio};
    if (key == gem.underKey || box.width <= 0.f || box.height <= 0.f)
        return;
    LiveGauge::BuildOrbUnder(box, m_config.gemOutline, key.rimWidth, {m_config.rimOuter, m_config.rimInner},
                             gem.underQuads);
    gem.under->SetPlainMesh(gem.underQuads, 1.f, false);
    gem.underKey = key;
}
