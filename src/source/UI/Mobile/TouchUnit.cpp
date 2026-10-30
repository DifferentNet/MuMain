#include "stdafx.h"
#include "UI/Mobile/TouchUnit.h"

#include "Data/GameConfig/GameConfig.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/Mobile/ButtonSize.h"
#include "UI/Mobile/InterfaceMode.h"

#ifdef __EMSCRIPTEN__
#include "Core/Platform/web/WebScreen.h"
#endif

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Property.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace
{
// The canvas's size on the page also changes with the browser's zoom, which leaves the window's size
// alone: measured again this often.
constexpr auto RemeasureEvery = std::chrono::seconds(1);

// The phone view's smallest text, in finger units at 100 %: the smallest a phone shows readably
// (Material's caption, 12 sp).
constexpr float ReadableTextUnits = 12.f;
constexpr int FullSize = 100;

struct Measured
{
    unsigned int windowWidth = 0;
    unsigned int windowHeight = 0;
    int percent = 0;
    std::chrono::steady_clock::time_point at{};
    float pixels = 0.f;
    // The unit at 100 %, whatever the player's percent.
    float fullPixels = 0.f;
};
Measured g_measured;

float DpRatio()
{
    const Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    const float ratio = context != nullptr ? context->GetDensityIndependentPixelRatio() : 1.f;
    return ratio > 0.f ? ratio : 1.f;
}

// The unit, and whether it is settled: in the browser, before the page lays the canvas out, it is dp's
// for now and measured again next time.
float Measure(int percent, bool& settled)
{
    settled = true;
#ifdef __EMSCRIPTEN__
    mu::web::CanvasOnPage canvas;
    if (mu::web::MeasureCanvas(canvas))
    {
        return UI::Mobile::ButtonSize::UnitCssPixels(canvas.cssWidth, canvas.cssHeight, percent) * canvas.pixelsPerCss;
    }
    settled = false;
#endif
    return DpRatio() * static_cast<float>(percent) / 100.f;
}

const Measured& Current()
{
    const int percent = UI::Mobile::TouchUnit::GetPercent();
    const auto now = std::chrono::steady_clock::now();
    if (g_measured.pixels <= 0.f || g_measured.windowWidth != WindowWidth || g_measured.windowHeight != WindowHeight ||
        g_measured.percent != percent || now - g_measured.at >= RemeasureEvery)
    {
        bool settled = true;
        const float pixels = Measure(percent, settled);
        const float fullPixels = Measure(FullSize, settled);
        const auto at = settled ? now : std::chrono::steady_clock::time_point{};
        g_measured = Measured{WindowWidth, WindowHeight, percent, at, pixels, fullPixels};
    }
    return g_measured;
}
} // namespace

float UI::Mobile::TouchUnit::Pixels()
{
    return Current().pixels;
}

float UI::Mobile::TouchUnit::ReadableTextPx(float px)
{
    if (!UI::Mobile::IsPhoneView())
        return px;
    return std::max(px, ReadableTextUnits * Current().fullPixels);
}

void UI::Mobile::TouchUnit::Apply(Rml::ElementDocument* document)
{
    if (document == nullptr)
        return;
    // Whole hundredths of a pixel: a size that wobbles in the last digit would lay the document out
    // again every frame.
    const float pixels = std::round(Pixels() * 100.f) / 100.f;
    const Rml::Property* current = document->GetLocalProperty(Rml::PropertyId::FontSize);
    if (current == nullptr || current->unit != Rml::Unit::PX || current->Get<float>() != pixels)
        document->SetProperty(Rml::PropertyId::FontSize, Rml::Property(pixels, Rml::Unit::PX));
}

int UI::Mobile::TouchUnit::GetPercent()
{
    return GameConfig::GetInstance().GetTouchButtonSize();
}

void UI::Mobile::TouchUnit::SetPercent(int percent)
{
    GameConfig& config = GameConfig::GetInstance();
    config.SetTouchButtonSize(percent);
    config.Save();
}
