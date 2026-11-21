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
#include <string>

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

// The skill slots, in LiveHud::Inputs::cooldowns' order.
constexpr const char* SkillSlotIds[] = {"skill_slot_0", "skill_slot_1", "skill_slot_2",
                                        "skill_slot_3", "skill_slot_4", "current_skill_slot"};
static_assert(std::size(SkillSlotIds) == UI::HUD::LiveHud::SkillSlotCount);
// A slot's class while its ready flash runs (main_frame_live.rcss's skill-ready-flash, as long).
constexpr const char* ReadyClass = "ready";
constexpr double ReadyFlashTime = 0.4;
// The EXP spark: a glow this big round the fill's end, this bright at the top of its breathing (a 1.4 s cycle,
// LiveGauge::SparkBreath()); with HudEffects off it stands still at its breathing's middle. An EXP gain makes it
// FlareRadius times as big and FlareBrightness times as bright, both falling back over FlareTime.
constexpr float SparkRadiusDp = 5.f;
constexpr float SparkPeak = 0.6f;
constexpr float SparkBreathPeriod = 1.4f;
constexpr float SparkStill = 0.825f;
constexpr float FlareRadius = 1.8f;
constexpr float FlareBrightness = 1.5f;
constexpr float FlareTime = 0.8f;
// The level-up: a band this wide either side of its line and this bright crosses the EXP bar, and the sheen crosses
// both gems, in LevelUpRun.
constexpr float LevelUpRun = 0.9f;
constexpr float BandHalfWidthDp = 16.f;
constexpr float BandStrength = 0.8f;
// The spark's and the band's gold, #ffd778: the skill slots' ready flash (tokens.ini hud-ready-flash).
constexpr std::uint32_t SparkColour = 0xFF78D7FFu;

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

// The first element under `parent` with `className`, or null.
Rml::Element* FirstByClass(Rml::Element* parent, const Rml::String& className)
{
    Rml::ElementList found;
    parent->GetElementsByClassName(found, className);
    return found.empty() ? nullptr : found.front();
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

    Cue(inputs, tick);
    const float levelUp =
        inputs.effects && m_levelUpStart ? LiveGauge::RunPhase(tick - *m_levelUpStart, LevelUpRun) : -1.f;

    Look life;
    life.poisoned = inputs.poisoned;
    life.effects = inputs.effects;
    // The hero's own life, not the eased one: the pulse starts and stops with it.
    life.pulsing = inputs.effects && LiveGauge::IsLowLife(inputs.life, m_config.lowLife);
    life.pulse = life.pulsing ? LiveGauge::PulseLevel(inputs.life, m_config.lowLife, m_config.pulsePeriod, tick) : 0.f;
    life.levelUp = levelUp;
    Look mana;
    mana.effects = inputs.effects;
    mana.levelUp = levelUp;
    Draw(m_gems[LifeGem], life, tick);
    Draw(m_gems[ManaGem], mana, tick);
    for (std::size_t i = 0; i < SkillSlotCount; ++i)
        DrawSkill(m_slots[i], inputs.cooldowns[i], inputs.effects, tick);
    DrawSpark(inputs, levelUp, tick);
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
    // The elements may be gone already (a reload): a slot's class and number are put right when it is bound again.
    m_slots = {};
    m_spark = nullptr;
    m_sparkKey = {};
    m_cuesStarted = false;
    m_expGains = 0;
    m_levelUp = {};
    m_flareStart.reset();
    m_levelUpStart.reset();
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
    if (m_bound)
        BindSkills(document);
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

void UI::HUD::LiveHud::BindSkills(Rml::ElementDocument* document)
{
    // Each part is optional: a document without them still gets its gems.
    for (std::size_t i = 0; i < SkillSlotCount; ++i)
    {
        SkillSlot& slot = m_slots[i];
        slot.slot = document->GetElementById(SkillSlotIds[i]);
        if (slot.slot == nullptr)
            continue;
        slot.sweep = rmlui_dynamic_cast<Render::RmlUi::AdditiveMeshElement*>(FirstByClass(slot.slot, "skill-sweep"));
        slot.seconds = FirstByClass(slot.slot, "skill-cd-secs");
        // A return from the phone's HUD finds the same document, maybe with a flash left on.
        slot.slot->SetClass(ReadyClass, false);
    }
    m_spark = MeshById(document, "exp_spark");
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
    if (look.levelUp >= 0.f)
        shineKey.sheenPhase = look.levelUp;
    else
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

void UI::HUD::LiveHud::Cue(const Inputs& inputs, double tick)
{
    // The first tick after binding only notes where the gains stand (the level: LevelUpDetector's first call).
    const bool gained = m_cuesStarted && inputs.expGains > m_expGains;
    m_cuesStarted = true;
    m_expGains = inputs.expGains;
    const bool levelledUp = m_levelUp.Update(inputs.hero, inputs.level);
    if (!inputs.effects)
        return;
    if (gained)
        m_flareStart = tick;
    if (levelledUp)
        m_levelUpStart = tick;
}

void UI::HUD::LiveHud::DrawSkill(SkillSlot& slot, const Inputs::Cooldown& cooldown, bool effects, double tick)
{
    if (slot.slot == nullptr)
        return;
    const float fraction = std::clamp(cooldown.fraction, 0.f, 1.f);

    if (cooldown.skill != slot.skill)
    {
        slot.skill = cooldown.skill;
        slot.flash = {};
    }
    if (slot.flash.Update(fraction) && effects)
    {
        slot.slot->SetClass(ReadyClass, true);
        slot.readyUntil = tick + ReadyFlashTime;
    }
    else if (slot.readyUntil && tick >= *slot.readyUntil)
    {
        slot.slot->SetClass(ReadyClass, false);
        slot.readyUntil.reset();
    }

    if (slot.seconds != nullptr)
    {
        const int seconds = fraction > 0.f ? LiveGauge::CooldownSeconds(cooldown.remainingMs) : 0;
        if (seconds != slot.shownSeconds)
        {
            slot.seconds->SetInnerRML(seconds > 0 ? std::to_string(seconds) : Rml::String());
            slot.shownSeconds = seconds;
        }
    }

    if (slot.sweep == nullptr)
        return;
    const LiveGauge::Box box = OwnBox(slot.sweep);
    // Rebuilt while the slot cools down, and cleared once when it is ready.
    if (fraction == slot.sweepFraction && box == slot.sweepBox)
        return;
    LiveGauge::BuildSweep(box, fraction, m_config.sweepShade, slot.quads);
    slot.sweep->SetPlainMesh(slot.quads, 1.f, false);
    slot.sweepFraction = fraction;
    slot.sweepBox = box;
}

void UI::HUD::LiveHud::DrawSpark(const Inputs& inputs, float levelUp, double tick)
{
    if (m_spark == nullptr)
        return;
    const LiveGauge::Box box = OwnBox(m_spark);
    const float flare = inputs.effects && m_flareStart ? LiveGauge::FlareLeft(tick - *m_flareStart, FlareTime) : 0.f;
    const float breath = inputs.effects ? LiveGauge::SparkBreath(tick, SparkBreathPeriod) : SparkStill;
    SparkKey key;
    key.box = box;
    key.fill = std::clamp(inputs.expFill, 0.f, 1.f);
    key.radius = SparkRadiusDp * m_dpRatio * (1.f + (FlareRadius - 1.f) * flare);
    key.strength = SparkPeak * breath * (1.f + (FlareBrightness - 1.f) * flare);
    key.bandPhase = levelUp;
    if (key == m_sparkKey || !HasArea(box))
        return;

    LiveGauge::BuildGlow(box.left + key.fill * box.width, box.top + box.height / 2.f, key.radius, SparkColour,
                         key.strength, m_sparkQuads);
    LiveGauge::AppendBand(box, key.bandPhase, BandHalfWidthDp * m_dpRatio, SparkColour, BandStrength, m_sparkQuads);
    m_spark->SetPlainMesh(m_sparkQuads, 1.f, true);
    m_sparkKey = key;
}
