#include "stdafx.h"

#include "Scenes/SceneLightingSetup.h"

#include "Camera/CameraState.h"
#include "Core/Utilities/Log/MuLogger.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Render/Lighting/DynamicLightList.h"
#include "Render/Lighting/DynamicLightSelection.h"
#include "Render/Lighting/DynamicLightsSetting.h"
#include "Render/Lighting/EnhancedLightingSetting.h"
#include "Render/Lighting/LightMoodSetting.h"
#include "Render/Look/LookProfileSetting.h"
#include "Render/Renderer/MuRenderer.h"
#include "Render/Renderer/SceneLighting.h"
#include "Scenes/SceneCore.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>

namespace Scenes::Lighting
{
namespace
{
Render::Lighting::DroppedLightReportLimiter s_droppedLightReport;

void ReportDroppedLights(const Render::Lighting::DynamicLightSelection& selection, std::size_t overflow)
{
    if (!s_droppedLightReport.ShouldReport(std::chrono::steady_clock::now()))
    {
        return;
    }
    mu::log::Get("render")->info(
        "Enhanced dynamic lights: {} of {} lights dropped (cap {}), {} beyond the frame's light list",
        selection.dropped, selection.brighteningLights, mu::kMaxDynamicLights, overflow);
}

// Where the capped lights matter most: the player's character in the game, the camera elsewhere.
void SelectionReference(float (&reference)[3])
{
    const float* position = SceneFlag == MAIN_SCENE && Hero ? Hero->Object.Position : g_Camera.Position;
    reference[0] = position[0];
    reference[1] = position[1];
    reference[2] = position[2];
}
} // namespace

void SubmitSceneLighting()
{
    mu::SceneLighting lighting{};
    lighting.cameraPosition[0] = g_Camera.Position[0];
    lighting.cameraPosition[1] = g_Camera.Position[1];
    lighting.cameraPosition[2] = g_Camera.Position[2];
    // Towards the map's sun (its mood file, as the sun shadow takes it): the water highlight's
    // light and whether the sun is up (terrain_water.frag).
    const Data::Maps::LightMood& mood = Render::Lighting::LightMood::WorldMood();
    constexpr double degreesToRadians = 3.14159265358979323846 / 180.0;
    const double azimuth = mood.sunAzimuth * degreesToRadians;
    const double elevation = std::clamp(mood.sunElevation, 20.0, 85.0) * degreesToRadians;
    lighting.lightDirection[0] = static_cast<float>(std::cos(elevation) * std::cos(azimuth));
    lighting.lightDirection[1] = static_cast<float>(std::cos(elevation) * std::sin(azimuth));
    lighting.lightDirection[2] = static_cast<float>(std::sin(elevation));
    Render::Lighting::ApplyTuning(lighting);
    lighting.highlightKnee = Render::Lighting::LightMood::Active().highlightKnee;
    mu::GetRenderer().SetSceneLighting(lighting);
}

void SubmitDynamicLights()
{
    if (!Render::Lighting::DynamicLightsActive())
    {
        mu::GetRenderer().SetDynamicLights({});
        return;
    }

    const Render::Lighting::DynamicLightList& lights = Render::Lighting::FrameDynamicLights();
    float reference[3];
    SelectionReference(reference);
    Render::Lighting::DynamicLightSelection selection =
        Render::Lighting::SelectDynamicLights(lights.Lights(), reference);
    selection.uniform.wrap = Render::Look::Value(Data::Maps::LookValue::DynamicWrap);
    mu::GetRenderer().SetDynamicLights(selection.uniform);

    if (selection.dropped > 0 || lights.Overflow() > 0)
    {
        ReportDroppedLights(selection, lights.Overflow());
    }
}

} // namespace Scenes::Lighting
