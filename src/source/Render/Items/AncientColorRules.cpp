#include "Render/Items/AncientColorRules.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <utility>

namespace Render::Items::AncientColors
{
namespace
{
constexpr Tint C = Tint::Colorize;
constexpr Tint H = Tint::Hue;

// Palette "A" (ghinzuro 2026-11-07): each ancient set apart from its normal set and from the other ancient set of
// the same model. Numbers are the sets of itemsettype.bmd; the comment names the model.
constexpr std::array<SetColor, 46> SetColors{{
    {3, 0.45f, 0.65f, 1.00f, H},   // Hyperion (Bronze), sky blue
    {4, 0.85f, 0.88f, 0.95f, H},   // Mist (Bronze), misty silver
    {11, 0.40f, 0.55f, 1.00f, H},  // Hyon (Dragon), steel blue
    {12, 0.45f, 1.00f, 0.35f, H},  // Vicious (Dragon), venom green
    {13, 1.00f, 0.82f, 0.35f, C},  // Apollo (Pad), sun gold
    {14, 0.75f, 0.50f, 1.00f, C},  // Barnake (Pad), amethyst
    {19, 1.00f, 0.80f, 0.45f, H},  // Anubis (Legendary), sand gold
    {20, 1.00f, 0.35f, 0.35f, H},  // Enis (Legendary), ruby
    {15, 0.35f, 0.95f, 0.95f, C},  // Evis (Bone), turquoise
    {16, 0.80f, 0.55f, 1.00f, C},  // Sylion (Bone), lilac
    {2, 0.55f, 0.58f, 0.68f, C},   // Anonymous (Leather), blued steel
    {1, 1.00f, 0.60f, 0.35f, C},   // Warrior (Leather), copper
    {6, 1.00f, 0.30f, 0.25f, C},   // Berserker (Scale), blood red
    {5, 1.00f, 0.85f, 0.40f, C},   // Eplete (Scale), gold
    {17, 0.35f, 0.60f, 1.00f, H},  // Heras (Sphinx), azure
    {18, 0.30f, 0.95f, 0.55f, H},  // Minet (Sphinx), emerald
    {8, 0.85f, 0.90f, 1.00f, H},   // Cloud (Brass), cloud silver
    {7, 1.00f, 0.45f, 0.20f, H},   // Garuda (Brass), fire
    {9, 0.35f, 1.00f, 0.60f, C},   // Kantata (Plate), emerald
    {10, 1.00f, 0.65f, 0.35f, C},  // Rave (Plate), bronze
    {21, 0.30f, 0.85f, 0.90f, C},  // Ceto (Vine), sea wave
    {22, 1.00f, 0.35f, 0.30f, C},  // Drake (Vine), scarlet
    {24, 0.50f, 0.75f, 1.00f, H},  // Fase (Silk), sky
    {23, 0.45f, 0.95f, 0.40f, H},  // Gaia (Silk), forest green
    {26, 0.88f, 0.90f, 0.95f, H},  // Elvian (Wind), silver
    {25, 1.00f, 0.82f, 0.35f, H},  // Odin (Wind), gold
    {27, 0.40f, 0.60f, 1.00f, H},  // Argo (Spirit), azure
    {28, 1.00f, 0.35f, 0.40f, H},  // Karis (Spirit), ruby
    {30, 0.35f, 1.00f, 0.60f, H},  // Aruan (Guardian), emerald
    {29, 0.75f, 0.82f, 1.00f, H},  // Gywen (Guardian), moon silver
    {31, 1.00f, 0.35f, 0.30f, C},  // Gaion (Storm Crow), scarlet
    {32, 0.55f, 0.85f, 1.00f, C},  // Muren (Storm Crow), ice
    {50, 1.00f, 0.35f, 0.30f, C},  // Mahes (Black Dragon), crimson
    {51, 1.00f, 0.50f, 0.20f, H},  // Bragi (Dark Phoenix), fire
    {55, 0.35f, 1.00f, 0.60f, H},  // Alvis (Grand Soul), emerald
    {53, 0.70f, 0.80f, 1.00f, H},  // Frigg (Divine), moon silver
    {57, 0.45f, 0.80f, 1.00f, H},  // Tyr (Thunder Hawk), storm blue
    {33, 1.00f, 0.50f, 0.25f, C},  // Agnis (Adamantine), flame
    {34, 0.35f, 1.00f, 0.60f, C},  // Broy (Adamantine), emerald
    {58, 1.00f, 0.82f, 0.35f, C},  // Khone (Dark Steel), gold
    {54, 0.75f, 0.50f, 1.00f, C},  // Bes (Eclipse), amethyst
    {52, 1.00f, 0.40f, 0.35f, C},  // Serket (Iris), scarlet
    {56, 0.40f, 0.60f, 1.00f, H},  // Apis (Valiant), azure
    {59, 0.45f, 0.80f, 1.00f, H},  // Surt (Glorious), ice flame
    {35, 0.50f, 0.75f, 1.00f, H},  // Chrono (Red Wing), sky
    {36, 0.35f, 1.00f, 0.60f, H},  // Semeden (Red Wing), emerald
}};

// Colorize: how much of the grey replaces the texture's own colour, and the brightening before the colour.
constexpr float GreyShare = 0.85f;
constexpr float ColorizeGain = 1.6f;

// Hue: pixels below this saturation stay as they are, fully coloured above it plus the ramp.
constexpr float NeutralSaturation = 0.12f;
constexpr float SaturationRamp = 0.18f;
// The new saturation: at least this much of the old one, scaled by the set colour's own saturation.
constexpr float MinimumSaturation = 0.35f;
constexpr float SaturationBase = 0.6f;
constexpr float SaturationFromColor = 0.6f;
constexpr float HueGain = 1.15f;

// Skin tones (warm, mid saturation, bright, between red and yellow).
constexpr float SkinMinSaturation = 0.15f;
constexpr float SkinMaxSaturation = 0.6f;
constexpr float SkinMinBrightness = 0.45f;
constexpr float SkinMinGreenShare = 0.35f;
constexpr float SkinMaxGreenShare = 0.85f;

constexpr float LumaR = 0.30f;
constexpr float LumaG = 0.59f;
constexpr float LumaB = 0.11f;
constexpr float ByteMax = 255.0f;
constexpr float Epsilon = 1e-6f;
constexpr int HueSectors = 6;
// An empty slot of itemsettype.bmd (0 is empty too).
constexpr int NoSet = 0xFF;

struct Rgb
{
    float r;
    float g;
    float b;
};

float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

bool IsSkinTone(const Rgb& c)
{
    const float high = std::max({c.r, c.g, c.b});
    const float low = std::min({c.r, c.g, c.b});
    const float saturation = (high - low) / (high + Epsilon);
    return c.r > c.g && c.g > c.b && saturation > SkinMinSaturation && saturation < SkinMaxSaturation &&
           high > SkinMinBrightness && (c.g - c.b) > SkinMinGreenShare * (c.r - c.b) &&
           (c.g - c.b) < SkinMaxGreenShare * (c.r - c.b);
}

Rgb HsvToRgb(float hue, float saturation, float value)
{
    const float scaled = hue * HueSectors;
    const int sector = static_cast<int>(std::floor(scaled)) % HueSectors;
    const float f = scaled - std::floor(scaled);
    const float p = value * (1 - saturation);
    const float q = value * (1 - f * saturation);
    const float t = value * (1 - (1 - f) * saturation);
    switch (sector)
    {
    case 0: return {value, t, p};
    case 1: return {q, value, p};
    case 2: return {p, value, t};
    case 3: return {p, q, value};
    case 4: return {t, p, value};
    default: return {value, p, q};
    }
}

// The hue (0..1) and saturation of a colour.
std::pair<float, float> HueOf(const SetColor& color)
{
    const float high = std::max({color.r, color.g, color.b});
    const float low = std::min({color.r, color.g, color.b});
    const float range = high - low;
    if (range < Epsilon)
    {
        return {0.0f, 0.0f};
    }
    float sector = 0.0f;
    if (high == color.r)
    {
        sector = std::fmod((color.g - color.b) / range + HueSectors, static_cast<float>(HueSectors));
    }
    else if (high == color.g)
    {
        sector = (color.b - color.r) / range + 2;
    }
    else
    {
        sector = (color.r - color.g) / range + 4;
    }
    return {sector / HueSectors, range / high};
}

Rgb Colorize(const Rgb& c, const SetColor& color)
{
    const float luma = c.r * LumaR + c.g * LumaG + c.b * LumaB;
    const auto grey = [&](float channel) {
        return Clamp01((channel * (1 - GreyShare) + luma * GreyShare) * ColorizeGain);
    };
    return {grey(c.r) * color.r, grey(c.g) * color.g, grey(c.b) * color.b};
}

Rgb ShiftHue(const Rgb& c, float hue, float hueSaturation)
{
    const float high = std::max({c.r, c.g, c.b});
    const float low = std::min({c.r, c.g, c.b});
    const float saturation = high > 0 ? (high - low) / (high + Epsilon) : 0.0f;
    const float weight = Clamp01((saturation - NeutralSaturation) / SaturationRamp);
    if (weight <= 0)
    {
        return c;
    }
    const float newSaturation =
        Clamp01(std::max(saturation, MinimumSaturation) * (SaturationBase + SaturationFromColor * hueSaturation));
    const Rgb shifted = HsvToRgb(hue, newSaturation, Clamp01(high * HueGain));
    return {c.r + (shifted.r - c.r) * weight, c.g + (shifted.g - c.g) * weight, c.b + (shifted.b - c.b) * weight};
}

bool StartsWith(std::wstring_view text, std::wstring_view start)
{
    return text.size() >= start.size() &&
           std::equal(start.begin(), start.end(), text.begin(),
                      [](wchar_t a, wchar_t b) { return std::towlower(a) == std::towlower(b); });
}

bool Contains(std::wstring_view text, std::wstring_view part)
{
    return std::search(text.begin(), text.end(), part.begin(), part.end(), [](wchar_t a, wchar_t b) {
               return std::towlower(a) == std::towlower(b);
           }) != text.end();
}
} // namespace

const SetColor* FindSetColor(int set)
{
    if (set <= 0)
    {
        return nullptr;
    }
    const auto found = std::find_if(SetColors.begin(), SetColors.end(), [set](const SetColor& c) { return c.set == set; });
    return found == SetColors.end() ? nullptr : &*found;
}

int PickSet(const std::array<std::uint8_t, 2>& setsOfItem, int discriminator)
{
    if (discriminator < 1 || discriminator > static_cast<int>(setsOfItem.size()))
    {
        return 0;
    }
    const auto set = [&](std::size_t slot) {
        const int number = setsOfItem[slot];
        return number == NoSet ? 0 : number;
    };
    const int own = set(static_cast<std::size_t>(discriminator - 1));
    return own != 0 ? own : set(discriminator == 1 ? 1 : 0);
}

bool KeepsTexture(std::wstring_view fileName)
{
    // The folder part doesn't count.
    const auto slash = fileName.find_last_of(L"/\\");
    const std::wstring_view name = slash == std::wstring_view::npos ? fileName : fileName.substr(slash + 1);
    return StartsWith(name, L"skin_") || StartsWith(name, L"hide") || StartsWith(name, L"level") ||
           Contains(name, L"hair") || Contains(name, L"face") || Contains(name, L"head_n");
}

void Recolor(std::uint8_t* pixels, std::size_t count, int components, const SetColor& color)
{
    if (pixels == nullptr || components < 3)
    {
        return;
    }
    const auto [hue, hueSaturation] = HueOf(color);
    for (std::size_t i = 0; i < count; ++i)
    {
        std::uint8_t* px = pixels + i * static_cast<std::size_t>(components);
        const Rgb in{px[0] / ByteMax, px[1] / ByteMax, px[2] / ByteMax};
        if (IsSkinTone(in))
        {
            continue;
        }
        const Rgb out = color.tint == Tint::Colorize ? Colorize(in, color) : ShiftHue(in, hue, hueSaturation);
        px[0] = static_cast<std::uint8_t>(std::lround(Clamp01(out.r) * ByteMax));
        px[1] = static_cast<std::uint8_t>(std::lround(Clamp01(out.g) * ByteMax));
        px[2] = static_cast<std::uint8_t>(std::lround(Clamp01(out.b) * ByteMax));
    }
}
} // namespace Render::Items::AncientColors
