#include "stdafx.h"

#include "Render/Terrain/TorchLights.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Data/GameConfig/GameConfig.h"
#include "Data/GameConfig/GameConfigConstants.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Render/Lighting/DynamicLightList.h"
#include "Render/Lighting/DynamicLightsSetting.h"
#include "Render/Renderer/MuRenderer.h"
#include "Render/Textures/HdTextures.h"
#include "Scenes/SceneCore.h"
#include "World/MapInfra/MapManager.h"

#include <cmath>
#include <optional>

namespace Render::Terrain::TorchLights
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
// The strength last given, for logging only its changes.
std::optional<float> s_appliedStrength;

ShareGrid s_shareGrid;
// The frame's lights are taken out of the ground's vertex colour (BuildFrame selected some), for
// this world.
bool s_frameActive = false;
int s_frameWorld = -1;

// Whether the frame's ground is the game world's own (not the editor, not another scene).
[[nodiscard]] bool WorldFrame()
{
    return SceneFlag == MAIN_SCENE && EditFlag == EDIT_NONE && Hero != nullptr;
}

void EndFrameLights()
{
    s_shareGrid.Clear();
    s_frameActive = false;
    mu::GetRenderer().SetTerrainLightsFrame({});
}
} // namespace

void ApplyFromConfig()
{
    const GameConfig& config = GameConfig::GetInstance();
    s_strength = Resolve(config.GetTorchLights(), config.GetTorchLightsStrength(),
                         Available() && Render::Textures::Hd::Enabled(), LowPresetChosen());
    if (s_appliedStrength == s_strength)
    {
        return;
    }
    s_appliedStrength = s_strength;
    mu::log::Get("render")->info("Torch light on the ground {} (strength {} %)", s_strength > 0.0f ? "on" : "off",
                                 static_cast<int>(std::lround(s_strength * 100.0f)));
}

float Strength()
{
    return s_strength;
}

Block CurrentBlock()
{
    return ResolveBlock(Available() && Render::Textures::Hd::Enabled(), LowPresetChosen(),
                        Render::Lighting::DynamicLightsActive());
}

void Choose(bool enabled, int strengthPercent)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetTorchLights(enabled ? CfgDefaults::CfgMaxTorchLights : CfgDefaults::CfgMinTorchLights);
    config.SetTorchLightsStrength(strengthPercent);
    config.Save();
    ApplyFromConfig();
}

void BuildFrame()
{
    if (s_strength <= 0.0f || !mu::HasTerrainLightPipelines() || !Render::Lighting::DynamicLightsActive() ||
        !WorldFrame())
    {
        EndFrameLights();
        return;
    }
    if (s_frameWorld != gMapManager.WorldActive)
    {
        s_shareGrid.Clear();
        s_frameWorld = gMapManager.WorldActive;
    }

    Selection selection{};
    const std::size_t count =
        Select(Render::Lighting::FrameDynamicLights().Lights(), Hero->Object.Position, selection);
    if (count == 0)
    {
        EndFrameLights();
        return;
    }
    const std::span<const Selected> selected{selection.data(), count};
    s_shareGrid.Rebuild(selected);
    s_frameActive = true;
    mu::GetRenderer().SetTerrainLightsFrame(Pack(selected, Gain(s_strength)));
}

bool FrameActive()
{
    return s_frameActive && s_frameWorld == gMapManager.WorldActive && WorldFrame();
}

const float* Share(int terrainIndex)
{
    return s_shareGrid.At(terrainIndex);
}

} // namespace Render::Terrain::TorchLights
