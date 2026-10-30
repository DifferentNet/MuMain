#include "stdafx.h"
#include "UI/Mobile/NoticesUnderPanel.h"

#include "Render/RmlUi/RmlUiRuntime.h"
#include "UI/RmlBridge/RmlStackingOrder.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <chrono>
#include <string_view>

namespace
{
using Clock = std::chrono::steady_clock;

constexpr std::string_view NoticesFile = "notices.rml";
// Over the command windows (ForegroundPanelLayerDepth), under the open menu and its backdrop
// (OpenTouchMenuDepth); no document's depth lies between them.
constexpr float UnderOpenMenuDepth =
    (UI::RmlBridge::ForegroundPanelLayerDepth + UI::RmlBridge::OpenTouchMenuDepth) / 2.f;
// The notices not loaded yet are looked for again this often, not every frame of an open menu.
constexpr auto SearchEvery = std::chrono::milliseconds(250);

// The notices' document while it lives (a theme switch builds it anew, at its own depth).
Rml::ObserverPtr<Rml::Element> g_notices;
Clock::time_point g_searchedAt{};
// The notices were put under the open panel and not given back their depth yet.
bool g_lowered = false;

bool IsFile(std::string_view url, std::string_view file)
{
    return url.size() > file.size() && url.ends_with(file) && url[url.size() - file.size() - 1] == '/';
}

Rml::Element* Search()
{
    Rml::Context* context = RmlUiRuntime::Instance().GetContext();
    if (context == nullptr)
        return nullptr;
    for (int i = 0; i < context->GetNumDocuments(); ++i)
    {
        Rml::ElementDocument* document = context->GetDocument(i);
        if (IsFile(document->GetSourceURL(), NoticesFile))
            return document;
    }
    return nullptr;
}

Rml::Element* Find()
{
    if (Rml::Element* notices = g_notices.get())
        return notices;
    const Clock::time_point now = Clock::now();
    if (now - g_searchedAt < SearchEvery)
        return nullptr;
    g_searchedAt = now;
    Rml::Element* notices = Search();
    if (notices != nullptr)
        g_notices = notices->GetObserverPtr();
    return notices;
}

void SetDepth(Rml::Element& notices, float depth)
{
    const Rml::Property* current = notices.GetLocalProperty(Rml::PropertyId::ZIndex);
    if (current == nullptr || current->Get<float>() != depth)
        notices.SetProperty(Rml::PropertyId::ZIndex, Rml::Property(depth, Rml::Unit::NUMBER));
}
} // namespace

void UI::Mobile::NoticesUnderPanel::Sync(bool panelOpen)
{
    if (!panelOpen)
    {
        Release();
        return;
    }
    Rml::Element* notices = Find();
    if (notices == nullptr)
        return;
    // Written again each frame only when it differs: a rebuilt document comes at its own depth.
    SetDepth(*notices, UnderOpenMenuDepth);
    g_lowered = true;
}

void UI::Mobile::NoticesUnderPanel::Release()
{
    if (!g_lowered)
        return;
    g_lowered = false;
    // A document rebuilt since came at its own depth already.
    Rml::Element* notices = g_notices.get();
    if (notices == nullptr)
        return;
    const auto depth = UI::RmlBridge::StackingDepthForDocument(NoticesFile);
    if (depth.has_value())
        SetDepth(*notices, *depth);
}
