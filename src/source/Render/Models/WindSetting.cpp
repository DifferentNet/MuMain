#include "stdafx.h"

#include "Render/Models/WindSway.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Data/GameConfig/GameConfigConstants.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Textures/HdTextures.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "World/MapInfra/MapManager.h"

#include <array>
#include <cstring>
#include <optional>

namespace Render::Models::Wind
{

bool Available()
{
#ifdef __EMSCRIPTEN__
    return false;
#else
    return true;
#endif
}

namespace
{
[[nodiscard]] bool LowPresetChosen()
{
    return GameConfig::GetInstance().GetGraphicsPreset() == GraphicsPreset::Low;
}

float s_strength = 0.0f;
// The strength last logged, for logging only its changes.
std::optional<float> s_appliedStrength;

// Meshes past this index never sway (map object models have far fewer).
constexpr int kMaxMeshes = 32;

// Per world-object model of the current map: the kind of each mesh, once the model is loaded.
// World-object models are reloaded per map under the same indices, so the answers are per map.
struct ModelKinds
{
    bool known = false;
    std::array<Kind, kMaxMeshes> meshes{};
};

const ModelKinds* KindsFor(int type)
{
    static std::array<ModelKinds, MAX_WORLD_OBJECTS> s_models{};
    static int s_world = -1;
    if (s_world != gMapManager.WorldActive)
    {
        s_models.fill({});
        s_world = gMapManager.WorldActive;
    }
    if (type < 0 || type >= MAX_WORLD_OBJECTS)
    {
        return nullptr;
    }
    ModelKinds& kinds = s_models[static_cast<std::size_t>(type)];
    if (kinds.known)
    {
        return &kinds;
    }
    const BMD& model = ::Models[type];
    if (model.NumMeshs <= 0 || !model.Meshs || !model.Textures)
    {
        return nullptr;
    }
    const int meshes = std::min<int>(model.NumMeshs, kMaxMeshes);
    for (int i = 0; i < meshes; ++i)
    {
        const short texture = model.Meshs[i].Texture;
        if (texture < 0 || texture >= model.NumMeshs)
        {
            continue;
        }
        const char* fileName = model.Textures[texture].FileName;
        kinds.meshes[static_cast<std::size_t>(i)] =
            ClassifyTexture(std::string_view(fileName, strnlen(fileName, sizeof(model.Textures[texture].FileName))));
    }
    kinds.known = true;
    return &kinds;
}

// The world object being drawn (ScopedObject), or none.
struct ActiveObject
{
    const BMD* model = nullptr;
    const ModelKinds* kinds = nullptr;
    float scale = 1.0f;
    float phase = 0.0f;
};
ActiveObject s_active;
} // namespace

void ApplyFromConfig()
{
    const GameConfig& config = GameConfig::GetInstance();
    s_strength = Resolve(config.GetWind(), config.GetWindStrength(), Available() && Render::Textures::Hd::Enabled(),
                         LowPresetChosen());
    if (s_appliedStrength == s_strength)
    {
        return;
    }
    s_appliedStrength = s_strength;
    mu::log::Get("render")->info("Wind {} (strength {} %)", s_strength > 0.0f ? "on" : "off",
                                 static_cast<int>(s_strength * kPercentPerUnit));
}

float Strength()
{
    return s_strength;
}

Block CurrentBlock()
{
    return ResolveBlock(Available() && Render::Textures::Hd::Enabled(), LowPresetChosen());
}

void Choose(bool enabled, int strengthPercent)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetWind(enabled ? CfgDefaults::CfgMaxWind : CfgDefaults::CfgMinWind);
    config.SetWindStrength(strengthPercent);
    config.Save();
    ApplyFromConfig();
}

ScopedObject::ScopedObject(const OBJECT* object)
{
    if (s_strength <= 0.0f || !object)
    {
        return;
    }
    const ModelKinds* kinds = KindsFor(object->Type);
    if (!kinds)
    {
        return;
    }
    s_active = {&::Models[object->Type], kinds, object->Scale, Phase(WorldTime)};
    m_active = true;
}

ScopedObject::~ScopedObject()
{
    if (m_active)
    {
        s_active = {};
    }
}

bool MeshSway(const BMD* model, int meshIndex, float out[3])
{
    if (!s_active.model || model != s_active.model || meshIndex < 0 || meshIndex >= kMaxMeshes)
    {
        return false;
    }
    const Sway sway = SwayFor(s_active.kinds->meshes[static_cast<std::size_t>(meshIndex)], s_active.scale, s_strength);
    if (sway.amplitude <= 0.0f)
    {
        return false;
    }
    out[0] = sway.amplitude;
    out[1] = sway.inverseHeightRange;
    out[2] = s_active.phase;
    return true;
}

} // namespace Render::Models::Wind
