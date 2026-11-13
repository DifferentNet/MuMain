#include "Render/Models/WindSway.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace Render::Models::Wind
{
namespace
{
struct FoliageEntry
{
    std::string_view stem;
    Kind kind;
};

// Foliage textures of the map objects (Data/ObjectN BMDs), by file stem in lower case, sorted for
// the binary search. Not foliage on purpose: tree_08 (leaf litter), grass_01 (hay), leaf (Devias
// gift boxes), bu0* (Atlans seaweed, already animated), every .jpg picture.
constexpr std::array kFoliage{
    FoliageEntry{"aidaflower", Kind::Flower},     FoliageEntry{"branch01_line", Kind::Crown},
    FoliageEntry{"cho_grass_a", Kind::Bush},      FoliageEntry{"cho_grass_c", Kind::Bush},
    FoliageEntry{"cho_tree_a", Kind::Crown},      FoliageEntry{"cr1_branch01", Kind::Crown},
    FoliageEntry{"cr1_branch02", Kind::Crown},    FoliageEntry{"cr1_branch03", Kind::Crown},
    FoliageEntry{"cr1_branch04", Kind::Crown},    FoliageEntry{"cr1_branch05", Kind::Crown},
    FoliageEntry{"cr1_branch06", Kind::Crown},    FoliageEntry{"cr1_grass01", Kind::Bush},
    FoliageEntry{"cr1_grass02", Kind::Bush},      FoliageEntry{"cr1_grass03", Kind::Bush},
    FoliageEntry{"cr1_grass04", Kind::Bush},      FoliageEntry{"flower", Kind::Flower},
    FoliageEntry{"flower02", Kind::Flower},       FoliageEntry{"flower03", Kind::Flower},
    FoliageEntry{"flower05", Kind::Flower},       FoliageEntry{"grass011", Kind::Bush},
    FoliageEntry{"grass3", Kind::Bush},           FoliageEntry{"han_fftreeleaf", Kind::Crown},
    FoliageEntry{"iceplant01", Kind::Bush},       FoliageEntry{"iceplant02", Kind::Bush},
    FoliageEntry{"icetree01", Kind::Crown},       FoliageEntry{"icetree02", Kind::Crown},
    FoliageEntry{"icetree03", Kind::Crown},       FoliageEntry{"icetree0601", Kind::Crown},
    FoliageEntry{"plant2a", Kind::Bush},          FoliageEntry{"plant3a", Kind::Bush},
    FoliageEntry{"rgrass01", Kind::Bush},         FoliageEntry{"rgrass02", Kind::Bush},
    FoliageEntry{"sea3flower01", Kind::Flower},   FoliageEntry{"sea3flower02", Kind::Flower},
    FoliageEntry{"so_tree01", Kind::Crown},       FoliageEntry{"so_tree05", Kind::Crown},
    FoliageEntry{"song_k3mtree02", Kind::Crown},  FoliageEntry{"song_k3mtree03", Kind::Crown},
    FoliageEntry{"song_tree04", Kind::Crown},     FoliageEntry{"song_tree05", Kind::Crown},
    FoliageEntry{"sos3fol", Kind::Bush},          FoliageEntry{"stree", Kind::Crown},
    FoliageEntry{"tree_01", Kind::Bush},          FoliageEntry{"tree_02", Kind::Bush},
    FoliageEntry{"tree_04", Kind::Crown},         FoliageEntry{"tree_05", Kind::Crown},
    FoliageEntry{"tree_06", Kind::Crown},         FoliageEntry{"tree_07", Kind::Bush},
    FoliageEntry{"tree_09", Kind::Bush},          FoliageEntry{"tree_a", Kind::Crown},
    FoliageEntry{"treea01", Kind::Crown},         FoliageEntry{"treeleap", Kind::Crown},
};

constexpr bool FoliageSorted()
{
    for (std::size_t i = 1; i < kFoliage.size(); ++i)
    {
        if (!(kFoliage[i - 1].stem < kFoliage[i].stem))
        {
            return false;
        }
    }
    return true;
}
static_assert(FoliageSorted(), "kFoliage must be sorted by stem for the binary search");

// The longest stem in kFoliage is shorter than this; a longer name is no foliage.
constexpr std::size_t kMaxStemLength = 32;

// The look of each kind: height range (world units) over which the sway grows, and amplitude.
struct KindLook
{
    float heightRange;
    float amplitude;
};
constexpr KindLook kCrownLook{350.0f, 6.0f};
constexpr KindLook kBushLook{90.0f, 4.0f};
constexpr KindLook kFlowerLook{50.0f, 3.0f};

// An object's scale counts within these bounds for the amplitude.
constexpr float kMinScaleFactor = 0.5f;
constexpr float kMaxScaleFactor = 1.5f;

// The phase advances this much per second; it wraps at 4 pi, where sin(phase) and sin(2.5 phase)
// both return to their start.
constexpr double kPhasePerSecond = 1.6;
constexpr double kMillisecondsPerSecond = 1000.0;
constexpr double kPhaseWrap = 4.0 * std::numbers::pi;

// Terrain grass: the largest offset at full strength, the slow wave's share and the tile steps.
constexpr float kGrassAmplitude = 12.0f;
constexpr float kSlowShare = 0.65f;
constexpr float kGustShare = 0.35f;
constexpr float kGustRate = 2.5f;

constexpr std::string_view kSwayingExtension = ".tga";
} // namespace

Kind ClassifyTexture(std::string_view textureName)
{
    const std::size_t slash = textureName.find_last_of("\\/");
    const std::string_view file = slash == std::string_view::npos ? textureName : textureName.substr(slash + 1);
    const std::size_t dot = file.find_last_of('.');
    if (dot == std::string_view::npos || dot == 0 || dot > kMaxStemLength)
    {
        return Kind::None;
    }
    const std::string_view extension = file.substr(dot);
    if (extension.size() != kSwayingExtension.size())
    {
        return Kind::None;
    }
    for (std::size_t i = 0; i < extension.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(extension[i])) != kSwayingExtension[i])
        {
            return Kind::None;
        }
    }

    std::array<char, kMaxStemLength> lowered{};
    for (std::size_t i = 0; i < dot; ++i)
    {
        lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(file[i])));
    }
    const std::string_view stem(lowered.data(), dot);
    const auto found = std::lower_bound(kFoliage.begin(), kFoliage.end(), stem,
                                        [](const FoliageEntry& entry, std::string_view key) { return entry.stem < key; });
    return found != kFoliage.end() && found->stem == stem ? found->kind : Kind::None;
}

Sway SwayFor(Kind kind, float objectScale, float strength)
{
    if (kind == Kind::None || strength <= 0.0f || objectScale <= 0.0f)
    {
        return {};
    }
    const KindLook look = kind == Kind::Crown ? kCrownLook : kind == Kind::Bush ? kBushLook : kFlowerLook;
    const float scaleFactor = std::clamp(objectScale, kMinScaleFactor, kMaxScaleFactor);
    return {look.amplitude * scaleFactor * strength, 1.0f / (look.heightRange * objectScale)};
}

float Phase(double worldTimeMs)
{
    const double phase = std::fmod(worldTimeMs / kMillisecondsPerSecond * kPhasePerSecond, kPhaseWrap);
    return static_cast<float>(phase < 0.0 ? phase + kPhaseWrap : phase);
}

float GrassOffset(float phase, int x, int y, float strength)
{
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    const float slow = std::sin(phase - (fx * 0.6f + fy * 0.8f));
    const float gust = std::sin(kGustRate * phase - fx * 1.9f - fy * 2.5f);
    return kGrassAmplitude * strength * (slow * kSlowShare + gust * kGustShare);
}

} // namespace Render::Models::Wind
