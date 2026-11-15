#include "stdafx.h"

#include "UI/Events/EventEntryView.h"

#include "I18N/All.h"
#include "Core/Utilities/StringUtils.h"
#include "UI/RmlBridge/RmlSyncField.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/Mobile/InterfaceMode.h"

#include <RmlUi/Core/ElementDocument.h>

using namespace mu::ui::window;

mu::ui::window::EventEntryView::EventEntryView(const char* modelName, const char* documentPath)
    : m_View(modelName, [this](Rml::DataModelConstructor& c, EventEntryRmlModel& model) { BindModel(c, model); },
             {{documentPath}}, {.stacking = UI::RmlBridge::ThemedStacking::Front})
{
}

void mu::ui::window::EventEntryView::BindModel(Rml::DataModelConstructor& c, EventEntryRmlModel& model)
{
    c.Bind("text_px", &model.textPx);
    c.Bind("bold_text_px", &model.boldTextPx);
    c.Bind("title_text", &model.titleText);
    c.Bind("exit_tooltip", &model.exitTooltip);

    auto line = c.RegisterStruct<EventEntryLineEntry>();
    line.RegisterMember("text", &EventEntryLineEntry::text);
    c.RegisterArray<std::vector<EventEntryLineEntry>>();
    c.Bind("lines", &model.lines);

    auto button = c.RegisterStruct<EventEntryButtonEntry>();
    button.RegisterMember("label", &EventEntryButtonEntry::label);
    button.RegisterMember("enabled", &EventEntryButtonEntry::enabled);
    c.RegisterArray<std::vector<EventEntryButtonEntry>>();
    c.Bind("buttons", &model.buttons);

    c.BindEventCallback("entry_press",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments)
                        {
                            if (arguments.size() == 1)
                                m_PressedButton = arguments[0].Get<int>(-1);
                        });
    c.BindEventCallback("entry_exit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_ExitPressed = true; });
    model.exitTooltip = StringUtils::WideToNarrow(I18N::Game::Close388);
}

void mu::ui::window::EventEntryView::Build()
{
    m_View.Ensure();
}

void mu::ui::window::EventEntryView::SetContent(const wchar_t* title, const std::vector<std::wstring>& lines,
                                                const std::vector<Button>& buttons)
{
    Build();
    EventEntryRmlModel& model = m_View.GetModel();
    model.titleText = StringUtils::WideToNarrow(title != nullptr ? title : L"");
    model.lines.clear();
    for (const std::wstring& text : lines)
        model.lines.push_back({StringUtils::WideToNarrow(text.c_str())});
    model.buttons.clear();
    for (const Button& button : buttons)
        model.buttons.push_back({StringUtils::WideToNarrow(button.label.c_str()), button.enabled});
    m_View.MarkDirty("title_text");
    m_View.MarkDirty("lines");
    m_View.MarkDirty("buttons");

    m_ScrollButtonCount = static_cast<int>(buttons.size());
    m_ScrollTargetIndex = -1;
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        if (buttons[i].enabled)
        {
            m_ScrollTargetIndex = static_cast<int>(i);
            break;
        }
    }
    m_ScrollToEnabledPending = m_ScrollTargetIndex >= 0;
    m_ScrollFramesLeft = ScrollWaitFrames;
}

bool mu::ui::window::EventEntryView::IsPointerOver() const
{
    return UI::RmlBridge::IsPointerOver(m_View.Document());
}

void mu::ui::window::EventEntryView::Sync(bool visible)
{
    Build();
    if (!m_View.Document())
        return;

    // Layer depth 4.0/4.1: over the HUD like every panel the original opened.
    UI::RmlBridge::SyncDocumentVisibilityInFront(m_View.Document(), visible);
    if (!visible)
        return;

    SyncTextSizes();
    ScrollEnabledButtonIntoView();
}

// The phone view's list of level bands is taller than the panel: when the hero's band (the enabled one) starts
// below the visible part, the list scrolls just far enough to show it. Elsewhere the whole list fits.
void mu::ui::window::EventEntryView::ScrollEnabledButtonIntoView()
{
    if (!m_ScrollToEnabledPending)
        return;
    if (!UI::Mobile::IsPhoneView() || m_ScrollFramesLeft <= 0)
    {
        m_ScrollToEnabledPending = false;
        return;
    }

    // SetContent() runs before the data model rebuilds the bands, so the old bands can still be there: skip the
    // first frame, then wait (a few frames at most) until the list has the new count with the hero's band enabled.
    const bool firstFrame = m_ScrollFramesLeft == ScrollWaitFrames;
    --m_ScrollFramesLeft;
    if (firstFrame)
        return;
    Rml::ElementDocument* document = m_View.Document();
    if (!document)
        return;
    document->UpdateDocument();
    Rml::Element* levels = document->GetElementById("entry_levels");
    if (!levels)
        return;
    Rml::ElementList bands;
    levels->GetElementsByClassName(bands, "entry-btn");
    if (static_cast<int>(bands.size()) != m_ScrollButtonCount)
        return;
    Rml::Element* target = bands[static_cast<std::size_t>(m_ScrollTargetIndex)];
    if (!target->IsClassSet("enabled"))
        return;
    m_ScrollToEnabledPending = false;
    target->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest,
                                                               Rml::ScrollAlignment::Nearest,
                                                               Rml::ScrollBehavior::Instant,
                                                               Rml::ScrollParentage::Closest));
}

void mu::ui::window::EventEntryView::SyncTextSizes()
{
    SyncField(m_View.Binder(), &EventEntryRmlModel::textPx, "text_px",
              UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Normal));
    SyncField(m_View.Binder(), &EventEntryRmlModel::boldTextPx, "bold_text_px",
              UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Bold));
}

int mu::ui::window::EventEntryView::TakePressedButton()
{
    const int pressed = m_PressedButton;
    m_PressedButton = -1;
    const EventEntryRmlModel& model = m_View.GetModel();
    if (pressed < 0 || pressed >= static_cast<int>(model.buttons.size()) || !model.buttons[pressed].enabled)
        return -1;
    return pressed;
}

bool mu::ui::window::EventEntryView::TakeExitPressed()
{
    const bool pressed = m_ExitPressed;
    m_ExitPressed = false;
    return pressed;
}
