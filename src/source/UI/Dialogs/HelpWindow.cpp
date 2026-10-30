
#include "stdafx.h"
#include "UI/Dialogs/HelpWindow.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/HelpPages.h"
#include "UI/HUD/TouchControls.h"
#include "UI/Mobile/InterfaceMode.h"
#include "Audio/DSPlaySound.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Render/Text/CUIRenderTextSDLTtf.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlTheme.h"
#include "Render/Text/CUIRenderText.h"

#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>

using namespace SEASON3B;
using namespace mu::ui::window;

namespace
{
UI::Help::Devices DevicesInHand()
{
    return {UI::Mobile::IsTouchControls(), UI::Mobile::IsGamepadInUse()};
}

int PageCount()
{
    return static_cast<int>(UI::Help::Pages(DevicesInHand()).size());
}
} // namespace

mu::ui::window::CHelpWindow::CHelpWindow()
{
    m_pNewUIMng = NULL;

    m_iIndex = 0;
}

mu::ui::window::CHelpWindow::~CHelpWindow()
{
    Release();
}

bool mu::ui::window::CHelpWindow::Create(CManager* pNewUIMng)
{
    if (NULL == pNewUIMng)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_HELP, this);

    BuildRmlUi();

    Show(false);

    return true;
}

void mu::ui::window::CHelpWindow::Release()
{

    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = NULL;
    }

    m_RmlView.Release();
}

bool mu::ui::window::CHelpWindow::UpdateMouseEvent()
{
    // The hint row takes the pointer: a click on it turns the page and stays out of the world. The
    // other rows let the pointer through to the world, as the original's help did (the themes give
    // them no pointer events), but not to a window under them: on a phone's screen the box lies over
    // the inventory, and a tap on its text took off the worn weapon there.
    Rml::ElementDocument* document = m_RmlView.Document();
    if (UI::RmlBridge::IsPointerOver(document))
        return false;
    const bool onBox = document != nullptr && UI::RmlBridge::IsPointerWithin(document->GetElementById("help_panel"));
    return !(onBox && m_pNewUIMng != nullptr && m_pNewUIMng->IsPointerOnWindowUnder(this));
}

bool mu::ui::window::CHelpWindow::UpdateKeyEvent()
{
    if (g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_HELP))
    {
        if (IsPress(VK_F1) == true)
        {
            TurnPage(1);
            return false;
        }

        if (IsPress(VK_ESCAPE) == true)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_HELP);
            PlayBuffer(SOUND_CLICK01);

            return false;
        }
    }

    return true;
}

bool mu::ui::window::CHelpWindow::Update()
{
    SyncRmlModel();
    return true;
}

bool mu::ui::window::CHelpWindow::Render()
{
    // Nothing native left: the page is an RmlUi document. Kept because CObject requires it.
    return true;
}

void mu::ui::window::CHelpWindow::BindRmlModel(Rml::DataModelConstructor& c, HelpWindowRmlModel& model)
{
    c.Bind("text_px", &model.textPx);
    c.Bind("bold_text_px", &model.boldTextPx);
    c.Bind("line_height", &model.lineHeight);
    c.Bind("bold_line_height", &model.boldLineHeight);

    auto line = c.RegisterStruct<HelpLineEntry>();
    line.RegisterMember("text", &HelpLineEntry::text);
    line.RegisterMember("heading", &HelpLineEntry::heading);
    line.RegisterMember("half_spacer", &HelpLineEntry::halfSpacer);
    line.RegisterMember("hint", &HelpLineEntry::hint);
    c.RegisterArray<std::vector<HelpLineEntry>>();
    c.Bind("lines", &model.lines);

    // The hint row turns the page: a phone has no F1, and the menu's Help is two taps away. The
    // other rows let the pointer through to the world (the themes give only the hint row pointer
    // events); the flag keeps them inert in a theme that does not.
    c.BindEventCallback("help_line_click",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments)
                        {
                            if (arguments.empty() || !arguments[0].Get<bool>())
                                return;
                            // On a phone the page often lies over the inventory: the finger's pointer
                            // would show the tooltip of the item under the hint row.
                            UI::HUD::TouchControls::RestPointerOnLift();
                            TurnPage(1);
                        });
}

void mu::ui::window::CHelpWindow::OnRmlReloaded()
{
    m_BuiltPage = -1;
}

void mu::ui::window::CHelpWindow::BuildRmlUi()
{
    m_RmlView.Ensure();
}

void mu::ui::window::CHelpWindow::SyncRmlModel()
{
    BuildRmlUi();
    if (!m_RmlView.Document())
        return;

    // The original drew this page over the location bar and the chat and system logs.
    const bool visible = IsVisible();
    UI::RmlBridge::SyncDocumentVisibilityInFront(m_RmlView.Document(), visible);
    if (!visible)
        return;

    RebuildPageModel();
}

void mu::ui::window::CHelpWindow::RebuildPageModel()
{
    const float textPx = UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Normal);
    const UI::Help::Devices devices = DevicesInHand();
    // A pad put down or picked up while the window is open changes how many pages there are.
    m_iIndex = std::min(m_iIndex, PageCount() - 1);
    if (m_BuiltPage == m_iIndex && m_BuiltTextPx == textPx && m_BuiltTouchControls == devices.touchControls &&
        m_BuiltGamepad == devices.gamepad)
        return;

    m_BuiltPage = m_iIndex;
    m_BuiltTextPx = textPx;
    m_BuiltTouchControls = devices.touchControls;
    m_BuiltGamepad = devices.gamepad;

    const std::vector<UI::Help::PageLine> page = UI::Help::BuildPage(m_iIndex, devices);

    HelpWindowRmlModel& model = m_RmlView.GetModel();
    model.lines.clear();
    model.lines.reserve(page.size());

    for (const UI::Help::PageLine& pageLine : page)
    {
        HelpLineEntry entry;
        entry.heading = pageLine.heading;
        entry.halfSpacer = pageLine.halfSpacer;
        entry.hint = pageLine.hint;
        if (!pageLine.halfSpacer)
            entry.text = StringUtils::WideToNarrow(pageLine.text.c_str());
        model.lines.push_back(std::move(entry));
    }

    model.textPx = textPx;
    model.boldTextPx = UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Bold);
    model.lineHeight = static_cast<float>(CUIRenderTextSDLTtf::LineHeight(UI::Scaling::FontRole::Normal));
    model.boldLineHeight = static_cast<float>(CUIRenderTextSDLTtf::LineHeight(UI::Scaling::FontRole::Bold));

    for (const char* field : {"lines", "text_px", "bold_text_px", "line_height", "bold_line_height"})
        m_RmlView.MarkDirty(field);
}

float mu::ui::window::CHelpWindow::GetLayerDepth()
{
    return 8.3f;
}

float mu::ui::window::CHelpWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CHelpWindow::OpenningProcess()
{
    m_iIndex = 0;
}

void mu::ui::window::CHelpWindow::ClosingProcess() {}

void mu::ui::window::CHelpWindow::AutoUpdateIndex()
{
    TurnPage(1);
}

void mu::ui::window::CHelpWindow::TurnPage(int step)
{
    const int page = std::max(m_iIndex + step, 0);
    if (page < PageCount())
    {
        m_iIndex = page;
        return;
    }
    g_pNewUISystem->Hide(mu::ui::window::INTERFACE_HELP);
    PlayBuffer(SOUND_CLICK01);
}
