#include "stdafx.h"

#include "Render/PostProcessing/PostProcessingSetting.h"

#include "Core/Utilities/Log/MuLogger.h"
#include "Render/Look/LookProfileSetting.h"
#include "Render/PostProcessing/ScreenFilterSetting.h"
#include "Render/Renderer/MuRenderer.h"

namespace Render::PostProcessing
{
namespace
{
// What the look asked for and what the renderer accepted of it. The look's bloom
// and colour grading are drawn only while it is on; the pass itself also runs
// for the screen filters alone.
bool s_lookWantsPostProcessing = false;
bool s_postProcessingActive = false;

void ApplyToRenderer()
{
    const bool filters = ScreenFiltersOn();
    const bool enabled = s_lookWantsPostProcessing || filters;
    const bool accepted = mu::GetRenderer().SetPostProcessing(enabled);
    s_postProcessingActive = accepted && s_lookWantsPostProcessing;
    if (!accepted)
    {
        mu::log::Get("render")->warn("Post-processing requested but unavailable; frames stay unprocessed");
        return;
    }
    mu::log::Get("render")->info("Post-processing {}{}", enabled ? "on" : "off",
                                 enabled && !s_lookWantsPostProcessing ? " (screen filters only)" : "");
}
} // namespace

bool SetPostProcessingEnabled(bool enabled)
{
    s_lookWantsPostProcessing = enabled;
    ApplyToRenderer();
    return !enabled || s_postProcessingActive;
}

bool PostProcessingActive()
{
    return s_postProcessingActive;
}

void RefreshPostProcessing()
{
    ApplyToRenderer();
}

bool SetPostProcessingView(std::wstring_view name)
{
    struct NamedView
    {
        std::wstring_view name;
        const char* logName;
        mu::PostProcessingView view;
    };
    static constexpr NamedView views[] = {
        {L"scene", "scene", mu::PostProcessingView::Scene},
        {L"source", "bloom source", mu::PostProcessingView::BloomSource},
        {L"bloom", "bloom", mu::PostProcessingView::Bloom},
    };
    for (const NamedView& entry : views)
    {
        if (entry.name == name)
        {
            mu::GetRenderer().SetPostProcessingView(entry.view);
            mu::log::Get("render")->info("Post-processing view: {}", entry.logName);
            return true;
        }
    }
    mu::log::Get("render")->warn("$post view expects scene, source or bloom");
    return false;
}

void SubmitPostProcessingLook()
{
    using Data::Maps::LookValue;
    mu::PostProcessingLook look;
    if (s_postProcessingActive)
    {
        look.bloomStrength = Render::Look::Value(LookValue::BloomStrength);
        look.bloomLevelFalloff = Render::Look::Value(LookValue::BloomLevelFalloff);
        look.gradingStrength = Render::Look::Value(LookValue::GradingStrength);
    }
    else
    {
        look.gradingStrength = 0.0f; // filters only: the scene keeps its colours
    }
    const ScreenFilters filters = GetScreenFilters();
    look.fxaa = filters.fxaa;
    look.sharpen = filters.sharpen;
    mu::GetRenderer().SetPostProcessingLook(look);
}

} // namespace Render::PostProcessing
