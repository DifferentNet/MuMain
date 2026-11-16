#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Options/KeyBindingsWindow.h"

#include "Audio/DSPlaySound.h"
#include "Core/Input/KeyBindingsRuntime.h"
#include "Core/Utilities/StringUtils.h"
#include "UI/Core/WindowSystem.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlDraggable.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlSyncField.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <cwchar>
#include <iterator>
#include <string>
#include <utility>

using namespace SEASON3B;
using namespace mu::ui::window;
namespace KeyBindings = Core::Input::KeyBindings;
using KeyAction = KeyBindings::Action;

namespace
{
// How long the row of the action that took the other key in a swap flashes.
constexpr std::chrono::milliseconds SwapFlashTime{700};

// What a waiting row shows as its key.
constexpr const char* WaitingKeyText = "...";

// Keys looked at while waiting, besides the letters and the bindable keys: pressing one of these says it can't be
// bound. Mouse buttons are left out (the click on the row starts the wait); Escape cancels and is checked first.
constexpr int FixedKeys[] = {
    VK_TAB, VK_RETURN, VK_SPACE, VK_BACK, VK_PRIOR, VK_NEXT, VK_END, VK_INSERT, VK_DELETE, VK_LEFT, VK_RIGHT, VK_UP,
    VK_DOWN, VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
};

// A row's element id, for scrolling it into view.
Rml::String RowId(int action)
{
    return "kb_row_" + std::to_string(action);
}

// The names of the window actions, in Action order (the potion and skill rows are numbered instead).
const wchar_t* const* const WindowLabels[] = {
    &I18N::Game::TouchMenuInventory,    &I18N::Game::KeyBindingsInventoryAlt, &I18N::Game::TouchMenuCharacter,
    &I18N::Game::TouchMenuParty,        &I18N::Game::TouchMenuGuild,          &I18N::Game::TouchMenuFriends,
    &I18N::Game::TouchMenuQuests,       &I18N::Game::TouchMenuMove,           &I18N::Game::TouchMenu,
    &I18N::Game::KeyBindingsCommand,    &I18N::Game::TouchMenuCommands,       &I18N::Game::TouchMenuEvents,
    &I18N::Game::TouchMenuDaily,        &I18N::Game::TouchMenuBattlePass,     &I18N::Game::Gens,
    &I18N::Game::TouchMenuHelper,       &I18N::Game::KeyBindingsHelperToggle,
};
static_assert(std::size(WindowLabels) == static_cast<size_t>(KeyAction::Potion1), "one label per window action");

Rml::String Narrow(const wchar_t* text)
{
    return StringUtils::WideToNarrow(text);
}

Rml::String Numbered(const wchar_t* format, int number)
{
    wchar_t text[128] = {};
    std::swprintf(text, std::size(text), format, number);
    return Narrow(text);
}

// "Skill slot 1".."Skill slot 9", then "Skill slot 0": the slot's number is its default key.
Rml::String ActionLabel(const KeyBindings::ActionInfo& info)
{
    const int index = static_cast<int>(info.action);
    switch (info.group)
    {
    case KeyBindings::Group::Windows:
        return Narrow(*WindowLabels[index]);
    case KeyBindings::Group::Potions:
        return Numbered(I18N::Game::KeyBindingsPotionSlot, index - static_cast<int>(KeyAction::Potion1) + 1);
    case KeyBindings::Group::Skills:
        return Numbered(I18N::Game::KeyBindingsSkillSlot, info.defaultKey - '0');
    }
    return {};
}

KeyBindingsLabels MakeLabels()
{
    KeyBindingsLabels labels;
    labels.windows = Narrow(I18N::Game::KeyBindingsWindows);
    labels.potions = Narrow(I18N::Game::KeyBindingsPotions);
    labels.skills = Narrow(I18N::Game::KeyBindingsSkills);
    labels.reset = Narrow(I18N::Game::KeyBindingsReset);
    labels.close = Narrow(I18N::Game::KeyBindingsClose);
    return labels;
}

void BindStructs(Rml::DataModelConstructor& c)
{
    auto labels = c.RegisterStruct<KeyBindingsLabels>();
    labels.RegisterMember("windows", &KeyBindingsLabels::windows);
    labels.RegisterMember("potions", &KeyBindingsLabels::potions);
    labels.RegisterMember("skills", &KeyBindingsLabels::skills);
    labels.RegisterMember("reset", &KeyBindingsLabels::reset);
    labels.RegisterMember("close", &KeyBindingsLabels::close);

    auto row = c.RegisterStruct<KeyBindingsRow>();
    row.RegisterMember("label", &KeyBindingsRow::label);
    row.RegisterMember("key", &KeyBindingsRow::keyText);
    row.RegisterMember("index", &KeyBindingsRow::index);
    row.RegisterMember("id", &KeyBindingsRow::id);
    row.RegisterMember("capturing", &KeyBindingsRow::capturing);
    row.RegisterMember("flash", &KeyBindingsRow::flash);
    c.RegisterArray<std::vector<KeyBindingsRow>>();
}

int FirstArg(const Rml::VariantList& args, int fallback = -1)
{
    return args.empty() ? fallback : args[0].Get<int>(fallback);
}

bool IsValidAction(int action)
{
    return action >= 0 && action < KeyBindings::ActionCount;
}

// The bindable key pressed this frame, or 0.
int PressedBindableKey()
{
    if (IsPress(KeyBindings::KeyHome))
        return KeyBindings::KeyHome;
    for (int key = '0'; key <= '9'; ++key)
    {
        if (IsPress(key))
            return key;
    }
    for (int key = 'A'; key <= 'Z'; ++key)
    {
        if (KeyBindings::IsAssignable(key) && IsPress(key))
            return key;
    }
    return 0;
}

// A key that may not be bound pressed this frame: a fixed key or a letter the game keeps (KeyBindings::IsAssignable).
bool PressedRefusedKey()
{
    for (int key : FixedKeys)
    {
        if (IsPress(key))
            return true;
    }
    for (int key = 'A'; key <= 'Z'; ++key)
    {
        if (!KeyBindings::IsAssignable(key) && IsPress(key))
            return true;
    }
    return false;
}

std::vector<KeyBindingsRow>& GroupRows(KeyBindingsRmlModel& model, KeyBindings::Group group)
{
    switch (group)
    {
    case KeyBindings::Group::Potions:
        return model.potions;
    case KeyBindings::Group::Skills:
        return model.skills;
    case KeyBindings::Group::Windows:
        break;
    }
    return model.windows;
}
} // namespace

CKeyBindingsWindow::CKeyBindingsWindow() = default;

CKeyBindingsWindow::~CKeyBindingsWindow()
{
    Release();
}

bool CKeyBindingsWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(INTERFACE_KEY_BINDINGS, this);
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void CKeyBindingsWindow::Release()
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float CKeyBindingsWindow::GetLayerDepth()
{
    return LayerDepth;
}

float CKeyBindingsWindow::GetKeyEventOrder()
{
    return KeyEventOrder;
}

void CKeyBindingsWindow::OpenningProcess()
{
    m_pending.clear();
    StopCapture();
    m_flashing = -1;
    m_scrollTo = -1;
}

void CKeyBindingsWindow::ClosingProcess()
{
    m_pending.clear();
    StopCapture();
}

void CKeyBindingsWindow::OnRmlBuilt()
{
    Rml::ElementDocument* document = m_RmlView.Document();
    if (document == nullptr)
        return;
    // Dragged by any part that is not a control (base.rcss blocks those).
    if (Rml::Element* panel = document->GetElementById("panel"))
        UI::RmlBridge::MakeDraggable(panel, panel, nullptr, [panel] { UI::RmlBridge::KeepInsideWindow(panel); });
}

bool CKeyBindingsWindow::UpdateMouseEvent()
{
    // Every control is RmlUi's (see Update()); a click on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool CKeyBindingsWindow::UpdateKeyEvent()
{
    if (!IsVisible())
        return true;

    if (m_capturing < 0)
    {
        if (!IsPress(VK_ESCAPE))
            return true;
        Close();
        return false;
    }

    if (IsPress(VK_ESCAPE))
        StopCapture();
    else
        CaptureKey();
    // While waiting no key reaches the hot keys behind.
    return false;
}

void CKeyBindingsWindow::CaptureKey()
{
    const int key = PressedBindableKey();
    if (key != 0)
    {
        const auto action = static_cast<KeyAction>(m_capturing);
        const KeyBindings::Table::Change change = KeyBindings::Runtime::Assign(action, key);
        StopCapture();
        if (change.swapped != KeyAction::None)
        {
            m_flashing = static_cast<int>(change.swapped);
            m_flashUntil = Clock::now() + SwapFlashTime;
            m_scrollTo = m_flashing;
        }
        PlayBuffer(SOUND_CLICK01);
        return;
    }

    if (PressedRefusedKey())
        m_refused = true;
}

bool CKeyBindingsWindow::Update()
{
    if (IsVisible())
        ApplyPending();
    else
        m_pending.clear();

    if (m_flashing >= 0 && Clock::now() >= m_flashUntil)
        m_flashing = -1;

    SyncRmlModel();
    ScrollToPendingRow();
    return true;
}

void CKeyBindingsWindow::ScrollToPendingRow()
{
    if (m_scrollTo < 0 || !IsVisible())
        return;
    Rml::ElementDocument* document = m_RmlView.Document();
    Rml::Element* row = document != nullptr ? document->GetElementById(RowId(m_scrollTo)) : nullptr;
    m_scrollTo = -1;
    if (row == nullptr)
        return;
    // The other row may be in another group, out of sight: it comes into view so its flash is seen.
    row->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest,
                                                   Rml::ScrollBehavior::Instant, Rml::ScrollParentage::Closest));
}

void CKeyBindingsWindow::ApplyPending()
{
    const std::vector<PendingCommand> pending = std::exchange(m_pending, {});
    for (const PendingCommand& command : pending)
    {
        switch (command.command)
        {
        case Command::Close:
            Close();
            return;
        case Command::Capture:
            StartCapture(command.action);
            break;
        case Command::Reset:
            StopCapture();
            m_flashing = -1;
            KeyBindings::Runtime::ResetToDefaults();
            PlayBuffer(SOUND_CLICK01);
            break;
        }
    }
}

bool CKeyBindingsWindow::Render()
{
    // Nothing native: the whole window is RmlUi. Kept because CObject requires it.
    return true;
}

void CKeyBindingsWindow::Queue(Command command, int action)
{
    m_pending.push_back({command, action});
}

void CKeyBindingsWindow::StartCapture(int action)
{
    if (!IsValidAction(action))
        return;
    // A second click on the waiting row stops the wait.
    m_capturing = m_capturing == action ? -1 : action;
    m_refused = false;
    PlayBuffer(SOUND_CLICK01);
}

void CKeyBindingsWindow::StopCapture()
{
    m_capturing = -1;
    m_refused = false;
}

void CKeyBindingsWindow::Close()
{
    g_pNewUISystem->Hide(INTERFACE_KEY_BINDINGS);
    PlayBuffer(SOUND_CLICK01);
}

void CKeyBindingsWindow::BindRmlModel(Rml::DataModelConstructor& c, KeyBindingsRmlModel& model)
{
    BindStructs(c);
    c.Bind("has_title", &model.hasTitle);
    c.Bind("title", &model.title);
    c.Bind("positioned", &model.positioned);
    c.Bind("root_x", &model.rootX);
    c.Bind("root_y", &model.rootY);
    c.Bind("labels", &model.labels);
    c.Bind("windows", &model.windows);
    c.Bind("potions", &model.potions);
    c.Bind("skills", &model.skills);
    c.Bind("hint", &model.hint);
    c.Bind("refused", &model.refused);

    const auto queue = [this](Command command)
    {
        return [this, command](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
        { Queue(command, FirstArg(args)); };
    };
    c.BindEventCallback("key_bindings_close", queue(Command::Close));
    c.BindEventCallback("key_bindings_capture", queue(Command::Capture));
    c.BindEventCallback("key_bindings_reset", queue(Command::Reset));
}

void CKeyBindingsWindow::SyncRmlModel()
{
    m_RmlView.Ensure();
    Rml::ElementDocument* document = m_RmlView.Document();
    if (!document)
        return;

    UI::RmlBridge::SyncDocumentVisibilityInFront(document, IsVisible());
    if (!IsVisible())
        return;

    SyncContent();
}

void CKeyBindingsWindow::SyncContent()
{
    const ShownState shown{KeyBindings::Runtime::Revision(), m_capturing, m_flashing, m_refused,
                           I18N::Game::KeyBindingsTitle, m_RmlView.Document()};
    if (m_shown == shown)
        return;
    m_shown = shown;

    const KeyBindings::Table& table = KeyBindings::Runtime::Current();
    KeyBindingsRmlModel updated;
    updated.title = Narrow(I18N::Game::KeyBindingsTitle);
    updated.labels = MakeLabels();

    for (const KeyBindings::ActionInfo& info : KeyBindings::Actions())
    {
        const int index = static_cast<int>(info.action);
        const bool capturing = index == m_capturing;
        const Rml::String keyText = capturing ? Rml::String(WaitingKeyText) : KeyBindings::KeyName(table.KeyOf(info.action));
        GroupRows(updated, info.group).push_back({ActionLabel(info), keyText, index, RowId(index), capturing,
                                                  index == m_flashing});
    }

    const wchar_t* hint = I18N::Game::KeyBindingsHint;
    if (m_capturing >= 0)
        hint = m_refused ? I18N::Game::KeyBindingsFixed : I18N::Game::KeyBindingsPress;
    updated.hint = Narrow(hint);
    updated.refused = m_capturing >= 0 && m_refused;

    auto& binder = m_RmlView.Binder();
    SyncFieldFrom(binder, &KeyBindingsRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &KeyBindingsRmlModel::labels, "labels", updated);
    SyncFieldFrom(binder, &KeyBindingsRmlModel::windows, "windows", updated);
    SyncFieldFrom(binder, &KeyBindingsRmlModel::potions, "potions", updated);
    SyncFieldFrom(binder, &KeyBindingsRmlModel::skills, "skills", updated);
    SyncFieldFrom(binder, &KeyBindingsRmlModel::hint, "hint", updated);
    SyncFieldFrom(binder, &KeyBindingsRmlModel::refused, "refused", updated);
}
