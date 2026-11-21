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
constexpr float HighlightPeak = 0.12f;
constexpr float GhostShade = 0.60f;
constexpr float FillShade = 1.f;
// The value arc sits this far inside the ring's track on both sides (the track 27.5-31dp, the value 28-30.5dp), so
// the track's bronze edges show round it.
constexpr float ArcInsetDp = 0.5f;
// The bright cap at the arc value's end: this long, its art added at this tint (1.3 times as bright).
constexpr float CapLengthDp = 2.f;
constexpr float CapTint = 0.30f;

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

// `element`'s own border box: the ring's, which every mesh layer fills.
LiveGauge::Box OwnBox(Rml::Element* element)
{
    const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
    return {0.f, 0.f, size.x, size.y};
}

bool HasArea(const LiveGauge::Box& box)
{
    return box.width > 0.f && box.height > 0.f;
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
    Advance(m_gems[LifeGem].motion, inputs.life, now, dt);
    Advance(m_gems[ManaGem].motion, inputs.mana, now, dt);
    Advance(m_gems[LifeGem].arc.motion, inputs.shield, now, dt);
    Advance(m_gems[ManaGem].arc.motion, inputs.skillMana, now, dt);

    const double tick = std::floor(now * TicksPerSecond) / TicksPerSecond;
    if (tick == m_lastTick)
        return;
    m_lastTick = tick;
    if (const Rml::Context* context = document->GetContext())
        m_dpRatio = context->GetDensityIndependentPixelRatio();

    Look life;
    life.poisoned = inputs.poisoned;
    life.effects = inputs.effects;
    // The hero's own life, not the eased one: the pulse starts and stops with it.
    life.pulsing = inputs.effects && LiveGauge::IsLowLife(inputs.life, m_config.lowLife);
    life.pulse = life.pulsing ? LiveGauge::PulseLevel(inputs.life, m_config.lowLife, m_config.pulsePeriod, tick) : 0.f;
    Look mana;
    mana.effects = inputs.effects;
    Draw(m_gems[LifeGem], life, tick);
    Draw(m_gems[ManaGem], mana, tick);
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
        gem.halo = nullptr;
        gem.body = nullptr;
        gem.shine = nullptr;
        gem.motion = {};
        gem.arc.mesh = nullptr;
        gem.arc.cap = nullptr;
        gem.arc.motion = {};
        gem.arc.key = {};
        gem.shape.xs.clear();
        gem.underKey = {};
        gem.haloKey = {};
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
    life.motion.ghosted = true;
    life.arc.motion.ghosted = true;
    life.haloColour = m_config.haloLife;
    life.poisonedHaloColour = m_config.haloPoison;
    Gem& mana = m_gems[ManaGem];
    mana.wavePeriod = m_config.wavePeriodMana;
    mana.sheenOffset = ManaSheenOffset;
    mana.motion.ghosted = false;
    mana.arc.motion.ghosted = false;
    mana.haloColour = m_config.haloMana;
    mana.poisonedHaloColour = m_config.haloMana;
    m_bound = BindGem(document, "hp", "sd", life) && BindGem(document, "mp", "ag", mana);
}

bool UI::HUD::LiveHud::BindGem(Rml::ElementDocument* document, const char* name, const char* arcName, Gem& gem)
{
    const Rml::String prefix = Rml::String("orb_") + name;
    gem.box = document->GetElementById(prefix + "_box");
    gem.under = MeshById(document, prefix + "_under");
    gem.halo = MeshById(document, prefix + "_halo");
    gem.body = MeshById(document, prefix + "_body");
    gem.shine = MeshById(document, prefix + "_shine");
    if (gem.box == nullptr || gem.under == nullptr || gem.body == nullptr || gem.shine == nullptr)
        return false;
    gem.source = gem.body->GetAttribute<Rml::String>("data-src", "");
    gem.poisonedSource = gem.body->GetAttribute<Rml::String>("data-src-alt", "");

    // The halo and the ring are the gem's extras: a document without them still gets its gem.
    const Rml::String arcPrefix = Rml::String("orb_") + arcName;
    gem.arc.mesh = MeshById(document, arcPrefix + "_arc");
    gem.arc.cap = MeshById(document, arcPrefix + "_cap");
    gem.arc.source = gem.arc.mesh != nullptr ? gem.arc.mesh->GetAttribute<Rml::String>("data-src", "") : "";
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

void UI::HUD::LiveHud::Advance(Motion& motion, float target, double now, float dt)
{
    const float level = std::clamp(target, 0.f, 1.f);
    if (!motion.started)
    {
        motion.started = true;
        motion.shown = level;
        motion.lastTarget = level;
        motion.ghost.Update(level, level, now);
        return;
    }
    if (level != motion.lastTarget)
        motion.slosh.Kick(level - motion.lastTarget, now);
    motion.lastTarget = level;
    motion.shown = LiveGauge::EaseToward(motion.shown, level, dt);
    if (motion.ghosted)
        motion.ghost.Update(level, motion.shown, now);
}

LiveGauge::Ring UI::HUD::LiveHud::TrackRing(const LiveGauge::Box& gem, const LiveGauge::Box& ring) const
{
    LiveGauge::Ring track;
    track.centreX = gem.left + gem.width / 2.f;
    track.centreY = gem.top + gem.height / 2.f;
    track.outer = std::min(ring.width, ring.height) / 2.f;
    track.inner = std::max(0.f, track.outer - m_config.arcWidth * m_dpRatio);
    track.startDegrees = m_config.arcStart;
    track.sweepDegrees = m_config.arcSweep;
    return track;
}

void UI::HUD::LiveHud::Draw(Gem& gem, const Look& look, double tick)
{
    DrawUnder(gem);
    DrawHalo(gem, look);
    DrawArc(gem);
    // The body and the shine share the ring's box (main_frame.rml), so one shape serves both.
    const LiveGauge::Box box = BoxWithin(gem.box, gem.body);
    if (!HasArea(box))
        return;

    const Motion& motion = gem.motion;
    const bool waving = look.effects && LiveGauge::WaveFade(motion.shown) > 0.f && m_config.waveAmplitude > 0.f;
    MeshKey bodyKey;
    bodyKey.box = box;
    bodyKey.level = motion.shown;
    // Without a ghost its line sits at the bottom, so the wave's troughs show the empty art.
    bodyKey.ghost = motion.GhostLevel();
    bodyKey.waveTime = waving ? tick : 0.0;
    bodyKey.waveAmplitude = waving ? m_config.waveAmplitude * m_dpRatio * motion.slosh.Gain(tick) : 0.f;
    bodyKey.poisoned = look.poisoned;
    MeshKey shineKey = bodyKey;
    shineKey.ghost = 0.f;
    shineKey.sheenPhase = look.effects ? SheenPhase(tick, gem.sheenOffset) : -1.f;
    shineKey.brighten = look.pulsing ? m_config.pulseStrength * look.pulse : 0.f;
    const bool bodyDirty = !(bodyKey == gem.bodyKey);
    const bool shineDirty = !(shineKey == gem.shineKey);
    if (!bodyDirty && !shineDirty)
        return;

    if (gem.shape.xs.empty() || !(gem.shape.box == box))
        LiveGauge::ShapeGem(box, m_config.gemOutline, m_config.gemZoom, gem.shape);
    LiveGauge::ShapeSurface(gem.shape, motion.shown, {bodyKey.waveTime, gem.wavePeriod, bodyKey.waveAmplitude},
                            gem.surface);
    const std::string& source = look.poisoned && !gem.poisonedSource.empty() ? gem.poisonedSource : gem.source;

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
        shine.crestTint = m_config.crestTint * LiveGauge::WaveFade(motion.shown);
        shine.highlightPeak = HighlightPeak;
        shine.sheenPhase = shineKey.sheenPhase;
        shine.brighten = shineKey.brighten;
        LiveGauge::BuildOrbShine(gem.shape, gem.surface, shine, gem.shineQuads);
        gem.shine->SetMesh(source, gem.shineQuads, 1.f, true);
        gem.shineKey = shineKey;
    }
}

void UI::HUD::LiveHud::DrawUnder(Gem& gem)
{
    const LiveGauge::Box box = BoxWithin(gem.box, gem.under);
    const LiveGauge::Box ring = OwnBox(gem.under);
    // The track goes with the ring's arc: none without one.
    const float trackWidth = gem.arc.mesh != nullptr ? m_config.arcWidth * m_dpRatio : 0.f;
    const UnderKey key{box, ring, m_config.rimWidth * m_dpRatio, trackWidth};
    if (key == gem.underKey || !HasArea(box))
        return;
    const LiveGauge::RimColours colours{m_config.rimOuter, m_config.rimInner};
    LiveGauge::BuildOrbUnder(box, m_config.gemOutline, key.rimWidth, colours, gem.underQuads);
    if (trackWidth > 0.f)
        LiveGauge::AppendArcTrack(TrackRing(box, ring), colours, gem.underQuads);
    gem.under->SetPlainMesh(gem.underQuads, 1.f, false);
    gem.underKey = key;
}

void UI::HUD::LiveHud::DrawHalo(Gem& gem, const Look& look)
{
    if (gem.halo == nullptr)
        return;
    const LiveGauge::Box box = BoxWithin(gem.box, gem.halo);
    const LiveGauge::Box ring = OwnBox(gem.halo);
    const HaloKey key{box, ring, LiveGauge::HaloStrength(gem.motion.shown, look.pulsing, look.pulse),
                      look.poisoned ? gem.poisonedHaloColour : gem.haloColour};
    if (key == gem.haloKey || !HasArea(box))
        return;
    // Round the gem as far as the ring reaches, and never under the group's box (the gem's bottom is its bottom).
    const LiveGauge::HaloShape shape{box.left + box.width / 2.f, box.top + box.height / 2.f,
                                     std::min(ring.width, ring.height) / 2.f, box.top + box.height};
    LiveGauge::BuildHalo(box, m_config.gemOutline, shape, key.colour, key.strength, gem.haloQuads);
    gem.halo->SetPlainMesh(gem.haloQuads, 1.f, true);
    gem.haloKey = key;
}

void UI::HUD::LiveHud::DrawArc(Gem& gem)
{
    Arc& arc = gem.arc;
    if (arc.mesh == nullptr || arc.source.empty())
        return;
    const LiveGauge::Box box = BoxWithin(gem.box, arc.mesh);
    const ArcKey key{box, OwnBox(arc.mesh), arc.motion.shown, arc.motion.GhostLevel(), m_dpRatio};
    if (key == arc.key || !HasArea(box))
        return;

    LiveGauge::Ring value = TrackRing(box, key.ring);
    const float inset = ArcInsetDp * m_dpRatio;
    value.inner += inset;
    value.outer -= inset;
    LiveGauge::BuildArc(value, {key.level, key.ghost, GhostShade}, arc.quads);
    arc.mesh->SetMesh(arc.source, arc.quads, 1.f, false);
    if (arc.cap != nullptr)
    {
        LiveGauge::BuildArcCap(value, key.level, CapLengthDp * m_dpRatio, CapTint, arc.capQuads);
        arc.cap->SetMesh(arc.source, arc.capQuads, 1.f, true);
    }
    arc.key = key;
}
