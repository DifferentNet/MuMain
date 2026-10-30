#include "stdafx.h"
#include "UI/HUD/PadKeyboard.h"

#include "Core/Input/TouchInput.h"
#include "Core/Input/UiInputRouter.h"
#include "Core/Platform/GameWindow.h"
#include "Core/Utilities/StringUtils.h"
#include "I18N/All.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Scenes/SceneCore.h"
#include "UI/Core/WindowSystem.h"
#include "UI/HUD/ChatInputBox.h"
#include "UI/HUD/PadKeyboardLayout.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlNumericInputFilter.h"
#include "UI/RmlBridge/RmlThemedView.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlTextArea.h>
#include <RmlUi/Core/StringUtilities.h>
#include <SDL3/SDL.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace
{
namespace Layout = UI::HUD::PadKeyboardLayout;
namespace Pad = Core::Input::Gamepad;
using Layout::Action;
using Layout::Page;
using Layout::Spot;
using Pad::Button;
using Clock = std::chrono::steady_clock;

// The highlight moves at once with the stick or the d-pad, again after this while they are held,
// then at the second pace. Backspace and the cursor repeat the same way, a little faster.
constexpr auto FirstRepeat = std::chrono::milliseconds(400);
constexpr auto MoveRepeat = std::chrono::milliseconds(110);
constexpr auto KeyRepeat = std::chrono::milliseconds(70);
// A field whose middle is lower than this share of the screen gets the keyboard at the top.
constexpr float LowFieldShare = 0.55f;
// How much of the field's text the keyboard's line shows either side of the cursor, in characters.
constexpr int LineBefore = 56;
constexpr int LineAfter = 24;
// Where the highlight starts: the middle of the letters' home row, the number pad's 5.
constexpr Spot LettersStart{2, 4};
constexpr Spot DigitsStart{1, 1};

constexpr std::string_view PasswordDot = "\xE2\x80\xA2"; // U+2022

enum class Shift : std::uint8_t
{
    Off,
    // For the next letter only.
    Once,
    Locked,
};

struct KeyView
{
    Rml::String label;
    // Its share of the row, as an RCSS width ("10%").
    Rml::String width;
    // Not a letter: drawn darker.
    bool action = false;
    // A Shift that is on, and on to stay.
    bool on = false;
    bool locked = false;
    // A symbol drawn from DejaVu Sans (the only bundled font with the keyboard's arrows).
    bool icon = false;
    // The pad button that also presses it: "x", "y", "lt", "rt", "view", "menu" or none.
    Rml::String glyph;
};

struct RowView
{
    std::vector<KeyView> keys;
};

struct KeyboardModel
{
    std::vector<RowView> rows;
    int focusRow = 0;
    int focusColumn = 0;
    // The field's text either side of the cursor.
    Rml::String before;
    Rml::String after;
    bool top = false;
    bool digits = false;
    Rml::String typeLabel;
    Rml::String caretLabel;
    Rml::String closeLabel;
};

void BindModel(Rml::DataModelConstructor& c, KeyboardModel& model)
{
    auto key = c.RegisterStruct<KeyView>();
    key.RegisterMember("label", &KeyView::label);
    key.RegisterMember("width", &KeyView::width);
    key.RegisterMember("action", &KeyView::action);
    key.RegisterMember("on", &KeyView::on);
    key.RegisterMember("locked", &KeyView::locked);
    key.RegisterMember("icon", &KeyView::icon);
    key.RegisterMember("glyph", &KeyView::glyph);
    c.RegisterArray<std::vector<KeyView>>();
    auto row = c.RegisterStruct<RowView>();
    row.RegisterMember("keys", &RowView::keys);
    c.RegisterArray<std::vector<RowView>>();

    c.Bind("rows", &model.rows);
    c.Bind("focus_row", &model.focusRow);
    c.Bind("focus_column", &model.focusColumn);
    c.Bind("before", &model.before);
    c.Bind("after", &model.after);
    c.Bind("top", &model.top);
    c.Bind("digits", &model.digits);
    c.Bind("type_label", &model.typeLabel);
    c.Bind("caret_label", &model.caretLabel);
    c.Bind("close_label", &model.closeLabel);
}

// Over every window and the login scene's windows and messages, under the loading screen
// (RmlStackingOrder.cpp); shown without focus, so the field keeps it.
UI::RmlBridge::ThemedView<KeyboardModel> g_View{"pad_keyboard", BindModel, {{"Data/Interface/RmlUi/pad_keyboard.rml"}}};

struct State
{
    // The field it types into, only compared: a different one starts the keyboard over.
    const Rml::Element* field = nullptr;
    Page page = Page::Latin;
    // The letters the symbols page goes back to, and a new field starts with.
    Page letters = Page::Latin;
    Shift shift = Shift::Off;
    Spot spot = LettersStart;
    bool rowsDirty = true;
    Pad::Stepper stepper{FirstRepeat, MoveRepeat};
    Pad::RepeatButton erase{FirstRepeat, KeyRepeat};
    Pad::RepeatButton caretLeft{FirstRepeat, KeyRepeat};
    Pad::RepeatButton caretRight{FirstRepeat, KeyRepeat};
};
State g_state;

bool Pressed(const Pad::Presses& presses, Button button)
{
    return presses[static_cast<int>(button)];
}

bool Held(Button button)
{
    return Pad::GetButton(button).held;
}

bool IsLetters(Page page)
{
    return page == Page::Latin || page == Page::Cyrillic;
}

bool IsPassword(const Rml::Element* field)
{
    return field->GetAttribute<Rml::String>("type", "") == "password";
}

bool IsDigitsOnly(const Rml::Element* field)
{
    return field->IsClassSet(UI::RmlBridge::NumericFieldClass);
}

// The field's middle is in the screen's lower part, where the keyboard would cover it.
bool IsLow(Rml::Element* field)
{
    const Rml::Context* context = field->GetContext();
    if (context == nullptr)
        return false;
    const float middle = field->GetAbsoluteTop() + field->GetOffsetHeight() / 2.f;
    return middle > static_cast<float>(context->GetDimensions().y) * LowFieldShare;
}

// --- Into the field, as from a real keyboard ---------------------------------------------------

void Route(SDL_Event& event)
{
    SDL_Window* window = Core::Platform::GameWindow();
    event.common.timestamp = SDL_GetTicksNS();
    Core::Input::RouteToUi(event, window);
}

void SendText(const std::string& text)
{
    SDL_Window* window = Core::Platform::GameWindow();
    SDL_Event event{};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.windowID = window != nullptr ? SDL_GetWindowID(window) : 0;
    event.text.text = text.c_str();
    Route(event);
}

void SendKey(SDL_Keycode key, SDL_Scancode scancode)
{
    SDL_Window* window = Core::Platform::GameWindow();
    for (const bool down : {true, false})
    {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.windowID = window != nullptr ? SDL_GetWindowID(window) : 0;
        event.key.key = key;
        event.key.scancode = scancode;
        event.key.down = down;
        Route(event);
    }
}

// The field's own Enter (a search field's keydown) and the game's (the chat sends, the login logs in).
void Enter()
{
    SendKey(SDLK_RETURN, SDL_SCANCODE_RETURN);
    Core::Input::Touch::TapKey(VK_RETURN);
}

// Whether the field is the chat's line. The chat stays open while another field has the keys (a
// dialog's amount, a window's search), and Esc would go to that one's window instead.
bool IsChatField(const Rml::Element* field)
{
    if (SceneFlag != MAIN_SCENE || !g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_CHATINPUTBOX))
        return false;
    const mu::ui::window::CChatInputBox* chat = g_pChatInputBox;
    return chat != nullptr && chat->TakesTypingFrom(field->GetOwnerDocument());
}

// The chat closes as with Esc; any other field just lets go of the keys.
void Close(Rml::Element* field)
{
    if (IsChatField(field))
    {
        Core::Input::Touch::TapKey(VK_ESCAPE);
        return;
    }
    field->Blur();
}

// --- The keyboard's state ------------------------------------------------------------------------

void SetPage(Page page)
{
    g_state.page = page;
    g_state.spot = Layout::Clamp(page, g_state.spot);
    g_state.rowsDirty = true;
}

void ToggleSymbols()
{
    if (g_state.page == Page::Digits)
        return;
    SetPage(g_state.page == Page::Symbols ? g_state.letters : Page::Symbols);
}

void SwitchLanguage(const Rml::Element* field)
{
    if (g_state.page == Page::Digits || IsPassword(field))
        return;
    g_state.letters = g_state.letters == Page::Latin ? Page::Cyrillic : Page::Latin;
    if (IsLetters(g_state.page))
        SetPage(g_state.letters);
    else
        g_state.rowsDirty = true;
}

void CycleShift()
{
    if (!IsLetters(g_state.page))
        return;
    switch (g_state.shift)
    {
    case Shift::Off:
        g_state.shift = Shift::Once;
        break;
    case Shift::Once:
        g_state.shift = Shift::Locked;
        break;
    default:
        g_state.shift = Shift::Off;
        break;
    }
    g_state.rowsDirty = true;
}

void Type(const Layout::Key& key)
{
    const bool capital = g_state.shift != Shift::Off && IsLetters(g_state.page);
    SendText(std::string(capital ? key.upper : key.lower));
    if (g_state.shift == Shift::Once)
    {
        g_state.shift = Shift::Off;
        g_state.rowsDirty = true;
    }
}

void Erase()
{
    SendKey(SDLK_BACKSPACE, SDL_SCANCODE_BACKSPACE);
}

void Press(const Layout::Key& key, Rml::Element* field)
{
    switch (key.action)
    {
    case Action::Type:
        Type(key);
        break;
    case Action::Shift:
        CycleShift();
        break;
    case Action::Backspace:
        Erase();
        break;
    case Action::Space:
        SendText(" ");
        break;
    case Action::Enter:
        Enter();
        break;
    case Action::Tab:
        SendKey(SDLK_TAB, SDL_SCANCODE_TAB);
        break;
    case Action::Symbols:
    case Action::Letters:
        ToggleSymbols();
        break;
    case Action::Language:
        SwitchLanguage(field);
        break;
    }
}

void Open(Rml::Element* field)
{
    g_state.field = field;
    g_state.shift = Shift::Off;
    if (IsPassword(field) && g_state.letters == Page::Cyrillic)
        g_state.letters = Page::Latin;
    const bool digits = IsDigitsOnly(field);
    g_state.page = digits ? Page::Digits : g_state.letters;
    g_state.spot = Layout::Clamp(g_state.page, digits ? DigitsStart : LettersStart);
    g_state.rowsDirty = true;
    g_state.stepper.Reset();

    KeyboardModel& model = g_View.GetModel();
    model.top = IsLow(field);
    model.digits = digits;
    g_View.MarkDirty("top");
    g_View.MarkDirty("digits");
}

void Act(Rml::Element* field, const Pad::Presses& presses, Clock::time_point now)
{
    const Pad::Step step = g_state.stepper.Take(presses, now);
    if (step.dx != 0 || step.dy != 0)
        g_state.spot = Layout::Step(g_state.page, g_state.spot, step.dx, step.dy);
    if (Pressed(presses, Button::A))
        Press(Layout::KeyAt(g_state.page, g_state.spot), field);
    if (g_state.erase.Take(Pressed(presses, Button::X), Held(Button::X), now))
        Erase();
    if (Pressed(presses, Button::Y))
        SendText(" ");
    if (Pressed(presses, Button::LeftTrigger))
        CycleShift();
    if (Pressed(presses, Button::RightTrigger))
        ToggleSymbols();
    if (Pressed(presses, Button::View))
        SwitchLanguage(field);
    if (g_state.caretLeft.Take(Pressed(presses, Button::LeftShoulder), Held(Button::LeftShoulder), now))
        SendKey(SDLK_LEFT, SDL_SCANCODE_LEFT);
    if (g_state.caretRight.Take(Pressed(presses, Button::RightShoulder), Held(Button::RightShoulder), now))
        SendKey(SDLK_RIGHT, SDL_SCANCODE_RIGHT);
    if (Pressed(presses, Button::Menu))
        Enter();
    if (Pressed(presses, Button::B))
        Close(field);
}

// --- What it shows -------------------------------------------------------------------------------

Rml::String LetterLabel(const Layout::Key& key)
{
    const bool capital = g_state.shift != Shift::Off && IsLetters(g_state.page);
    return Rml::String(capital ? key.upper : key.lower);
}

Rml::String ActionLabel(Action action)
{
    switch (action)
    {
    case Action::Shift:
        return "\xE2\x87\xA7"; // U+21E7
    case Action::Backspace:
        return "\xE2\x8C\xAB"; // U+232B
    case Action::Enter:
        return "\xE2\x86\xB5"; // U+21B5
    case Action::Tab:
        return "\xE2\x87\xA5"; // U+21E5
    case Action::Space:
        return StringUtils::WideToNarrow(I18N::Game::PadKeyboardSpace);
    case Action::Symbols:
        return "?123";
    case Action::Letters:
        return g_state.letters == Page::Cyrillic ? "\xD0\x90\xD0\x91\xD0\x92" : "ABC"; // АБВ
    case Action::Language:
        return g_state.letters == Page::Latin ? "RU" : "EN";
    default:
        return {};
    }
}

bool IsIcon(Action action)
{
    return action == Action::Shift || action == Action::Backspace || action == Action::Enter || action == Action::Tab;
}

Rml::String GlyphFor(Action action)
{
    switch (action)
    {
    case Action::Backspace:
        return "x";
    case Action::Space:
        return "y";
    case Action::Shift:
        return "lt";
    case Action::Symbols:
    case Action::Letters:
        return "rt";
    case Action::Language:
        return "view";
    case Action::Enter:
        return "menu";
    default:
        return {};
    }
}

KeyView ViewOf(const Layout::Key& key, float pageWidth)
{
    KeyView view;
    const bool letter = key.action == Action::Type;
    view.label = letter ? LetterLabel(key) : ActionLabel(key.action);
    view.width = Rml::CreateString("%.4f%%", key.width / pageWidth * 100.f);
    view.action = !letter;
    view.on = key.action == Action::Shift && g_state.shift != Shift::Off;
    view.locked = key.action == Action::Shift && g_state.shift == Shift::Locked;
    view.icon = IsIcon(key.action);
    view.glyph = GlyphFor(key.action);
    return view;
}

void SyncRows(KeyboardModel& model)
{
    if (!g_state.rowsDirty)
        return;
    g_state.rowsDirty = false;
    const float pageWidth = Layout::Width(g_state.page);
    model.rows.clear();
    for (const auto row : Layout::Rows(g_state.page))
    {
        RowView rowView;
        for (const Layout::Key& key : row)
            rowView.keys.push_back(ViewOf(key, pageWidth));
        model.rows.push_back(std::move(rowView));
    }
    g_View.MarkDirty("rows");
}

void SyncInt(int& field, int value, const char* name)
{
    if (field == value)
        return;
    field = value;
    g_View.MarkDirty(name);
}

void SyncString(Rml::String& field, Rml::String value, const char* name)
{
    if (field == value)
        return;
    field = std::move(value);
    g_View.MarkDirty(name);
}

// The cursor, in characters from the start.
int CaretOf(Rml::Element* field, const Rml::String& value)
{
    int start = 0;
    int end = static_cast<int>(Rml::StringUtilities::LengthUTF8(value));
    if (auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(field))
        input->GetSelection(&start, &end, nullptr);
    else if (auto* area = rmlui_dynamic_cast<Rml::ElementFormControlTextArea*>(field))
        area->GetSelection(&start, &end, nullptr);
    return end;
}

Rml::String Dots(size_t count)
{
    Rml::String dots;
    for (size_t dot = 0; dot < count; ++dot)
        dots += PasswordDot;
    return dots;
}

// The last (or first) characters of a text, at most `count` of them.
Rml::String LastCharacters(const Rml::String& text, int count)
{
    const int length = static_cast<int>(Rml::StringUtilities::LengthUTF8(text));
    if (length <= count)
        return text;
    return text.substr(Rml::StringUtilities::ConvertCharacterOffsetToByteOffset(text, length - count));
}

Rml::String FirstCharacters(const Rml::String& text, int count)
{
    return text.substr(0, Rml::StringUtilities::ConvertCharacterOffsetToByteOffset(text, count));
}

void SyncLine(Rml::Element* field, KeyboardModel& model)
{
    const auto* control = rmlui_dynamic_cast<Rml::ElementFormControl*>(field);
    Rml::String value = control != nullptr ? control->GetValue() : Rml::String();
    const int caret = CaretOf(field, value);
    if (IsPassword(field))
        value = Dots(Rml::StringUtilities::LengthUTF8(value));
    const int split = Rml::StringUtilities::ConvertCharacterOffsetToByteOffset(value, caret);
    SyncString(model.before, LastCharacters(value.substr(0, split), LineBefore), "before");
    SyncString(model.after, FirstCharacters(value.substr(split), LineAfter), "after");
}

void SyncLabels(KeyboardModel& model)
{
    SyncString(model.typeLabel, StringUtils::WideToNarrow(I18N::Game::PadKeyboardType), "type_label");
    SyncString(model.caretLabel, StringUtils::WideToNarrow(I18N::Game::PadKeyboardCaret), "caret_label");
    SyncString(model.closeLabel, StringUtils::WideToNarrow(I18N::Game::PadClose), "close_label");
}

void Sync(Rml::Element* field)
{
    KeyboardModel& model = g_View.GetModel();
    SyncRows(model);
    SyncInt(model.focusRow, g_state.spot.row, "focus_row");
    SyncInt(model.focusColumn, g_state.spot.column, "focus_column");
    SyncLine(field, model);
    SyncLabels(model);
}

void Hide()
{
    g_state.field = nullptr;
    UI::RmlBridge::SyncDocumentVisibility(g_View.Document(), false);
}
} // namespace

namespace UI::HUD::PadKeyboard
{
bool Update(bool padInUse, const Core::Input::Gamepad::Presses& presses)
{
    Rml::Element* field = padInUse ? RmlUiRuntime::Instance().GetFocusedTextField() : nullptr;
    if (field == nullptr)
    {
        Hide();
        return false;
    }
    if (!g_View.Ensure())
        return false;
    Rml::ElementDocument* document = g_View.Document();
    if (document == nullptr)
        return false;
    if (field != g_state.field)
        Open(field);
    // Shown again after a theme switch rebuilt the document, too.
    if (!document->IsVisible())
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);

    Act(field, presses, Clock::now());
    Sync(field);
    return true;
}
} // namespace UI::HUD::PadKeyboard
