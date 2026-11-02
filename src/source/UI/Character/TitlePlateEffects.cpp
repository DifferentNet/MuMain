#include "stdafx.h"
#include "UI/Character/TitlePlateEffects.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

#include "App/Platform/Windows/Winmain.h"
#include "Data/GameConfig/GameConfig.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Renderer/MuRenderer.h"
#include "Render/Renderer/Overlay2DRecorder.h"
#include "Render/Sprites/GlobalBitmap.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "UI/Character/TitlePlatesArt.h"

namespace UI::TitlePlates::Effects
{
namespace
{
using GameLogic::Titles::Rarity;
using GameLogic::Titles::RarityCount;
using Render::Renderer::RecordedVertex;

constexpr float Pi = std::numbers::pi_v<float>;

struct Rgb
{
    float r, g, b;
};

// The light running over the plate: how often (seconds), how bright at most and its colour (the plate's own light
// tint). Silver's is faint (ghinzuro, 2026-11-03).
struct Shine
{
    float period;
    float strength;
    Rgb color;
};

constexpr Shine Shines[RarityCount] = {
    {12.f, 0.3f, {235, 240, 250}},  // silver
    {9.f, 0.45f, {225, 205, 255}},  // ice
    {7.f, 0.55f, {225, 255, 230}},  // jade
    {6.f, 0.65f, {215, 235, 255}},  // gold
    {4.5f, 0.8f, {255, 225, 160}},  // crimson
    {5.f, 0.7f, {255, 235, 170}},   // violet
};
// Seconds the light takes to cross the plate.
constexpr float ShineRun = 0.85f;
// The band: half its width in plate heights, and its tilt (how far its top leans right per unit of height).
constexpr float ShineHalfWidth = 0.55f;
constexpr float ShineTilt = 0.362f;
// Across the band: where its colour stops are (in half widths) and how bright each is; the middle one is white.
constexpr float ShineStops[] = {-1.f, -0.16f, 0.f, 0.16f, 1.f};
constexpr float ShineStopLight[] = {0.f, 0.55f, 1.f, 0.55f, 0.f};
constexpr int ShineMiddleStop = 2;

// The sparkles' colours (silver has no gems), and how much white they keep in their middle.
constexpr Rgb SparkColors[RarityCount] = {
    {0, 0, 0}, {210, 180, 255}, {170, 255, 190}, {170, 210, 255}, {255, 215, 140}, {255, 225, 150},
};
constexpr float SparkWhite = 0.4f;
// A sparkle's size in plate heights; it flashes for this part of its period, which grows with each gem.
constexpr float SparkSize = 0.42f;
constexpr float SparkFlash = 0.35f;
constexpr float SparkPeriod = 1.9f;
constexpr float SparkPeriodStep = 0.37f;
// The amethyst sparkles less often: the lowest rarity with gems.
constexpr float IceSparkDelay = 1.6f;
// The side gems' sparkles are smaller, except on crimson's crown.
constexpr float SideSparkScale = 0.85f;
constexpr float SparkSpin = 0.8f;

// The glow around the edge (black: none) and how it breathes: period, lowest and highest strength.
constexpr Rgb GlowColors[RarityCount] = {
    {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {70, 140, 255}, {255, 90, 40}, {255, 190, 70},
};
constexpr float GlowPeriod = 3.f;
constexpr float GlowMiddle = 0.55f;
constexpr float GlowSwing = 0.4f;

// Crimson's text goes to this gold and back.
constexpr unsigned char ShimmerColor[3] = {255, 226, 140};
constexpr float ShimmerSpeed = 2.2f;

// The embers over the crown: how many, how long each lives, and their colour.
constexpr int EmberCount = 11;
constexpr float EmberLife = 1.3f;
constexpr float EmberLifeStep = 0.2f;
constexpr Rgb EmberColor = {255, 150, 50};
constexpr const wchar_t* EmberTitles[] = {L"cp-first", L"castle-lord"};
// The rainbow light's turn, degrees a second.
constexpr const wchar_t* RainbowTitle = L"streamer";
constexpr float RainbowSpeed = 90.f;

constexpr const wchar_t* AtlasFile = L"Data\\Interface\\newui_titles_fx.tga";

GLuint Texture()
{
    static GLuint texture = 0;
    static bool tried = false;
    if (!tried)
    {
        tried = true;
        texture = Bitmaps.LoadImageExclusive(AtlasFile, GL_LINEAR, GL_CLAMP_TO_EDGE);
        if (texture == BITMAP_UNKNOWN)
            texture = 0;
    }
    return texture;
}

float Seconds()
{
    static const auto start = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    // Wrapped so the float keeps its precision; every period here divides an hour closely enough.
    return static_cast<float>(std::fmod(elapsed.count(), 3600.0));
}

// 0 to 1 from the owner's name, so the plates around do not flash together.
float Phase(const CHARACTER& owner)
{
    std::uint32_t hash = 2166136261u;
    for (const wchar_t* c = owner.ID; *c != L'\0'; ++c)
        hash = (hash ^ static_cast<std::uint32_t>(*c)) * 16777619u;
    return static_cast<float>(hash % 1000u) / 1000.f;
}

std::uint32_t Abgr(const Rgb& color, float light)
{
    const auto channel = [light](float value)
    { return static_cast<std::uint32_t>(std::clamp(value * light, 0.f, 255.f) + 0.5f); };
    return 0xFF000000u | (channel(color.b) << 16) | (channel(color.g) << 8) | channel(color.r);
}

Rgb Mix(const Rgb& a, const Rgb& b, float t)
{
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

Rgb Hue(float degrees)
{
    const auto channel = [degrees](float n)
    {
        const float k = std::fmod(n + degrees / 30.f, 12.f);
        return 255.f * (0.75f - 0.25f * std::max(-1.f, std::min({k - 3.f, 9.f - k, 1.f})));
    };
    return {channel(0.f), channel(8.f), channel(4.f)};
}

// The quads of one plate's layer, in the name window's space until Flush().
class Quads
{
public:
    struct Corner
    {
        float x, y, u, v;
        std::uint32_t abgr;
    };

    // Corners top left, bottom left, bottom right, top right; u and v in the effects atlas' pixels.
    void Add(const Corner (&corners)[4])
    {
        for (const Corner& c : corners)
            m_vertices.push_back({ConvertPositionX(c.x), ConvertPositionY(c.y), c.u / Art::FxAtlasWidth,
                                  c.v / Art::FxAtlasHeight, c.abgr});
    }

    void Flush();

private:
    std::vector<RecordedVertex> m_vertices;
};

void Quads::Flush()
{
    const GLuint texture = Texture();
    if (m_vertices.empty() || texture == 0)
        return;

    if (Render::Renderer::IOverlay2DRecorder* recorder = Render::Renderer::ActiveOverlay2DRecorder())
    {
        if (BITMAP_t* bitmap = Bitmaps.GetTexture(texture))
            recorder->RecordAdditiveMesh({bitmap->FileName, m_vertices});
        return;
    }

    std::vector<mu::Vertex2D> vertices;
    vertices.reserve(m_vertices.size());
    for (const RecordedVertex& v : m_vertices)
        vertices.push_back({v.x, static_cast<float>(WindowHeight) - v.y, v.u, v.v, v.abgr});
    // EnableAlphaBlend() sets up the additive state (no depth writes) and records Glow; Additive weighs the colour by
    // the texture's alpha, then Glow is back for the state EnableAlphaBlend() remembers.
    EnableAlphaBlend();
    mu::GetRenderer().SetBlendMode(mu::BlendMode::Additive);
    BindTexture(static_cast<int>(texture));
    mu::GetRenderer().RenderQuad2D(vertices, static_cast<std::uint32_t>(texture));
    mu::GetRenderer().SetBlendMode(mu::BlendMode::Glow);
    EnableAlphaTest();
}

// A sprite of the atlas centred on (x, y), size wide and high, turned by angle (radians).
void AddSprite(Quads& quads, const Art::Rect& sprite, float x, float y, float size, float angle, std::uint32_t abgr)
{
    const float c = std::cos(angle) * size / 2.f;
    const float s = std::sin(angle) * size / 2.f;
    const float right = sprite.x + sprite.width;
    const float bottom = sprite.y + sprite.height;
    quads.Add({{x - c + s, y - s - c, sprite.x, sprite.y, abgr},
               {x - c - s, y - s + c, sprite.x, bottom, abgr},
               {x + c - s, y + s + c, right, bottom, abgr},
               {x + c + s, y + s - c, right, sprite.y, abgr}});
}

bool IsOneOf(const std::wstring& id, std::span<const wchar_t* const> ids)
{
    return std::any_of(ids.begin(), ids.end(), [&id](const wchar_t* candidate) { return id == candidate; });
}

// One slanted strip of the light between two of its stops (x along the plate at its middle height, in the plates'
// atlas pixels), cut to the plate.
void AddShineStrip(Quads& quads, const Placement& at, int rarity, float x0, float x1, std::uint32_t abgr0,
                   std::uint32_t abgr1)
{
    const Art::Plate& plate = Art::Plates[rarity];
    const Art::Rect& silhouette = Art::PlateEffects[rarity].silhouette;
    const float lean = ShineTilt * plate.height / 2.f;
    if (std::max(x0, x1) + lean <= 0.f || std::min(x0, x1) - lean >= plate.width)
        return;

    const auto corner = [&](float x, float y, std::uint32_t abgr)
    {
        x = std::clamp(x, 0.f, plate.width);
        return Quads::Corner{at.left + x * at.scale, at.top + y * at.scale,
                             silhouette.x + x / plate.width * silhouette.width,
                             silhouette.y + y / plate.height * silhouette.height, abgr};
    };
    quads.Add({corner(x0 + lean, 0.f, abgr0), corner(x0 - lean, plate.height, abgr0),
               corner(x1 - lean, plate.height, abgr1), corner(x1 + lean, 0.f, abgr1)});
}

void AddShine(Quads& quads, const GameLogic::Titles::Worn& title, const Placement& at, float now, float phase)
{
    const int rarity = static_cast<int>(title.Color);
    const Shine& shine = Shines[rarity];
    const float local = std::fmod(now + phase * shine.period, shine.period);
    if (local > ShineRun)
        return;

    const Art::Plate& plate = Art::Plates[rarity];
    const float progress = local / ShineRun;
    const float halfWidth = plate.height * ShineHalfWidth;
    const float middle = -2.f * halfWidth + (plate.width + 4.f * halfWidth) * progress;
    const float light = shine.strength * std::pow(std::sin(Pi * progress), 0.6f);
    const Rgb color = title.Id == RainbowTitle ? Hue(std::fmod(now * RainbowSpeed, 360.f)) : shine.color;

    std::uint32_t abgr[std::size(ShineStops)];
    float x[std::size(ShineStops)];
    for (std::size_t i = 0; i < std::size(ShineStops); ++i)
    {
        const Rgb stop = static_cast<int>(i) == ShineMiddleStop ? Rgb{255, 255, 255} : color;
        abgr[i] = Abgr(stop, light * ShineStopLight[i]);
        x[i] = middle + ShineStops[i] * halfWidth;
    }
    for (std::size_t i = 0; i + 1 < std::size(ShineStops); ++i)
        AddShineStrip(quads, at, rarity, x[i], x[i + 1], abgr[i], abgr[i + 1]);
}

void AddSparks(Quads& quads, Rarity rarity, const Placement& at, float now, float phase)
{
    const int index = static_cast<int>(rarity);
    const Art::Gems& gems = Art::PlateGems[index];
    const float height = Art::Plates[index].height * at.scale;
    const Rgb color = Mix(SparkColors[index], {255, 255, 255}, SparkWhite);
    for (int i = 0; i < gems.count; ++i)
    {
        const float period =
            SparkPeriod + static_cast<float>(i) * SparkPeriodStep + (rarity == Rarity::Ice ? IceSparkDelay : 0.f);
        const float u = std::fmod(now + phase * 7.f + static_cast<float>(i) * 0.61f, period) / period;
        if (u >= SparkFlash)
            continue;
        const float light = std::sin(Pi * u / SparkFlash);
        const float side = i > 0 && rarity != Rarity::Crimson ? SideSparkScale : 1.f;
        const float size = height * SparkSize * (0.55f + 0.45f * light) * side;
        AddSprite(quads, Art::Sparkle, at.left + gems.at[i].x * at.scale, at.top + gems.at[i].y * at.scale, size,
                  now * SparkSpin + static_cast<float>(i), Abgr(color, light));
    }
}

// Embers rising from the main gem (the crown's ruby on crimson), or the plate's top middle on silver.
void AddEmbers(Quads& quads, Rarity rarity, const Placement& at, float now, float phase)
{
    const int index = static_cast<int>(rarity);
    const Art::Plate& plate = Art::Plates[index];
    const Art::Gems& gems = Art::PlateGems[index];
    const float height = plate.height * at.scale;
    const float fromX = at.left + (gems.count > 0 ? gems.at[0].x : plate.width / 2.f) * at.scale;
    const float fromY = at.top + (gems.count > 0 ? gems.at[0].y : 0.f) * at.scale;
    for (int i = 0; i < EmberCount; ++i)
    {
        const float life = EmberLife + static_cast<float>(i % 3) * EmberLifeStep;
        const float u = std::fmod(now + static_cast<float>(i) * 0.37f + phase * 5.f, life) / life;
        const float seed = std::sin(static_cast<float>(i) * 12.9898f) * 43758.5453f;
        const float spread = (seed - std::floor(seed)) - 0.5f;
        const float x = fromX + spread * height * 1.2f + std::sin(now * 2.f + static_cast<float>(i)) * height * 0.06f;
        const float y = fromY - u * height * 0.75f;
        const float size = height * (0.05f - u * 0.03f) * 4.4f;
        const float light = (u < 0.15f ? u / 0.15f : 1.f) * (1.f - u);
        AddSprite(quads, Art::Ember, x, y, size, 0.f, Abgr(EmberColor, light));
    }
}
} // namespace

bool Enabled()
{
    return GameConfig::GetInstance().GetTitleEffects();
}

void RenderUnder(const GameLogic::Titles::Worn& title, const CHARACTER& owner, const Placement& at)
{
    const int rarity = static_cast<int>(title.Color);
    const Rgb& color = GlowColors[rarity];
    if (color.r + color.g + color.b <= 0.f)
        return;

    const Art::Plate& plate = Art::Plates[rarity];
    const Art::PlateFx& fx = Art::PlateEffects[rarity];
    const float light = GlowMiddle + GlowSwing * std::sin((Seconds() + Phase(owner) * 3.f) * 2.f * Pi / GlowPeriod);
    const std::uint32_t abgr = Abgr(color, light);
    const float pad = fx.glowPad * at.scale;
    const float left = at.left - pad;
    const float top = at.top - pad;
    const float right = at.left + plate.width * at.scale + pad;
    const float bottom = at.top + plate.height * at.scale + pad;
    const Art::Rect& glow = fx.glow;
    Quads quads;
    quads.Add({{left, top, glow.x, glow.y, abgr},
               {left, bottom, glow.x, glow.y + glow.height, abgr},
               {right, bottom, glow.x + glow.width, glow.y + glow.height, abgr},
               {right, top, glow.x + glow.width, glow.y, abgr}});
    quads.Flush();
}

void RenderOver(const GameLogic::Titles::Worn& title, const CHARACTER& owner, const Placement& at)
{
    const float now = Seconds();
    const float phase = Phase(owner);
    Quads quads;
    AddShine(quads, title, at, now, phase);
    AddSparks(quads, title.Color, at, now, phase);
    if (IsOneOf(title.Id, EmberTitles))
        AddEmbers(quads, title.Color, at, now, phase);
    quads.Flush();
}

std::array<unsigned char, 3> TextColor(const GameLogic::Titles::Worn& title, const CHARACTER& owner,
                                       const unsigned char (&base)[3])
{
    std::array<unsigned char, 3> color = {base[0], base[1], base[2]};
    if (title.Color != Rarity::Crimson)
        return color;

    const float t = 0.5f + 0.5f * std::sin(Seconds() * ShimmerSpeed + Phase(owner) * 6.f);
    for (int i = 0; i < 3; ++i)
        color[i] = static_cast<unsigned char>(base[i] + (ShimmerColor[i] - base[i]) * t + 0.5f);
    return color;
}
} // namespace UI::TitlePlates::Effects
