#include "stdafx.h"
#include "UI/HUD/GraphicsHint.h"

#include "Audio/DSPlaySound.h"
#include "Core/Utilities/StringUtils.h"
#include "I18N/All.h"
#include "Render/Look/LowFpsHintPolicy.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

#include <string>

namespace
{
using UI::GraphicsHint::Answer;

struct HintModel
{
    Rml::String titleText;
    Rml::String bodyText;
    Rml::String noteText;
    bool hasNote = false;
    Rml::String lowerLabel;
    Rml::String keepLabel;
    Rml::String neverLabel;
};

Answer g_Answer = Answer::None;

void Choose(Answer answer);

void BindModel(Rml::DataModelConstructor& c, HintModel& model)
{
    c.Bind("title_text", &model.titleText);
    c.Bind("body_text", &model.bodyText);
    c.Bind("note_text", &model.noteText);
    c.Bind("has_note", &model.hasNote);
    c.Bind("lower_label", &model.lowerLabel);
    c.Bind("keep_label", &model.keepLabel);
    c.Bind("never_label", &model.neverLabel);
    c.BindEventCallback("graphics_hint_lower",
                        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { Choose(Answer::Lower); });
    c.BindEventCallback("graphics_hint_keep",
                        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { Choose(Answer::Keep); });
    c.BindEventCallback("graphics_hint_never",
                        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { Choose(Answer::Never); });
}

// Built on the first hint. No focus and no modal flag: the toast must never take Enter, Esc or the chat's typing.
UI::RmlBridge::ThemedView<HintModel> g_View{"graphics_hint", BindModel,
                                            {{"Data/Interface/RmlUi/graphics_hint.rml"}},
                                            {.modal = Rml::ModalFlag::None, .focus = Rml::FocusFlag::None}};

void Choose(Answer answer)
{
    g_Answer = answer;
    PlayBuffer(SOUND_CLICK01);
    g_View.Hide();
}

constexpr size_t TextBufferSize = 512;

std::wstring Formatted(const wchar_t* format, int value)
{
    wchar_t text[TextBufferSize] = {};
    mu_swprintf_s(text, format, value);
    return text;
}

std::wstring Formatted(const wchar_t* format, const wchar_t* value)
{
    wchar_t text[TextBufferSize] = {};
    mu_swprintf_s(text, format, value);
    return text;
}

void SetText(Rml::String HintModel::*field, const char* boundName, const std::wstring& text)
{
    const std::string utf8 = StringUtils::WideToNarrow(text.c_str());
    if (g_View.GetModel().*field == utf8)
        return;
    g_View.GetModel().*field = utf8;
    g_View.MarkDirty(boundName);
}

void FillModel(GraphicsPreset target, int fps)
{
    const bool classicNote = Render::Look::LowFpsHint::TargetIsClassicLook(target);
    SetText(&HintModel::titleText, "title_text", I18N::Game::GraphicsHintTitle);
    SetText(&HintModel::bodyText, "body_text", Formatted(I18N::Game::GraphicsHintBody, fps));
    SetText(&HintModel::noteText, "note_text", classicNote ? I18N::Game::GraphicsHintClassicNote : L"");
    SetText(&HintModel::lowerLabel, "lower_label",
            Formatted(I18N::Game::GraphicsHintLower, UI::GraphicsHint::PresetLabel(target)));
    SetText(&HintModel::keepLabel, "keep_label", I18N::Game::GraphicsHintKeep);
    SetText(&HintModel::neverLabel, "never_label", I18N::Game::GraphicsHintNever);
    if (g_View.GetModel().hasNote != classicNote)
    {
        g_View.GetModel().hasNote = classicNote;
        g_View.MarkDirty("has_note");
    }
}
} // namespace

namespace UI::GraphicsHint
{

void Show(GraphicsPreset target, int fps)
{
    g_Answer = Answer::None;
    if (!g_View.Ensure())
        return;
    FillModel(target, fps);
    if (Rml::ElementDocument* document = g_View.Document())
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
}

void Hide()
{
    if (g_View.IsBuilt())
        g_View.Hide();
}

bool IsShown()
{
    const Rml::ElementDocument* document = g_View.IsBuilt() ? g_View.Document() : nullptr;
    return document != nullptr && document->IsVisible();
}

Answer TakeAnswer()
{
    const Answer answer = g_Answer;
    g_Answer = Answer::None;
    return answer;
}

const wchar_t* PresetLabel(GraphicsPreset preset)
{
    switch (preset)
    {
    case GraphicsPreset::Low:
        return I18N::Game::GraphicsPresetLow;
    case GraphicsPreset::Medium:
        return I18N::Game::GraphicsPresetMedium;
    case GraphicsPreset::High:
        return I18N::Game::GraphicsPresetHigh;
    case GraphicsPreset::Ultra:
        return I18N::Game::GraphicsPresetUltra;
    case GraphicsPreset::Auto:
    case GraphicsPreset::Custom:
        break;
    }
    return I18N::Game::GraphicsPresetCustom;
}

} // namespace UI::GraphicsHint
