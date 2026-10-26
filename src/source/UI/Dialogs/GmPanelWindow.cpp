#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Dialogs/GmPanelWindow.h"

#include "Audio/DSPlaySound.h"
#include "Data/GameConfig/GameConfig.h"
#include "Data/GameData/ItemData/ItemDatabase.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Commands/GmPanel.h"
#include "GameLogic/Items/CSItemOption.h"
#include "Network/Server/SocketSystem.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Dialogs/GenericMenuDialog.h"
#include "World/MapInfra/MapManager.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Text/CUIRenderText.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlElementBox.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/RmlBridge/RmlNumericInputFilter.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlSyncField.h"

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>

#include "UI/Inventory/ItemMng.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>
#include <map>
#include <span>
#include <tuple>
#include <utility>

using namespace SEASON3B;
using namespace mu::ui::window;
using GameLogic::GmPanel::Panel;

namespace
{
// While the players tab shows who is in game, it asks again this often.
constexpr auto PlayersRefresh = std::chrono::seconds(10);
// While the Events tab shows the events or the Tier Map runs, it asks again this often; after an action on one,
// a moment later (a forced start takes the server up to two ticks of a second: prepared, then started).
constexpr auto EventsRefresh = std::chrono::seconds(5);
constexpr auto AfterEventAction = std::chrono::milliseconds(2500);
// Without an answer this long after asking, the server doesn't send it.
constexpr auto AnswerTimeout = std::chrono::seconds(5);
// The longest texts the server gets, in characters: the chat packet holds about 240 bytes, Cyrillic takes two.
constexpr int MaxReason = 60;
constexpr int MaxNotice = 100;
constexpr int MaxSearch = 10;
constexpr int MaxMonsterSearch = 20;
// The most monsters one spawn makes, and the most a monster search answers with (the add-on's GmPanelEvents).
constexpr int MaxSpawn = 20;
constexpr size_t MonsterLimit = 30;
// The Items tab: the most items the catalog lists, made at a time (the add-on's GmPanelItems) and saved; the
// shortest reason the server takes for an item; the longest name search.
constexpr size_t MaxCatalogRows = 150;
constexpr int MaxItemCount = 20;
constexpr size_t MaxSavedItems = 40;
constexpr size_t MinItemReason = 3;
constexpr int MaxItemSearch = 20;
constexpr int ItemGroups = 16;
// The Character tab: the worn slots and where the personal store starts (OpenMU's inventory), the highest PK state
// (OpenMU's HeroState), and how long a request's chat line may be in bytes (UTF-8: a Cyrillic letter takes two).
constexpr int WornSlots = 12;
constexpr int FirstStoreSlot = 204;
constexpr int MaxPkState = 6;
constexpr size_t MaxLineBytes = 232;

int MeasureInReferenceUnits(const std::wstring& text)
{
    return g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size())).cx;
}

// 21450 -> "21,450".
std::wstring Thousands(long long value)
{
    std::wstring digits = std::to_wstring(std::abs(value));
    for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
        digits.insert(static_cast<size_t>(i), L",");
    return value < 0 ? L"-" + digits : digits;
}

std::wstring Printf(const wchar_t* format, ...)
{
    wchar_t text[512] = {};
    va_list args;
    va_start(args, format);
    std::vswprintf(text, std::size(text), format, args);
    va_end(args);
    return text;
}

// A line of the window's own text, at the size the native renderer would have shrunk it to for its box.
GmPanelLine Line(const std::wstring& text, int width, bool bold = false)
{
    const auto role = bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal;
    // An empty line keeps its font size, so it keeps its height and the lines under it stay put.
    if (text.empty())
        return {{}, UI::RmlBridge::NativeTextPx(role)};
    g_pRenderText->SetFont(bold ? g_hFontBold : g_hFont);
    return {StringUtils::WideToNarrow(text.c_str()),
            UI::RmlBridge::NativeTextPxInBox(role, static_cast<float>(MeasureInReferenceUnits(text)), static_cast<float>(width))};
}

// The same, cut short with "..." when even the smallest size would overflow the box (a long option's text).
GmPanelLine ClippedLine(const std::wstring& text, int width, bool bold = false)
{
    GmPanelLine line = Line(text, width, bold);
    const float normal = UI::RmlBridge::NativeTextPx(bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal);
    if (text.empty() || normal <= 0.0f)
        return line;
    const float ratio = line.textPx / normal;
    if (static_cast<float>(MeasureInReferenceUnits(text)) * ratio <= static_cast<float>(width))
        return line;
    std::wstring cut = text;
    while (cut.size() > 1 && static_cast<float>(MeasureInReferenceUnits(cut + L"...")) * ratio > static_cast<float>(width))
        cut.pop_back();
    while (!cut.empty() && cut.back() == L' ')
        cut.pop_back();
    line.text = StringUtils::WideToNarrow((cut + L"...").c_str());
    return line;
}

// A text that wraps in its box instead of shrinking (the server's answers).
GmPanelLine WrappedLine(const std::wstring& text)
{
    if (text.empty())
        return {};
    return {StringUtils::WideToNarrow(text.c_str()), UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Normal)};
}

GmPanelButton Button(int id, const std::wstring& label, int width, bool on = false, bool off = false)
{
    GmPanelButton button;
    button.id = id;
    button.label = Line(label, width - 4);
    button.on = on;
    button.off = off;
    return button;
}

std::wstring MapName(int map)
{
    const wchar_t* name = map >= 0 ? gMapManager.GetMapName(map) : nullptr;
    return name != nullptr && *name != L'\0' ? std::wstring(name) : std::to_wstring(map);
}

int ToInt(const std::wstring& text)
{
    return static_cast<int>(std::wcstol(text.c_str(), nullptr, 10));
}

// The server's UTC "2026-10-24 15:00" on the player's own clock, "24.10 18:00"; "forever" and unknown text stay.
std::wstring LocalTime(const std::wstring& utc)
{
    if (utc.size() != 16 || utc[4] != L'-' || utc[7] != L'-' || utc[10] != L' ' || utc[13] != L':')
        return utc;

    using namespace std::chrono;
    const int y = ToInt(utc.substr(0, 4));
    const auto mo = static_cast<unsigned>(ToInt(utc.substr(5, 2)));
    const auto d = static_cast<unsigned>(ToInt(utc.substr(8, 2)));
    const year_month_day date{year{y}, month{mo}, day{d}};
    if (!date.ok())
        return utc;

    const auto at = sys_days{date} + hours{ToInt(utc.substr(11, 2))} + minutes{ToInt(utc.substr(14, 2))};
    const std::time_t time = system_clock::to_time_t(at);
    std::tm local = {};
    localtime_s(&local, &time);

    wchar_t text[32] = {};
    mu_swprintf(text, L"%02d.%02d %02d:%02d", local.tm_mday, local.tm_mon + 1, local.tm_hour, local.tm_min);
    return text;
}

// A typed text as one line for the server: control characters become spaces, the ends are trimmed, at most `max`.
std::wstring OneLine(std::wstring text, size_t max)
{
    for (auto& c : text)
    {
        if (std::iswcntrl(c))
            c = L' ';
    }
    const size_t first = text.find_first_not_of(L' ');
    if (first == std::wstring::npos)
        return {};
    text = text.substr(first, text.find_last_not_of(L' ') - first + 1);
    if (text.size() > max)
        text.resize(max);
    return text;
}

bool SameName(const std::wstring& a, const std::wstring& b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](wchar_t x, wchar_t y)
                                               { return std::towlower(x) == std::towlower(y); });
}

const GameLogic::GmPanel::OnlinePlayer* FindOnline(const std::wstring& name)
{
    const auto& rows = Panel().Players().Rows;
    const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& p) { return SameName(p.Name, name); });
    return found == rows.end() ? nullptr : &*found;
}

std::wstring Joined(const std::vector<std::wstring>& parts, const wchar_t* separator)
{
    std::wstring text;
    for (const auto& part : parts)
    {
        if (part.empty())
            continue;
        if (!text.empty())
            text += separator;
        text += part;
    }
    return text;
}

// 75 -> "1:15", 3725 -> "1:02:05".
std::wstring Clock(int seconds)
{
    seconds = std::max(0, seconds);
    wchar_t text[32] = {};
    if (seconds >= 3600)
        mu_swprintf(text, L"%d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60);
    else
        mu_swprintf(text, L"%d:%02d", seconds / 60, seconds % 60);
    return text;
}

// A Tier Map run that is over: only its loot minute is left.
bool TierEnded(const std::wstring& state)
{
    return state.rfind(L"Ended", 0) == 0 || state.rfind(L"Disposed", 0) == 0;
}

// The run's state as the server names it (OpenMU's mini game states, " held" when a GM paused it), in words.
std::wstring TierStateText(const std::wstring& state)
{
    const bool held = state.size() > 5 && state.compare(state.size() - 5, 5, L" held") == 0;
    const std::wstring name = held ? state.substr(0, state.size() - 5) : state;
    std::wstring text = name;
    if (name == L"Open")
        text = I18N::Game::GmPanelTierOpen;
    else if (name == L"Closed")
        text = I18N::Game::GmPanelTierClosed;
    else if (name == L"Playing")
        text = I18N::Game::GmPanelTierPlaying;
    else if (TierEnded(name))
        text = I18N::Game::GmPanelTierEnded;
    return held ? Printf(I18N::Game::GmPanelTierHeld, text.c_str()) : text;
}

// A list's line: how many, that it is empty, or that the answer is awaited (or didn't come).
GmPanelLine ListLine(bool received, size_t count, const wchar_t* countFormat, const wchar_t* empty, bool waiting, int width)
{
    if (!received)
        return Line(waiting ? I18N::Game::GmPanelAsking : I18N::Game::GmPanelNoAnswer, width);
    if (count == 0)
        return Line(empty, width);
    return Line(Printf(countFormat, static_cast<int>(count)), width);
}

// "Ban forever", "Ban till 24.10 18:00", "Mute till ...".
std::wstring PenaltyText(const std::wstring& kind, const std::wstring& until)
{
    if (kind == L"mute")
        return Printf(I18N::Game::GmPanelKindMute, LocalTime(until).c_str());
    if (kind == L"tban")
        return Printf(I18N::Game::GmPanelKindTimedBan, LocalTime(until).c_str());
    return I18N::Game::GmPanelKindBan;
}

// Asks for the reason of a mute or ban, then sends it ("/gm mute <name> <minutes> [reason]").
void ConfirmPenalty(const std::wstring& op, const std::wstring& name, int minutes, const std::wstring& duration)
{
    const bool ban = op == L"ban";
    GenericDialogConfig cfg;
    cfg.showCancel = true;
    cfg.lines = {
        {Printf(ban ? I18N::Game::GmPanelBanConfirm : I18N::Game::GmPanelMuteConfirm, name.c_str(), duration.c_str()), true},
        {ban ? I18N::Game::GmPanelReasonRequired : I18N::Game::GmPanelReasonOptional, false},
    };
    cfg.input = GenericDialogConfig::InputField{};
    cfg.input->mode = GenericDialogConfig::InputField::Mode::Text;
    cfg.input->maxLength = MaxReason;
    cfg.onPrimary = [op, name, minutes, ban]
    {
        const std::wstring reason = OneLine(g_pGenericConfirmDialog->GetInputText(), MaxReason);
        if (ban && reason.empty())
        {
            g_pSystemLogBox->AddText(I18N::Game::GmPanelNeedReason, mu::ui::window::TYPE_ERROR_MESSAGE);
            g_pGenericConfirmDialog->KeepOpen();
            return;
        }
        Panel().Send(op + L" " + name + L" " + std::to_wstring(minutes) + (reason.empty() ? L"" : L" " + reason));
    };
    cfg.tag = "gm_panel_" + StringUtils::WideToNarrow(op.c_str());
    g_pGenericConfirmDialog->Show(std::move(cfg));
}

// A yes/no question; yes sends the request.
void ConfirmRequest(const std::wstring& question, const std::wstring& request, const char* tag)
{
    GenericDialogConfig cfg;
    cfg.showCancel = true;
    cfg.lines = {{question, true}};
    cfg.onPrimary = [request] { Panel().Send(request); };
    cfg.tag = tag;
    g_pGenericConfirmDialog->Show(std::move(cfg));
}

// The longest reason, in letters, that still fits the request's chat line (as if every letter took two bytes).
int ReasonRoom(const std::wstring& request)
{
    const size_t used = StringUtils::WideToNarrow((L"/gm " + request + L" ").c_str()).size();
    return used >= MaxLineBytes ? 0 : std::min(MaxReason, static_cast<int>((MaxLineBytes - used) / 2));
}

// Asks for a reason (at least `minReason` letters; 0: it may stay empty), then sends the request with it. The item,
// if any, is shown in the dialog.
void ConfirmWithReason(std::vector<GenericDialogConfig::Line> lines, const std::wstring& request, size_t minReason, int maxReason,
                       const char* tag, const ITEM* item = nullptr)
{
    GenericDialogConfig cfg;
    cfg.showCancel = true;
    cfg.lines = std::move(lines);
    cfg.input = GenericDialogConfig::InputField{};
    cfg.input->mode = GenericDialogConfig::InputField::Mode::Text;
    cfg.input->maxLength = maxReason;
    if (item != nullptr)
        cfg.item3D = *item;
    cfg.onPrimary = [request, minReason, maxReason]
    {
        const std::wstring reason = OneLine(g_pGenericConfirmDialog->GetInputText(), static_cast<size_t>(std::max(maxReason, 0)));
        if (reason.size() < minReason)
        {
            g_pSystemLogBox->AddText(I18N::Game::GmPanelItemNeedReason, mu::ui::window::TYPE_ERROR_MESSAGE);
            g_pGenericConfirmDialog->KeepOpen();
            return;
        }
        Panel().Send(reason.empty() ? request : request + L" " + reason);
    };
    cfg.tag = tag;
    g_pGenericConfirmDialog->Show(std::move(cfg));
}
} // namespace

mu::ui::window::CGmPanelWindow::CGmPanelWindow() = default;

mu::ui::window::CGmPanelWindow::~CGmPanelWindow()
{
    Release();
}

bool mu::ui::window::CGmPanelWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_GM_PANEL, this);
    // The coordinate fields take digits only; a theme switch builds the document again.
    m_RmlView.SetAfterBuild([this] { UI::RmlBridge::AttachNumericInputFilter(m_RmlView.Document()); });
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void mu::ui::window::CGmPanelWindow::Release()
{
    m_itemTarget.Disable();
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float mu::ui::window::CGmPanelWindow::GetLayerDepth()
{
    return LayerDepth;
}

float mu::ui::window::CGmPanelWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CGmPanelWindow::OpenningProcess()
{
    m_seenResult = Panel().ResultVersion();
    m_status.clear();
    m_statusBad = false;
    // The invisibility and GM mode may have changed since (OpenMU's own commands).
    Panel().Send(L"hello");
    // The Events, Items and Character tabs are the admins'; the role may have changed since.
    if (m_tab >= TAB_EVENTS && !Panel().IsAdmin())
        m_tab = TAB_PLAYERS;
    RequestCurrent();
    m_focusSearchPending = FocusField() != nullptr;
}

void mu::ui::window::CGmPanelWindow::ClosingProcess()
{
    m_focusSearchPending = false;
    if (Rml::ElementDocument* document = m_RmlView.Document())
    {
        if (Rml::Element* focused = document->GetFocusLeafNode(); focused != nullptr && focused != document)
            focused->Blur();
    }
}

bool mu::ui::window::CGmPanelWindow::UpdateMouseEvent()
{
    // Everything in the window is RmlUi's (see Update()); a click on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool mu::ui::window::CGmPanelWindow::UpdateKeyEvent()
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_GM_PANEL))
        return true;

    if (IsPress(VK_ESCAPE))
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_GM_PANEL);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    // Enter in a field: the search, the teleport to the typed coordinates, a spawn, ...
    Rml::ElementDocument* document = m_RmlView.Document();
    const bool typing = document != nullptr && RmlUiRuntime::Instance().GetKeyOwningDocument() == document;
    if (typing && IsPress(VK_RETURN))
    {
        Rml::Element* focused = document->GetFocusLeafNode();
        TakeEnter(focused != nullptr ? focused->GetId() : Rml::String());
        return false;
    }

    return true;
}

void mu::ui::window::CGmPanelWindow::TakeEnter(const Rml::String& fieldId)
{
    if (m_tab == TAB_PLAYERS && m_mode == MODE_SEARCH)
        m_pendingFind = true;
    else if (m_tab == TAB_WORLD)
        m_pendingGo = true;
    else if (m_tab == TAB_EVENTS && fieldId == "gm_monster")
        m_pendingFind = true;
    else if (m_tab == TAB_EVENTS && fieldId == "gm_count")
        m_pendingAction = EV_SPAWN;
    else if (m_tab == TAB_EVENTS && fieldId == "gm_entries")
        m_pendingAction = EV_ENTRIES;
    else if (m_tab == TAB_ITEMS && fieldId == "gm_item_search")
        m_pendingFind = true;
    else if (m_tab == TAB_CHARACTER && fieldId == "gm_char_name")
        m_pendingFind = true;
    else if (m_tab == TAB_CHARACTER && fieldId.rfind("gm_ch_", 0) == 0)
        m_pendingAction = CH_SAVE;
}

const char* mu::ui::window::CGmPanelWindow::FocusField() const
{
    if (m_tab == TAB_PLAYERS && m_mode == MODE_SEARCH)
        return "gm_search";
    if (m_tab == TAB_EVENTS && m_evMode == EV_MONSTERS)
        return "gm_monster";
    if (m_tab == TAB_ITEMS && m_itMode == IT_CATALOG)
        return "gm_item_search";
    if (m_tab == TAB_CHARACTER && m_charName.empty())
        return "gm_char_name";
    return nullptr;
}

int mu::ui::window::CGmPanelWindow::SecondsLeft(int left, std::chrono::steady_clock::time_point receivedAt) const
{
    if (left < 0)
        return -1;
    const auto since = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - receivedAt).count();
    return std::max(0, left - static_cast<int>(since));
}

void mu::ui::window::CGmPanelWindow::NoteLists()
{
    auto& panel = Panel();
    const auto now = std::chrono::steady_clock::now();
    if (panel.Events().Version != m_eventsVersion)
    {
        m_eventsVersion = panel.Events().Version;
        m_eventsAt = now;
    }
    if (panel.Tiers().Version != m_tiersVersion)
    {
        m_tiersVersion = panel.Tiers().Version;
        m_tiersAt = now;
    }
}

void mu::ui::window::CGmPanelWindow::Send(const std::wstring& request)
{
    Panel().Send(request);
    m_status = Printf(I18N::Game::GmPanelSending, request.c_str());
    m_statusBad = false;
}

void mu::ui::window::CGmPanelWindow::RequestCurrent()
{
    auto& panel = Panel();
    m_requestedAt = std::chrono::steady_clock::now();
    if (m_tab == TAB_PLAYERS)
    {
        if (m_mode == MODE_ONLINE)
        {
            panel.Send(L"players");
            m_playersAt = m_requestedAt;
        }
        else if (m_mode == MODE_SEARCH)
        {
            if (m_lastSearch.size() >= 2)
                panel.Send(L"find " + m_lastSearch);
        }
        else
        {
            panel.Send(L"bans");
        }
    }
    else if (m_tab == TAB_WORLD)
    {
        if (!panel.Warps().Received)
            panel.Send(L"maps");
    }
    else if (m_tab == TAB_LOG)
    {
        panel.Send(L"log");
    }
    else if (m_tab == TAB_ITEMS)
    {
        // What the item being made can have, unless the server said it already (its answer doesn't change).
        if (m_itMode == IT_MAKE && m_draft.Picked() && !panel.Item().Is(m_draft.Group, m_draft.Number))
            panel.Send(L"iteminfo " + std::to_wstring(m_draft.Group) + L" " + std::to_wstring(m_draft.Number));
    }
    else if (m_tab == TAB_CHARACTER)
    {
        // The places for the move step; the opened character's values (its two lines in either mode), its items.
        if (!panel.Warps().Received)
            panel.Send(L"maps");
        if (!m_charName.empty())
        {
            panel.Send(L"char " + m_charName);
            if (m_chMode == CH_ITEMS)
                panel.Send(L"inv " + m_charName);
        }
    }
    else
    {
        m_evAskAt = m_requestedAt;
        if (m_evMode == EV_EVENTS)
            panel.Send(L"events");
        else if (m_evMode == EV_TIERS)
            panel.Send(L"tiers");
        else if (!m_lastMonsterSearch.empty())
            panel.Send(L"monsters " + m_lastMonsterSearch);
    }
}

void mu::ui::window::CGmPanelWindow::SwitchTab(int tab)
{
    if (tab < TAB_PLAYERS || tab > (Panel().IsAdmin() ? TAB_CHARACTER : TAB_LOG))
        return;
    m_tab = tab;
    // The Character tab opens with the player picked on the Players tab, until a name is typed there.
    if (tab == TAB_CHARACTER && m_charName.empty() && !m_selected.empty() && m_RmlView.GetModel().charNameText.empty())
    {
        SyncField(m_RmlView.Binder(), &GmPanelRmlModel::charNameText, "char_name_text", StringUtils::WideToNarrow(m_selected.c_str()));
        OpenCharacter();
    }
    else
    {
        RequestCurrent();
    }
    m_focusSearchPending = FocusField() != nullptr;
}

void mu::ui::window::CGmPanelWindow::SwitchMode(int mode)
{
    if (m_tab == TAB_EVENTS)
    {
        if (mode < EV_EVENTS || mode > EV_TIERS)
            return;
        m_evMode = mode;
    }
    else if (m_tab == TAB_ITEMS)
    {
        if (mode < IT_CATALOG || mode > IT_SAVED)
            return;
        m_itMode = mode;
    }
    else if (m_tab == TAB_CHARACTER)
    {
        if (mode < CH_EDITOR || mode > CH_ITEMS)
            return;
        m_chMode = mode;
    }
    else
    {
        if (mode < MODE_ONLINE || mode > MODE_BANS)
            return;
        m_mode = mode;
    }
    RequestCurrent();
    m_focusSearchPending = FocusField() != nullptr;
}

void mu::ui::window::CGmPanelWindow::FindMonsters()
{
    const std::wstring text = OneLine(StringUtils::NarrowToWide(m_RmlView.GetModel().monsterText), MaxMonsterSearch);
    if (text.empty())
    {
        m_status = I18N::Game::GmPanelMonsterHint;
        m_statusBad = true;
        return;
    }

    m_lastMonsterSearch = text;
    m_requestedAt = std::chrono::steady_clock::now();
    Panel().Send(L"monsters " + text);
}

void mu::ui::window::CGmPanelWindow::Search()
{
    const std::wstring text = OneLine(StringUtils::NarrowToWide(m_RmlView.GetModel().searchText), MaxSearch);
    if (text.size() < 2)
    {
        m_status = I18N::Game::GmPanelSearchHint;
        m_statusBad = true;
        return;
    }

    m_lastSearch = text;
    m_requestedAt = std::chrono::steady_clock::now();
    Panel().Send(L"find " + text);
}

void mu::ui::window::CGmPanelWindow::GoToCoordinates()
{
    const auto& model = m_RmlView.GetModel();
    const Rml::String map = UI::RmlBridge::KeepDigitsOnly(model.mapText);
    const Rml::String x = UI::RmlBridge::KeepDigitsOnly(model.xText);
    const Rml::String y = UI::RmlBridge::KeepDigitsOnly(model.yText);
    if (map.empty() || x.empty() || y.empty())
        return;

    Send(L"warp " + StringUtils::NarrowToWide(map) + L" " + StringUtils::NarrowToWide(x) + L" " + StringUtils::NarrowToWide(y));
}

mu::ui::window::CGmPanelWindow::Target mu::ui::window::CGmPanelWindow::GetTarget() const
{
    Target target;
    target.Name = m_selected;
    if (m_selected.empty())
        return target;

    auto& panel = Panel();
    if (const auto* online = FindOnline(m_selected))
    {
        target.Known = true;
        target.Online = true;
        target.Muted = online->Has(L'm');
    }

    if (m_mode == MODE_SEARCH)
    {
        for (const auto& c : panel.Found().Rows)
        {
            if (!SameName(c.Name, m_selected))
                continue;
            target.Known = true;
            target.Online = target.Online || c.Online;
            target.Muted = target.Muted || !c.MutedUntil.empty();
            target.Banned = c.State == L"ban" || c.State == L"tban";
        }
    }
    else if (m_mode == MODE_BANS)
    {
        for (const auto& p : panel.Penalties().Rows)
        {
            if (!SameName(p.Name, m_selected))
                continue;
            target.Known = true;
            target.Muted = target.Muted || p.Kind == L"mute";
            target.Banned = target.Banned || p.Kind != L"mute";
        }
    }

    return target;
}

void mu::ui::window::CGmPanelWindow::AskMute(const std::wstring& name)
{
    GenericMenuConfig menu;
    menu.title = Printf(I18N::Game::GmPanelMuteFor, name.c_str());
    const std::pair<const wchar_t*, int> durations[] = {
        {I18N::Game::GmPanel15Minutes, 15}, {I18N::Game::GmPanel1Hour, 60}, {I18N::Game::GmPanel1Day, 1440}, {I18N::Game::GmPanelForever, 0}};
    for (const auto& [label, minutes] : durations)
    {
        GenericMenuConfig::MenuButton button;
        button.label = label;
        button.onClick = [name, minutes = minutes, duration = std::wstring(label)] { ConfirmPenalty(L"mute", name, minutes, duration); };
        menu.buttons.push_back(std::move(button));
    }

    GenericMenuConfig::MenuButton cancel;
    cancel.label = I18N::Game::Cancel;
    cancel.compact = true;
    cancel.dismiss = true;
    menu.buttons.push_back(std::move(cancel));
    g_pGenericMenuDialog->Show(std::move(menu));
}

void mu::ui::window::CGmPanelWindow::AskBan(const std::wstring& name)
{
    GenericMenuConfig menu;
    menu.title = Printf(I18N::Game::GmPanelBanFor, name.c_str());
    const std::pair<const wchar_t*, int> durations[] = {{I18N::Game::GmPanel1Hour, 60},
                                                        {I18N::Game::GmPanel1Day, 1440},
                                                        {I18N::Game::GmPanel7Days, 7 * 1440},
                                                        {I18N::Game::GmPanel30Days, 30 * 1440},
                                                        {I18N::Game::GmPanelForever, 0}};
    for (const auto& [label, minutes] : durations)
    {
        GenericMenuConfig::MenuButton button;
        button.label = label;
        button.onClick = [name, minutes = minutes, duration = std::wstring(label)] { ConfirmPenalty(L"ban", name, minutes, duration); };
        menu.buttons.push_back(std::move(button));
    }

    GenericMenuConfig::MenuButton cancel;
    cancel.label = I18N::Game::Cancel;
    cancel.compact = true;
    cancel.dismiss = true;
    menu.buttons.push_back(std::move(cancel));
    g_pGenericMenuDialog->Show(std::move(menu));
}

void mu::ui::window::CGmPanelWindow::DoAction(int action)
{
    if (action == ACTION_REFRESH)
    {
        RequestCurrent();
        return;
    }

    const Target target = GetTarget();
    if (target.Name.empty())
        return;
    const std::wstring& name = target.Name;

    switch (action)
    {
    case ACTION_GOTO:
        Send(L"goto " + name);
        break;
    case ACTION_SUMMON:
        Send(L"summon " + name);
        break;
    case ACTION_KICK:
    {
        GenericDialogConfig cfg;
        cfg.showCancel = true;
        cfg.lines = {{Printf(I18N::Game::GmPanelKickConfirm, name.c_str()), true}, {I18N::Game::GmPanelReasonOptional, false}};
        cfg.input = GenericDialogConfig::InputField{};
        cfg.input->mode = GenericDialogConfig::InputField::Mode::Text;
        cfg.input->maxLength = MaxReason;
        cfg.onPrimary = [name]
        {
            const std::wstring reason = OneLine(g_pGenericConfirmDialog->GetInputText(), MaxReason);
            Panel().Send(L"kick " + name + (reason.empty() ? L"" : L" " + reason));
        };
        cfg.tag = "gm_panel_kick";
        g_pGenericConfirmDialog->Show(std::move(cfg));
        break;
    }
    case ACTION_MUTE:
        AskMute(name);
        break;
    case ACTION_UNMUTE:
        ConfirmRequest(Printf(I18N::Game::GmPanelUnmuteConfirm, name.c_str()), L"unmute " + name, "gm_panel_unmute");
        break;
    case ACTION_BAN:
        AskBan(name);
        break;
    case ACTION_UNBAN:
        ConfirmRequest(Printf(I18N::Game::GmPanelUnbanConfirm, name.c_str()), L"unban " + name, "gm_panel_unban");
        break;
    default:
        break;
    }
}

void mu::ui::window::CGmPanelWindow::DoSwitch(int id)
{
    auto& panel = Panel();
    switch (id)
    {
    case SWITCH_HIDE:
        Send(panel.Hidden() ? L"hide off" : L"hide on");
        break;
    case SWITCH_GM_MODE:
        if (panel.IsAdmin())
            Send(panel.GmMode() ? L"gmmode off" : L"gmmode on");
        break;
    case SWITCH_FIREWORKS:
        Send(L"fireworks");
        break;
    case SWITCH_NOTICE:
    {
        GenericDialogConfig cfg;
        cfg.showCancel = true;
        cfg.lines = {{I18N::Game::GmPanelNoticePrompt, true}};
        cfg.input = GenericDialogConfig::InputField{};
        cfg.input->mode = GenericDialogConfig::InputField::Mode::Text;
        cfg.input->maxLength = MaxNotice;
        cfg.onPrimary = []
        {
            const std::wstring text = OneLine(g_pGenericConfirmDialog->GetInputText(), MaxNotice);
            if (text.empty())
            {
                g_pGenericConfirmDialog->KeepOpen();
                return;
            }
            Panel().Send(L"notice " + text);
        };
        cfg.tag = "gm_panel_notice";
        g_pGenericConfirmDialog->Show(std::move(cfg));
        break;
    }
    default:
        break;
    }
}

void mu::ui::window::CGmPanelWindow::DoEventsAction(int action)
{
    auto& panel = Panel();
    if (!panel.IsAdmin())
        return;

    const auto& events = panel.Events().Rows;
    const auto event = std::find_if(events.begin(), events.end(), [&](const auto& e) { return e.Key == m_evSelected; });
    const auto& runs = panel.Tiers().Rows;
    const auto run = std::find_if(runs.begin(), runs.end(), [&](const auto& r) { return SameName(r.Leader, m_tierSelected); });

    switch (action)
    {
    case EV_REFRESH:
        RequestCurrent();
        break;
    case EV_START:
        if (event != events.end() && event->Can(L's'))
        {
            // A mini game that runs ends first (as /startbc).
            const bool running = event->State == L"open" || event->State == L"run";
            ConfirmRequest(Printf(running ? I18N::Game::GmPanelEventRestartConfirm : I18N::Game::GmPanelEventStartConfirm, event->Name.c_str()),
                           L"event start " + event->Key, "gm_panel_event_start");
        }
        break;
    case EV_SKIP:
        if (event != events.end() && event->Can(L'k'))
            Send(L"event skip " + event->Key);
        break;
    case EV_END:
        if (event != events.end() && event->Can(L'e'))
            ConfirmRequest(Printf(I18N::Game::GmPanelEventEndConfirm, event->Name.c_str()), L"event end " + event->Key, "gm_panel_event_end");
        break;
    case EV_SPAWN:
    {
        const auto& monsters = panel.Monsters().Rows;
        if (std::none_of(monsters.begin(), monsters.end(), [&](const auto& m) { return m.Number == m_monsterSelected; }))
            break;
        const Rml::String digits = UI::RmlBridge::KeepDigitsOnly(m_RmlView.GetModel().countText);
        const int count = std::clamp(digits.empty() ? 1 : std::atoi(digits.c_str()), 1, MaxSpawn);
        Send(L"spawn " + std::to_wstring(m_monsterSelected) + L" " + std::to_wstring(count));
        break;
    }
    case EV_CLEAR:
        ConfirmRequest(I18N::Game::GmPanelClearConfirm, L"clear", "gm_panel_clear");
        break;
    case EV_TIER_END:
        if (run != runs.end())
            ConfirmRequest(Printf(I18N::Game::GmPanelTierEndConfirm, run->Tier, run->Leader.c_str()), L"tier end " + run->Leader,
                           "gm_panel_tier_end");
        break;
    case EV_TIER_SKIP:
        if (run != runs.end())
            Send(L"tier skip " + run->Leader);
        break;
    case EV_ENTRIES:
    {
        const std::wstring name = OneLine(StringUtils::NarrowToWide(m_RmlView.GetModel().entriesText), MaxSearch);
        if (name.empty())
        {
            m_status = I18N::Game::GmPanelEntriesNeedName;
            m_statusBad = true;
            break;
        }
        ConfirmRequest(Printf(I18N::Game::GmPanelEntriesConfirm, name.c_str()), L"entries " + name + L" 0", "gm_panel_entries");
        break;
    }
    default:
        break;
    }
}

void mu::ui::window::CGmPanelWindow::TakeResult()
{
    auto& panel = Panel();
    if (panel.ResultVersion() == m_seenResult)
        return;
    m_seenResult = panel.ResultVersion();

    const auto& result = panel.LastResult();
    m_status = result.Text;
    m_statusBad = !result.Ok;
    if (!result.Ok)
        return;

    // What changed shows in the lists and switches: ask again.
    const std::wstring& op = result.Op;
    if (op == L"hide" || op == L"gmmode")
    {
        panel.Send(L"hello");
    }
    else if (op == L"kick" || op == L"mute" || op == L"unmute" || op == L"ban" || op == L"unban" || op == L"summon")
    {
        if (m_tab == TAB_PLAYERS)
            RequestCurrent();
    }
    else if (op == L"event" || op == L"tier")
    {
        // The server changes it at its next tick: the list is asked for again a moment later.
        if (m_tab == TAB_EVENTS)
            m_evAskAt = std::chrono::steady_clock::now() - EventsRefresh + AfterEventAction;
    }
    else if (op == L"edit" || op == L"itemdel" || op == L"undo")
    {
        // The character's values and items, or the log, as they are now; the Log tab undoes an edit or a deletion.
        if (op != L"undo")
            m_status = Joined({result.Text, std::wstring(I18N::Game::GmPanelInvUndoHint)}, L" ");
        if (m_tab == TAB_CHARACTER || m_tab == TAB_LOG)
            RequestCurrent();
    }
}

void mu::ui::window::CGmPanelWindow::PickRow(int row)
{
    auto& panel = Panel();
    const size_t index = static_cast<size_t>(row);
    if (m_tab == TAB_EVENTS)
    {
        if (m_evMode == EV_EVENTS && index < panel.Events().Rows.size())
            m_evSelected = panel.Events().Rows[index].Key;
        else if (m_evMode == EV_MONSTERS && index < panel.Monsters().Rows.size())
            m_monsterSelected = panel.Monsters().Rows[index].Number;
        else if (m_evMode == EV_TIERS && index < panel.Tiers().Rows.size())
            m_tierSelected = panel.Tiers().Rows[index].Leader;
        return;
    }

    if (m_tab == TAB_LOG)
    {
        // An admin picks an entry to undo.
        if (panel.IsAdmin() && index < panel.Log().Rows.size())
            m_logSelected = panel.Log().Rows[index].Id;
        return;
    }

    if (m_tab == TAB_CHARACTER)
    {
        // An item of the list, by where it is (the list may come again in another order).
        const auto& items = panel.Inventory().Items;
        if (m_chMode == CH_ITEMS && m_invSeenVersion == panel.InventoryVersion() && index < items.size() && index < m_invItems.size())
        {
            m_invStorage = items[index].Storage;
            m_invSlot = items[index].Slot;
        }
        return;
    }

    if (m_tab == TAB_ITEMS)
    {
        if (m_itMode == IT_CATALOG && index < m_itTypes.size())
            PickItem(m_itTypes[index] / MAX_ITEM_INDEX, m_itTypes[index] % MAX_ITEM_INDEX);
        else if (m_itMode == IT_MAKE)
            ToggleExtra(row);
        else if (m_itMode == IT_SAVED)
            m_savedSelected = row;
        return;
    }

    if (m_mode == MODE_ONLINE && index < panel.Players().Rows.size())
        m_selected = panel.Players().Rows[index].Name;
    else if (m_mode == MODE_SEARCH && index < panel.Found().Rows.size())
        m_selected = panel.Found().Rows[index].Name;
    else if (m_mode == MODE_BANS && index < panel.Penalties().Rows.size())
        m_selected = panel.Penalties().Rows[index].Name;
}

void mu::ui::window::CGmPanelWindow::HandleClicks()
{
    const int tab = std::exchange(m_pendingTab, -1);
    const int mode = std::exchange(m_pendingMode, -1);
    const int action = std::exchange(m_pendingAction, 0);
    const int switchId = std::exchange(m_pendingSwitch, 0);
    const int row = std::exchange(m_pendingRow, -1);
    const int warp = std::exchange(m_pendingWarp, -1);
    const bool find = std::exchange(m_pendingFind, false);
    const bool go = std::exchange(m_pendingGo, false);
    const bool logRefresh = std::exchange(m_pendingLogRefresh, false);
    const bool exit = std::exchange(m_pendingExit, false);
    if (!IsVisible())
        return;

    if (exit)
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_GM_PANEL);
        PlayBuffer(SOUND_CLICK01);
        return;
    }

    const bool clicked = tab >= 0 || mode >= 0 || action > 0 || switchId > 0 || row >= 0 || warp >= 0 || find || go || logRefresh;
    if (clicked)
        PlayBuffer(SOUND_CLICK01);

    if (tab >= 0 && tab != m_tab)
        SwitchTab(tab);
    if (mode >= 0 && mode != (m_tab == TAB_EVENTS ? m_evMode : m_tab == TAB_ITEMS ? m_itMode : m_tab == TAB_CHARACTER ? m_chMode : m_mode))
        SwitchMode(mode);

    if (row >= 0)
        PickRow(row);

    if (find)
    {
        if (m_tab == TAB_EVENTS)
            FindMonsters();
        else if (m_tab == TAB_ITEMS)
            SearchItems();
        else if (m_tab == TAB_CHARACTER)
            OpenCharacter();
        else
            Search();
    }
    if (go)
        GoToCoordinates();
    if (warp >= 0)
        Send(L"warp " + std::to_wstring(warp));
    if (action > 0)
    {
        if (m_tab == TAB_EVENTS)
            DoEventsAction(action);
        else if (m_tab == TAB_ITEMS)
            DoItemsAction(action);
        else if (m_tab == TAB_CHARACTER)
            DoCharacterAction(action);
        else if (m_tab == TAB_LOG)
            DoLogAction(action);
        else
            DoAction(action);
    }
    if (switchId > 0)
        DoSwitch(switchId);
    if (logRefresh)
        RequestCurrent();
}

bool mu::ui::window::CGmPanelWindow::Update()
{
    auto& panel = Panel();
    if (IsVisible())
    {
        // The character changed, or the server took the rights away.
        if (panel.GetRole() == GameLogic::GmPanel::Role::None)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_GM_PANEL);
            return true;
        }
        if (panel.GetRole() == GameLogic::GmPanel::Role::Unknown)
            panel.Hello(false);
        // An admin who became a moderator: the Events, Items and Character tabs are gone.
        if (m_tab >= TAB_EVENTS && panel.GetRole() == GameLogic::GmPanel::Role::Moderator)
            SwitchTab(TAB_PLAYERS);

        HandleClicks();
        TakeResult();

        const auto now = std::chrono::steady_clock::now();
        if (IsVisible() && m_tab == TAB_PLAYERS && m_mode == MODE_ONLINE && now - m_playersAt >= PlayersRefresh)
        {
            panel.Send(L"players");
            m_playersAt = now;
        }
        // The events and the runs change by themselves (starts, waits, kills).
        if (IsVisible() && m_tab == TAB_EVENTS && m_evMode != EV_MONSTERS && now - m_evAskAt >= EventsRefresh)
        {
            panel.Send(m_evMode == EV_EVENTS ? L"events" : L"tiers");
            m_evAskAt = now;
        }
    }

    NoteLists();
    SyncRmlModel();
    return true;
}

bool mu::ui::window::CGmPanelWindow::Render()
{
    // The frame, the lists and the buttons are RmlUi; the item's icon is drawn by m_itemTarget. Only an item's
    // tooltip is native: drawn every frame while the icon is hovered, or on the Character tab an item's row (it hides
    // itself in a frame it isn't drawn).
    if (!IsVisible())
        return true;

    Rml::Vector2f offset;
    Rml::Vector2f size;
    if (tagITEM* item = ShownItem())
    {
        Rml::Element* icon = GetField(IconId());
        if (icon != nullptr && icon->IsPseudoClassSet("hover") && UI::RmlBridge::DrawnBox(*icon, Rml::BoxArea::Border, offset, size))
        {
            ::RenderItemInfoAtPx(offset.x + size.x / 2.f, offset.y + size.y, item, false);
            return true;
        }
    }

    Rml::ElementDocument* document = m_RmlView.Document();
    if (document == nullptr || m_tab != TAB_CHARACTER || m_chMode != CH_ITEMS || m_invSeenVersion != Panel().InventoryVersion())
        return true;
    Rml::ElementList rows;
    document->QuerySelectorAll(rows, ".gm-inv-row");
    for (Rml::Element* row : rows)
    {
        if (!row->IsPseudoClassSet("hover"))
            continue;
        const int index = row->GetAttribute<int>("gm-row-index", -1);
        if (index >= 0 && index < static_cast<int>(m_invItems.size()) && UI::RmlBridge::DrawnBox(*row, Rml::BoxArea::Border, offset, size))
            ::RenderItemInfoAtPx(offset.x + size.x / 2.f, offset.y + size.y, m_invItems[static_cast<size_t>(index)].get(), false);
        break;
    }
    return true;
}

Rml::Element* mu::ui::window::CGmPanelWindow::GetField(const char* id) const
{
    Rml::ElementDocument* document = m_RmlView.Document();
    return document != nullptr ? document->GetElementById(id) : nullptr;
}

void mu::ui::window::CGmPanelWindow::BindRmlModel(Rml::DataModelConstructor& c, GmPanelRmlModel& model)
{
    auto lineType = c.RegisterStruct<GmPanelLine>();
    lineType.RegisterMember("text", &GmPanelLine::text);
    lineType.RegisterMember("text_px", &GmPanelLine::textPx);

    auto buttonType = c.RegisterStruct<GmPanelButton>();
    buttonType.RegisterMember("id", &GmPanelButton::id);
    buttonType.RegisterMember("label", &GmPanelButton::label);
    buttonType.RegisterMember("on", &GmPanelButton::on);
    buttonType.RegisterMember("off", &GmPanelButton::off);
    c.RegisterArray<std::vector<GmPanelButton>>();

    auto rowType = c.RegisterStruct<GmPanelRow>();
    rowType.RegisterMember("index", &GmPanelRow::index);
    rowType.RegisterMember("title", &GmPanelRow::title);
    rowType.RegisterMember("detail", &GmPanelRow::detail);
    rowType.RegisterMember("selected", &GmPanelRow::selected);
    rowType.RegisterMember("dim", &GmPanelRow::dim);
    rowType.RegisterMember("alert", &GmPanelRow::alert);
    rowType.RegisterMember("live", &GmPanelRow::live);
    c.RegisterArray<std::vector<GmPanelRow>>();

    auto stepType = c.RegisterStruct<GmPanelStep>();
    stepType.RegisterMember("label_id", &GmPanelStep::labelId);
    stepType.RegisterMember("label", &GmPanelStep::label);
    stepType.RegisterMember("value", &GmPanelStep::value);
    stepType.RegisterMember("down_id", &GmPanelStep::downId);
    stepType.RegisterMember("up_id", &GmPanelStep::upId);
    stepType.RegisterMember("down_off", &GmPanelStep::downOff);
    stepType.RegisterMember("up_off", &GmPanelStep::upOff);
    c.RegisterArray<std::vector<GmPanelStep>>();

    auto charLabelsType = c.RegisterStruct<GmPanelCharLabels>();
    charLabelsType.RegisterMember("lv", &GmPanelCharLabels::lv);
    charLabelsType.RegisterMember("rs", &GmPanelCharLabels::rs);
    charLabelsType.RegisterMember("pt", &GmPanelCharLabels::pt);
    charLabelsType.RegisterMember("str", &GmPanelCharLabels::str);
    charLabelsType.RegisterMember("agi", &GmPanelCharLabels::agi);
    charLabelsType.RegisterMember("vit", &GmPanelCharLabels::vit);
    charLabelsType.RegisterMember("ene", &GmPanelCharLabels::ene);
    charLabelsType.RegisterMember("cmd", &GmPanelCharLabels::cmd);
    charLabelsType.RegisterMember("zen", &GmPanelCharLabels::zen);

    c.Bind("title", &model.title);
    c.Bind("tab", &model.tab);
    c.Bind("tabs", &model.tabs);
    c.Bind("field_px", &model.fieldPx);

    c.Bind("mode", &model.mode);
    c.Bind("modes", &model.modes);
    c.Bind("search_text", &model.searchText);
    c.Bind("find", &model.find);
    c.Bind("list_info", &model.listInfo);
    c.Bind("rows", &model.rows);
    c.Bind("sel_name", &model.selName);
    c.Bind("sel_stats", &model.selStats);
    c.Bind("sel_where", &model.selWhere);
    c.Bind("actions", &model.actions);

    c.Bind("warps_title", &model.warpsTitle);
    c.Bind("warps", &model.warps);
    c.Bind("map_label", &model.mapLabel);
    c.Bind("map_text", &model.mapText);
    c.Bind("x_text", &model.xText);
    c.Bind("y_text", &model.yText);
    c.Bind("go", &model.go);
    c.Bind("switches", &model.switches);
    c.Bind("here", &model.here);

    c.Bind("log_refresh", &model.logRefresh);
    c.Bind("log_info", &model.logInfo);
    c.Bind("log", &model.log);

    c.Bind("monster_text", &model.monsterText);
    c.Bind("count_text", &model.countText);
    c.Bind("entries_text", &model.entriesText);
    c.Bind("count_label", &model.countLabel);
    c.Bind("entries_info", &model.entriesInfo);
    c.Bind("entries", &model.entries);
    // One monster a spawn unless the GM types more.
    if (model.countText.empty())
        model.countText = "1";

    c.Bind("steps", &model.steps);
    c.Bind("toggles", &model.toggles);
    c.Bind("item_name", &model.itemName);
    c.Bind("item_line", &model.itemLine);
    c.Bind("item_hint", &model.itemHint);
    c.Bind("item_count_label", &model.itemCountLabel);
    c.Bind("item_dur_label", &model.itemDurLabel);
    c.Bind("item_ready", &model.itemReady);
    c.Bind("item_search_text", &model.itemSearchText);
    c.Bind("item_count_text", &model.itemCountText);
    c.Bind("item_dur_text", &model.itemDurText);
    // One item unless the GM types more; the durability field empty: a whole item.
    if (model.itemCountText.empty())
        model.itemCountText = "1";

    c.Bind("char_name_text", &model.charNameText);
    c.Bind("char_ready", &model.charReady);
    c.Bind("char_has_cmd", &model.charHasCmd);
    c.Bind("char_labels", &model.charLabels);
    c.Bind("ch_lv_text", &model.chLvText);
    c.Bind("ch_rs_text", &model.chRsText);
    c.Bind("ch_pt_text", &model.chPtText);
    c.Bind("ch_str_text", &model.chStrText);
    c.Bind("ch_agi_text", &model.chAgiText);
    c.Bind("ch_vit_text", &model.chVitText);
    c.Bind("ch_ene_text", &model.chEneText);
    c.Bind("ch_cmd_text", &model.chCmdText);
    c.Bind("ch_zen_text", &model.chZenText);

    c.Bind("status", &model.status);
    c.Bind("status_bad", &model.statusBad);
    c.Bind("exit_tooltip", &model.exitTooltip);

    auto intArg = [](const Rml::VariantList& args) { return args.size() == 1 ? args[0].Get<int>(-1) : -1; };
    c.BindEventCallback("gm_tab", [this, intArg](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        { m_pendingTab = intArg(args); });
    c.BindEventCallback("gm_mode", [this, intArg](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        { m_pendingMode = intArg(args); });
    c.BindEventCallback("gm_row", [this, intArg](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        { m_pendingRow = intArg(args); });
    c.BindEventCallback("gm_warp", [this, intArg](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        { m_pendingWarp = intArg(args); });
    c.BindEventCallback("gm_action", [this, intArg](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        { m_pendingAction = std::max(0, intArg(args)); });
    c.BindEventCallback("gm_switch", [this, intArg](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        { m_pendingSwitch = std::max(0, intArg(args)); });
    c.BindEventCallback("gm_find", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_pendingFind = true; });
    c.BindEventCallback("gm_go", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_pendingGo = true; });
    // Enter in a field, as the field sees it: the text input keeps the key from the game's own key state.
    c.BindEventCallback("gm_field_key",
                        [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&)
                        {
                            const auto key = static_cast<Rml::Input::KeyIdentifier>(event.GetParameter<int>("key_identifier", 0));
                            if (key != Rml::Input::KI_RETURN && key != Rml::Input::KI_NUMPADENTER)
                                return;
                            Rml::Element* field = event.GetTargetElement();
                            TakeEnter(field != nullptr ? field->GetId() : Rml::String());
                        });
    c.BindEventCallback("gm_log_refresh", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingLogRefresh = true; });
    c.BindEventCallback("gm_exit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { m_pendingExit = true; });

    wchar_t closeText[256] = {};
    mu_swprintf_s(closeText, I18N::Game::CloseS, L"Esc");
    model.exitTooltip = StringUtils::WideToNarrow(closeText);
}

void mu::ui::window::CGmPanelWindow::SyncRmlModel()
{
    m_RmlView.Ensure();
    // The item's icon (#gm_items over the panel), only while the Items tab's Make page or the Character tab's items
    // show one; its camera fits the item into the icon's box.
    if (IsVisible() && m_tab == TAB_ITEMS && m_itMode == IT_MAKE)
        UpdatePreview();
    if (IsVisible() && m_tab == TAB_CHARACTER)
        TakeInventory();
    const bool showItem = IsVisible() && ShownItem() != nullptr;
    if (showItem)
        m_itemTarget.SetFieldOfView(ItemFieldOfView());
    m_itemTarget.Sync(GetField("gm_items"), showItem);
    if (!m_RmlView.Document())
        return;

    // Layer depth UI::RmlBridge::ForegroundPanelLayerDepth: over the HUD and its logs.
    UI::RmlBridge::SyncDocumentVisibilityInFront(m_RmlView.Document(), IsVisible());
    if (!IsVisible())
        return;

    SyncContent();

    // The search field takes the keys when the search opens (it exists from the next frame on).
    if (m_focusSearchPending)
    {
        const char* id = FocusField();
        if (id == nullptr)
        {
            m_focusSearchPending = false;
        }
        else if (Rml::Element* field = GetField(id))
        {
            field->Focus();
            if (field->IsPseudoClassSet("focus"))
                m_focusSearchPending = false;
        }
    }
}

void mu::ui::window::CGmPanelWindow::SyncPlayers(GmPanelRmlModel& updated)
{
    auto& panel = Panel();
    const auto now = std::chrono::steady_clock::now();
    const int online = panel.Players().Received ? static_cast<int>(panel.Players().Rows.size()) : -1;
    updated.modes = {
        Button(MODE_ONLINE, online >= 0 ? Printf(I18N::Game::GmPanelModeOnline, online) : std::wstring(I18N::Game::GmPanelModeOnlineNone),
               TAB_WIDTH, m_mode == MODE_ONLINE),
        Button(MODE_SEARCH, I18N::Game::GmPanelModeSearch, TAB_WIDTH, m_mode == MODE_SEARCH),
        Button(MODE_BANS, I18N::Game::GmPanelModeBans, TAB_WIDTH, m_mode == MODE_BANS),
    };
    updated.find = Button(0, I18N::Game::GmPanelFind, ACTION_WIDTH);

    // A list's line: how many, or that the answer is awaited.
    auto info = [&](bool received, size_t count, const wchar_t* countFormat, const wchar_t* empty)
    {
        if (!received)
            return Line(now - m_requestedAt < AnswerTimeout ? I18N::Game::GmPanelAsking : I18N::Game::GmPanelNoAnswer, CONTENT_WIDTH);
        if (count == 0)
            return Line(empty, CONTENT_WIDTH);
        return Line(Printf(countFormat, static_cast<int>(count)), CONTENT_WIDTH);
    };

    if (m_mode == MODE_ONLINE)
    {
        const auto& list = panel.Players();
        updated.listInfo = info(list.Received, list.Rows.size(), I18N::Game::GmPanelOnlineCount, I18N::Game::GmPanelEmptyOnline);
        for (size_t i = 0; i < list.Rows.size(); ++i)
        {
            const auto& p = list.Rows[i];
            std::vector<std::wstring> tags;
            if (p.Has(L'g'))
                tags.emplace_back(I18N::Game::GmPanelTagGm);
            if (p.Has(L'h'))
                tags.emplace_back(I18N::Game::GmPanelTagHidden);
            if (p.Has(L'm'))
                tags.emplace_back(I18N::Game::GmPanelTagMuted);
            if (p.Has(L'e'))
                tags.emplace_back(I18N::Game::GmPanelTagEvent);
            if (p.Has(L't'))
                tags.emplace_back(I18N::Game::GmPanelTagTier);

            GmPanelRow row;
            row.index = static_cast<int>(i);
            row.title = Line(Joined({p.Name, Joined(tags, L", ")}, L"  "), ROW_WIDTH, true);
            row.detail = Line(Printf(I18N::Game::GmPanelRowPlayer, p.Level, p.Resets, MapName(p.Map).c_str(), p.X, p.Y), ROW_WIDTH);
            row.selected = SameName(p.Name, m_selected);
            row.alert = p.Has(L'm');
            updated.rows.push_back(std::move(row));
        }
    }
    else if (m_mode == MODE_SEARCH)
    {
        const auto& list = panel.Found();
        updated.listInfo = m_lastSearch.size() < 2
            ? Line(I18N::Game::GmPanelSearchHint, CONTENT_WIDTH)
            : info(list.Received, list.Rows.size(),
                   // The server sends at most FindLimit names: a full list may leave some out.
                   list.Rows.size() >= GameLogic::GmPanel::FindLimit ? I18N::Game::GmPanelFoundMany : I18N::Game::GmPanelFoundCount,
                   I18N::Game::GmPanelEmptyFind);
        for (size_t i = 0; i < list.Rows.size(); ++i)
        {
            const auto& c = list.Rows[i];
            const bool banned = c.State == L"ban" || c.State == L"tban";
            std::vector<std::wstring> tags;
            if (c.Online)
                tags.emplace_back(I18N::Game::GmPanelTagOnline);
            if (c.State == L"gm")
                tags.emplace_back(I18N::Game::GmPanelTagGm);
            if (banned)
                tags.emplace_back(I18N::Game::GmPanelTagBanned);
            if (!c.MutedUntil.empty())
                tags.emplace_back(I18N::Game::GmPanelTagMuted);

            GmPanelRow row;
            row.index = static_cast<int>(i);
            row.title = Line(Joined({c.Name, Joined(tags, L", ")}, L"  "), ROW_WIDTH, true);
            row.detail = Line(Joined({c.ClassName, c.Account}, L"  "), ROW_WIDTH);
            row.selected = SameName(c.Name, m_selected);
            row.dim = !c.Online;
            row.alert = banned || !c.MutedUntil.empty();
            updated.rows.push_back(std::move(row));
        }
    }
    else
    {
        const auto& list = panel.Penalties();
        updated.listInfo = info(list.Received, list.Rows.size(), I18N::Game::GmPanelBansCount, I18N::Game::GmPanelEmptyBans);
        for (size_t i = 0; i < list.Rows.size(); ++i)
        {
            const auto& p = list.Rows[i];
            GmPanelRow row;
            row.index = static_cast<int>(i);
            row.title = Line(p.Account.empty() ? p.Name : p.Name + L" (" + p.Account + L")", ROW_WIDTH, true);
            row.detail = Line(Joined({PenaltyText(p.Kind, p.Until), p.GmName, p.Reason}, L" - "), ROW_WIDTH);
            row.selected = SameName(p.Name, m_selected);
            row.alert = p.Kind != L"mute";
            updated.rows.push_back(std::move(row));
        }
    }

    // The picked character: everything the lists know of it. Three lines always, empty ones too, so the action
    // buttons under them stay put.
    const Target target = GetTarget();
    updated.selStats = Line({}, CONTENT_WIDTH);
    updated.selWhere = Line({}, CONTENT_WIDTH);
    if (target.Name.empty())
    {
        updated.selName = Line(I18N::Game::GmPanelNoneSelected, CONTENT_WIDTH);
    }
    else
    {
        const auto* online = FindOnline(target.Name);
        const GameLogic::GmPanel::FoundCharacter* found = nullptr;
        for (const auto& c : panel.Found().Rows)
        {
            if (m_mode == MODE_SEARCH && SameName(c.Name, target.Name))
                found = &c;
        }

        std::wstring account = online != nullptr ? online->Account : found != nullptr ? found->Account : std::wstring();
        if (account.empty() && m_mode == MODE_BANS)
        {
            for (const auto& p : panel.Penalties().Rows)
            {
                if (SameName(p.Name, target.Name))
                    account = p.Account;
            }
        }
        updated.selName = Line(account.empty() ? target.Name : target.Name + L" (" + account + L")", CONTENT_WIDTH, true);

        if (online != nullptr)
        {
            updated.selStats = Line(Printf(I18N::Game::GmPanelSelectedStats, online->ClassName.c_str(), online->Level, online->Resets,
                                           Thousands(online->CombatPower).c_str()),
                                    CONTENT_WIDTH);
            updated.selWhere = Line(Printf(I18N::Game::GmPanelSelectedWhere, MapName(online->Map).c_str(), online->X, online->Y), CONTENT_WIDTH);
        }
        else
        {
            if (found != nullptr)
                updated.selStats = Line(found->ClassName, CONTENT_WIDTH);
            updated.selWhere = Line(I18N::Game::GmPanelSelectedOffline, CONTENT_WIDTH);
        }

        // Its mute and ban, as far as the list it came from knows them.
        std::vector<std::wstring> penalties;
        if (found != nullptr)
        {
            if (found->BannedUntil == L"forever")
                penalties.emplace_back(I18N::Game::GmPanelBannedForever);
            else if (!found->BannedUntil.empty())
                penalties.push_back(Printf(I18N::Game::GmPanelBannedUntil, LocalTime(found->BannedUntil).c_str()));
            if (!found->MutedUntil.empty())
                penalties.push_back(Printf(I18N::Game::GmPanelMutedUntil, LocalTime(found->MutedUntil).c_str()));
        }
        else if (m_mode == MODE_BANS)
        {
            for (const auto& p : panel.Penalties().Rows)
            {
                if (SameName(p.Name, target.Name))
                    penalties.push_back(PenaltyText(p.Kind, p.Until));
            }
        }
        else if (target.Muted)
        {
            penalties.emplace_back(I18N::Game::GmPanelTagMuted);
        }

        if (!penalties.empty())
        {
            if (online == nullptr && updated.selStats.text.empty())
                updated.selStats = Line(Joined(penalties, L", "), CONTENT_WIDTH);
            else
                updated.selWhere = Line(Joined({StringUtils::NarrowToWide(updated.selWhere.text), Joined(penalties, L", ")}, L", "), CONTENT_WIDTH);
        }
    }

    const bool picked = !target.Name.empty();
    updated.actions = {
        Button(ACTION_GOTO, I18N::Game::GmPanelGoto, ACTION_WIDTH, false, !target.Online),
        Button(ACTION_SUMMON, I18N::Game::GmPanelSummon, ACTION_WIDTH, false, !target.Online),
        Button(ACTION_KICK, I18N::Game::GmPanelKick, ACTION_WIDTH, false, !target.Online),
        Button(ACTION_REFRESH, I18N::Game::GmPanelRefresh, ACTION_WIDTH),
        Button(ACTION_MUTE, I18N::Game::GmPanelMute, ACTION_WIDTH, false, !picked),
        Button(ACTION_UNMUTE, I18N::Game::GmPanelUnmute, ACTION_WIDTH, false, !target.Muted),
        Button(ACTION_BAN, I18N::Game::GmPanelBan, ACTION_WIDTH, false, !picked || target.Banned),
        Button(ACTION_UNBAN, I18N::Game::GmPanelUnban, ACTION_WIDTH, false, !target.Banned),
    };
}

void mu::ui::window::CGmPanelWindow::SyncWorld(GmPanelRmlModel& updated)
{
    auto& panel = Panel();
    const auto now = std::chrono::steady_clock::now();
    const auto& warps = panel.Warps();
    updated.warpsTitle = Line(warps.Received ? std::wstring(I18N::Game::GmPanelWarps)
                              : now - m_requestedAt < AnswerTimeout ? std::wstring(I18N::Game::GmPanelAsking)
                                                                    : std::wstring(I18N::Game::GmPanelNoAnswer),
                              CONTENT_WIDTH, true);
    for (const auto& warp : warps.Rows)
    {
        GmPanelRow row;
        row.index = warp.Index;
        row.title = Line(warp.Name, ROW_WIDTH);
        updated.warps.push_back(std::move(row));
    }

    updated.mapLabel = Line(I18N::Game::GmPanelMapLabel, 28);
    updated.go = Button(0, I18N::Game::GmPanelGo, ACTION_WIDTH);
    updated.switches = {
        Button(SWITCH_HIDE, panel.Hidden() ? I18N::Game::GmPanelHideOn : I18N::Game::GmPanelHideOff, SWITCH_WIDTH, panel.Hidden()),
        Button(SWITCH_GM_MODE, panel.GmMode() ? I18N::Game::GmPanelGmModeOn : I18N::Game::GmPanelGmModeOff, SWITCH_WIDTH, panel.GmMode(),
               !panel.IsAdmin()),
        Button(SWITCH_FIREWORKS, I18N::Game::GmPanelFireworks, SWITCH_WIDTH),
        Button(SWITCH_NOTICE, I18N::Game::GmPanelNotice, SWITCH_WIDTH),
    };

    if (Hero != nullptr)
        updated.here = Line(Printf(I18N::Game::GmPanelHere, MapName(gMapManager.WorldActive).c_str(), static_cast<int>(Hero->PositionX),
                                   static_cast<int>(Hero->PositionY)),
                            CONTENT_WIDTH);
}

void mu::ui::window::CGmPanelWindow::SyncLog(GmPanelRmlModel& updated)
{
    const auto& panel = Panel();
    const auto& log = panel.Log();
    const auto now = std::chrono::steady_clock::now();
    updated.logRefresh = Button(0, I18N::Game::GmPanelRefresh, SWITCH_WIDTH);
    updated.logInfo = !log.Received
        ? Line(now - m_requestedAt < AnswerTimeout ? I18N::Game::GmPanelAsking : I18N::Game::GmPanelNoAnswer, CONTENT_WIDTH)
        : log.Rows.empty() ? Line(I18N::Game::GmPanelEmptyLog, CONTENT_WIDTH)
                           : Line(Printf(I18N::Game::GmPanelLogCount, static_cast<int>(log.Rows.size())), CONTENT_WIDTH);
    const bool admin = panel.IsAdmin();
    const GameLogic::GmPanel::LogEntry* picked = nullptr;
    for (size_t i = 0; i < log.Rows.size(); ++i)
    {
        const auto& e = log.Rows[i];
        GmPanelRow row;
        row.index = static_cast<int>(i);
        // "24.10 23:35 kick Name" over "GmName: reason", so the longer part has the whole second line.
        const std::wstring title = Printf(I18N::Game::GmPanelLogTitle, LocalTime(e.Time).c_str(), e.Op.c_str(), e.Target.c_str());
        row.title = Line(e.Undone() ? Joined({title, std::wstring(I18N::Game::GmPanelLogUndone)}, L"  ") : title, ROW_WIDTH, true);
        row.detail = Line(e.Detail.empty() ? e.GmName : Printf(I18N::Game::GmPanelLogDetail, e.GmName.c_str(), e.Detail.c_str()), ROW_WIDTH);
        row.alert = e.Op == L"ban" || e.Op == L"kick" || e.Op == L"mute";
        // An admin picks an entry to undo it (an edit, a deleted item); an undone one is grey.
        row.dim = e.Undone();
        row.selected = admin && e.Id > 0 && e.Id == m_logSelected;
        if (row.selected)
            picked = &e;
        updated.log.push_back(std::move(row));
    }

    if (admin)
        updated.actions = {Button(LOG_UNDO, I18N::Game::GmPanelLogUndo, SWITCH_WIDTH, false, picked == nullptr || !picked->CanUndo())};
}

void mu::ui::window::CGmPanelWindow::SyncEvents(GmPanelRmlModel& updated)
{
    const auto& list = Panel().Events();
    const bool waiting = std::chrono::steady_clock::now() - m_requestedAt < AnswerTimeout;
    updated.listInfo = ListLine(list.Received, list.Rows.size(), I18N::Game::GmPanelEventsCount, I18N::Game::GmPanelEmptyEvents, waiting, CONTENT_WIDTH);

    // Short in the list ("open 3:12"), long under it ("Entrance open, closes in 3:12").
    auto stateText = [](const GameLogic::GmPanel::EventInfo& e, int left, bool brief) -> std::wstring
    {
        const std::wstring clock = Clock(left);
        // Without seconds (Crywolf's notices): starting.
        if (e.State == L"soon")
            return left >= 0 ? Printf(brief ? I18N::Game::GmPanelEventTagSoon : I18N::Game::GmPanelEventStateSoon, clock.c_str())
                             : std::wstring(brief ? I18N::Game::GmPanelEventTagStarting : I18N::Game::GmPanelEventStateStarting);
        if (e.State == L"open")
            return Printf(brief ? I18N::Game::GmPanelEventTagOpen : I18N::Game::GmPanelEventStateOpen, clock.c_str());
        if (e.State == L"run")
            return left >= 0 ? Printf(brief ? I18N::Game::GmPanelEventTagRunLeft : I18N::Game::GmPanelEventStateRunLeft, clock.c_str())
                             : std::wstring(brief ? I18N::Game::GmPanelEventTagRun : I18N::Game::GmPanelEventStateRun);
        if (e.State == L"off")
            return brief ? I18N::Game::GmPanelEventTagOff : I18N::Game::GmPanelEventStateOff;
        return brief ? std::wstring() : std::wstring(I18N::Game::GmPanelEventStateIdle);
    };
    auto nextText = [](const GameLogic::GmPanel::EventInfo& e)
    { return e.Next.empty() ? std::wstring() : Printf(I18N::Game::GmPanelEventNext, LocalTime(e.Next).c_str()); };

    const GameLogic::GmPanel::EventInfo* picked = nullptr;
    for (size_t i = 0; i < list.Rows.size(); ++i)
    {
        const auto& e = list.Rows[i];
        const int left = SecondsLeft(e.Left, m_eventsAt);
        GmPanelRow row;
        row.index = static_cast<int>(i);
        row.title = Line(Joined({e.Name, stateText(e, left, true)}, L"  "), ROW_WIDTH, true);
        row.detail = Line(Joined({nextText(e), e.Detail}, L", "), ROW_WIDTH);
        row.selected = e.Key == m_evSelected;
        row.dim = e.State == L"off";
        row.live = e.State == L"open" || e.State == L"run" || e.State == L"soon";
        updated.rows.push_back(std::move(row));
        if (e.Key == m_evSelected)
            picked = &e;
    }

    updated.selStats = Line({}, CONTENT_WIDTH);
    updated.selWhere = Line({}, CONTENT_WIDTH);
    if (picked == nullptr)
    {
        updated.selName = Line(I18N::Game::GmPanelEventPick, CONTENT_WIDTH);
    }
    else
    {
        updated.selName = Line(picked->Name, CONTENT_WIDTH, true);
        updated.selStats = Line(stateText(*picked, SecondsLeft(picked->Left, m_eventsAt), false), CONTENT_WIDTH);
        updated.selWhere = Line(Joined({nextText(*picked), picked->Detail}, L", "), CONTENT_WIDTH);
    }

    updated.actions = {
        Button(EV_START, I18N::Game::GmPanelEventStart, ACTION_WIDTH, false, picked == nullptr || !picked->Can(L's')),
        Button(EV_SKIP, I18N::Game::GmPanelEventSkip, ACTION_WIDTH, false, picked == nullptr || !picked->Can(L'k')),
        Button(EV_END, I18N::Game::GmPanelEventEnd, ACTION_WIDTH, false, picked == nullptr || !picked->Can(L'e')),
        Button(EV_REFRESH, I18N::Game::GmPanelRefresh, ACTION_WIDTH),
    };
}

void mu::ui::window::CGmPanelWindow::SyncMonsters(GmPanelRmlModel& updated)
{
    const auto& list = Panel().Monsters();
    const bool waiting = std::chrono::steady_clock::now() - m_requestedAt < AnswerTimeout;
    updated.find = Button(0, I18N::Game::GmPanelFind, ACTION_WIDTH);
    updated.listInfo = m_lastMonsterSearch.empty()
        ? Line(I18N::Game::GmPanelMonsterHint, CONTENT_WIDTH)
        : ListLine(list.Received, list.Rows.size(),
                   // The server sends at most MonsterLimit: a full list may leave some out.
                   list.Rows.size() >= MonsterLimit ? I18N::Game::GmPanelMonstersMany : I18N::Game::GmPanelMonstersCount,
                   I18N::Game::GmPanelEmptyMonsters, waiting, CONTENT_WIDTH);

    const GameLogic::GmPanel::MonsterInfo* picked = nullptr;
    for (size_t i = 0; i < list.Rows.size(); ++i)
    {
        const auto& m = list.Rows[i];
        GmPanelRow row;
        row.index = static_cast<int>(i);
        // One line a monster: the detail stays empty (and takes no room).
        row.title = Line(Printf(I18N::Game::GmPanelMonsterRow, m.Name.c_str(), m.Level, m.Number), ROW_WIDTH);
        row.selected = m.Number == m_monsterSelected;
        updated.rows.push_back(std::move(row));
        if (m.Number == m_monsterSelected)
            picked = &m;
    }

    updated.selName = picked == nullptr ? Line(I18N::Game::GmPanelMonsterPick, CONTENT_WIDTH)
                                        : Line(Printf(I18N::Game::GmPanelMonsterPicked, picked->Name.c_str(), picked->Number, picked->Level),
                                               CONTENT_WIDTH, true);
    updated.selStats = Line(I18N::Game::GmPanelSpawnHint, CONTENT_WIDTH);
    updated.selWhere = Line(I18N::Game::GmPanelClearHint, CONTENT_WIDTH);
    updated.countLabel = Line(I18N::Game::GmPanelCount, 34);
    updated.actions = {
        Button(EV_SPAWN, I18N::Game::GmPanelSpawn, ACTION_WIDTH, false, picked == nullptr),
        Button(EV_CLEAR, I18N::Game::GmPanelClear, ACTION_WIDTH),
    };
}

void mu::ui::window::CGmPanelWindow::SyncTiers(GmPanelRmlModel& updated)
{
    const auto& list = Panel().Tiers();
    const bool waiting = std::chrono::steady_clock::now() - m_requestedAt < AnswerTimeout;
    updated.listInfo = ListLine(list.Received, list.Rows.size(), I18N::Game::GmPanelTiersCount, I18N::Game::GmPanelEmptyTiers, waiting, CONTENT_WIDTH);

    const GameLogic::GmPanel::TierRun* picked = nullptr;
    for (size_t i = 0; i < list.Rows.size(); ++i)
    {
        const auto& r = list.Rows[i];
        GmPanelRow row;
        row.index = static_cast<int>(i);
        row.title = Line(Printf(I18N::Game::GmPanelTierTitle, r.Leader.c_str(), r.Tier, r.Day), ROW_WIDTH, true);
        row.detail = Line(Printf(I18N::Game::GmPanelTierDetail, MapName(r.Map).c_str(), r.Zone, r.Zones, r.Monsters,
                                 Clock(SecondsLeft(r.Left, m_tiersAt)).c_str()),
                          ROW_WIDTH);
        row.selected = SameName(r.Leader, m_tierSelected);
        row.live = !TierEnded(r.State);
        row.dim = TierEnded(r.State);
        updated.rows.push_back(std::move(row));
        if (SameName(r.Leader, m_tierSelected))
            picked = &r;
    }

    updated.selStats = Line({}, CONTENT_WIDTH);
    updated.selWhere = Line({}, CONTENT_WIDTH);
    if (picked == nullptr)
    {
        updated.selName = Line(I18N::Game::GmPanelTierPick, CONTENT_WIDTH);
    }
    else
    {
        updated.selName = Line(Printf(I18N::Game::GmPanelTierTitle, picked->Leader.c_str(), picked->Tier, picked->Day), CONTENT_WIDTH, true);
        updated.selStats = Line(Printf(I18N::Game::GmPanelTierParty, picked->Alive, picked->Roster.c_str()), CONTENT_WIDTH);
        updated.selWhere = Line(Printf(I18N::Game::GmPanelTierState, TierStateText(picked->State).c_str(),
                                       Clock(SecondsLeft(picked->Left, m_tiersAt)).c_str()),
                                CONTENT_WIDTH);
    }

    // A run that is over can't end again, and its loot minute can't be skipped.
    const bool open = picked != nullptr && !TierEnded(picked->State);
    updated.actions = {
        Button(EV_TIER_END, I18N::Game::GmPanelTierEnd, TAB_WIDTH, false, !open),
        Button(EV_TIER_SKIP, I18N::Game::GmPanelTierSkip, TAB_WIDTH, false, !open),
        Button(EV_REFRESH, I18N::Game::GmPanelRefresh, TAB_WIDTH),
    };
    updated.entriesInfo = Line(I18N::Game::GmPanelEntriesInfo, CONTENT_WIDTH);
    updated.entries = Button(EV_ENTRIES, I18N::Game::GmPanelEntriesReset, ACTION_WIDTH);
}

void mu::ui::window::CGmPanelWindow::SyncContent()
{
    auto& panel = Panel();

    GmPanelRmlModel updated;
    updated.title = Line(panel.IsAdmin() ? I18N::Game::GmPanelTitleAdmin : I18N::Game::GmPanelTitleMod, WINDOW_WIDTH, true);
    updated.tab = m_tab;
    // Admins have three more tabs, in a second row.
    updated.tabs = {
        Button(TAB_PLAYERS, I18N::Game::GmPanelTabPlayers, TAB_WIDTH, m_tab == TAB_PLAYERS),
        Button(TAB_WORLD, I18N::Game::GmPanelTabWorld, TAB_WIDTH, m_tab == TAB_WORLD),
        Button(TAB_LOG, I18N::Game::GmPanelTabLog, TAB_WIDTH, m_tab == TAB_LOG),
    };
    if (panel.IsAdmin())
    {
        updated.tabs.push_back(Button(TAB_EVENTS, I18N::Game::GmPanelTabEvents, TAB_WIDTH, m_tab == TAB_EVENTS));
        updated.tabs.push_back(Button(TAB_ITEMS, I18N::Game::GmPanelTabItems, TAB_WIDTH, m_tab == TAB_ITEMS));
        updated.tabs.push_back(Button(TAB_CHARACTER, I18N::Game::GmPanelTabCharacter, TAB_WIDTH, m_tab == TAB_CHARACTER));
    }
    updated.mode = m_tab == TAB_EVENTS ? m_evMode : m_tab == TAB_ITEMS ? m_itMode : m_tab == TAB_CHARACTER ? m_chMode : m_mode;

    if (m_tab == TAB_PLAYERS)
    {
        SyncPlayers(updated);
    }
    else if (m_tab == TAB_WORLD)
    {
        SyncWorld(updated);
    }
    else if (m_tab == TAB_LOG)
    {
        SyncLog(updated);
    }
    else if (m_tab == TAB_ITEMS)
    {
        SyncItems(updated);
    }
    else if (m_tab == TAB_CHARACTER)
    {
        SyncCharacter(updated);
    }
    else
    {
        updated.modes = {
            Button(EV_EVENTS, I18N::Game::GmPanelModeEvents, TAB_WIDTH, m_evMode == EV_EVENTS),
            Button(EV_MONSTERS, I18N::Game::GmPanelModeMonsters, TAB_WIDTH, m_evMode == EV_MONSTERS),
            Button(EV_TIERS, I18N::Game::GmPanelModeTiers, TAB_WIDTH, m_evMode == EV_TIERS),
        };
        if (m_evMode == EV_EVENTS)
            SyncEvents(updated);
        else if (m_evMode == EV_MONSTERS)
            SyncMonsters(updated);
        else
            SyncTiers(updated);
    }

    updated.status = WrappedLine(m_status);
    updated.statusBad = m_statusBad;

    auto& binder = m_RmlView.Binder();
    SyncField(binder, &GmPanelRmlModel::fieldPx, "field_px", UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Normal));
    SyncFieldFrom(binder, &GmPanelRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::tab, "tab", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::tabs, "tabs", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::mode, "mode", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::modes, "modes", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::find, "find", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::listInfo, "list_info", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::rows, "rows", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::selName, "sel_name", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::selStats, "sel_stats", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::selWhere, "sel_where", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::actions, "actions", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::warpsTitle, "warps_title", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::warps, "warps", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::mapLabel, "map_label", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::go, "go", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::switches, "switches", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::here, "here", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::logRefresh, "log_refresh", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::logInfo, "log_info", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::log, "log", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::countLabel, "count_label", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::entriesInfo, "entries_info", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::entries, "entries", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::steps, "steps", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::toggles, "toggles", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::itemName, "item_name", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::itemLine, "item_line", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::itemHint, "item_hint", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::itemCountLabel, "item_count_label", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::itemDurLabel, "item_dur_label", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::itemReady, "item_ready", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::charReady, "char_ready", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::charHasCmd, "char_has_cmd", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::charLabels, "char_labels", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::status, "status", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::statusBad, "status_bad", updated);
    // The typed texts (search, map, x, y, monster, count, entries, the items' search, count and durability, the
    // character's name and values) are the player's: the model owns them, nothing writes them here (picking or
    // loading an item sets the count and the durability; a character's answer sets its values).
}

// The Items tab (phase 3, admins only).
namespace
{
std::wstring GroupName(int group)
{
    const wchar_t* const names[] = {
        I18N::Game::GmPanelItemGroup0,  I18N::Game::GmPanelItemGroup1,  I18N::Game::GmPanelItemGroup2,  I18N::Game::GmPanelItemGroup3,
        I18N::Game::GmPanelItemGroup4,  I18N::Game::GmPanelItemGroup5,  I18N::Game::GmPanelItemGroup6,  I18N::Game::GmPanelItemGroup7,
        I18N::Game::GmPanelItemGroup8,  I18N::Game::GmPanelItemGroup9,  I18N::Game::GmPanelItemGroup10, I18N::Game::GmPanelItemGroup11,
        I18N::Game::GmPanelItemGroup12, I18N::Game::GmPanelItemGroup13, I18N::Game::GmPanelItemGroup14, I18N::Game::GmPanelItemGroup15,
    };
    return group >= 0 && group < static_cast<int>(std::size(names)) ? std::wstring(names[group]) : std::to_wstring(group);
}

// Lower case for the name search, Cyrillic too (towlower knows only the C locale's letters).
std::wstring Folded(std::wstring text)
{
    for (auto& c : text)
    {
        if (c >= 0x0410 && c <= 0x042F)
            c = static_cast<wchar_t>(c + 0x20);
        else if (c >= 0x0400 && c <= 0x040F)
            c = static_cast<wchar_t>(c + 0x50);
        else
            c = static_cast<wchar_t>(std::towlower(c));
    }
    return text;
}

// "14 13", "14,13" or "14.13": an item by its group and number.
bool ParseItemNumbers(const std::wstring& text, int& group, int& number)
{
    size_t i = 0;
    auto digits = [&](int& value)
    {
        const size_t start = i;
        value = 0;
        while (i < text.size() && text[i] >= L'0' && text[i] <= L'9' && i - start < 4)
            value = value * 10 + (text[i++] - L'0');
        return i > start;
    };
    auto separator = [&](size_t at) { return at < text.size() && (text[at] == L' ' || text[at] == L',' || text[at] == L'.'); };

    if (!digits(group) || !separator(i))
        return false;
    while (separator(i))
        ++i;
    return digits(number) && i == text.size();
}

// The item's name as the client shows it; the server's (English) while the client doesn't know it.
std::wstring ItemName(int group, int number)
{
    if (const auto* definition = g_ItemDatabase.Find(group, number); definition != nullptr && !definition->name.empty())
        return definition->name;
    const auto& info = Panel().Item();
    if (info.Is(group, number) && !info.Name.empty())
        return info.Name;
    return Printf(L"(%d,%d)", group, number);
}

// An option's text as the item's tooltip writes it: the first special that an item of that type and level gains
// from these options alone (the excellent byte, the option's type and level as OpenMU sends them). The server's name
// when the client has none (a Fenrir's colour).
std::wstring GainedText(int type, int level, int excellent, int optionType, int optionLevel, const std::wstring& fallback)
{
    static std::map<std::tuple<int, int, int, int, int>, std::wstring> cache;
    const auto key = std::make_tuple(type, level, excellent, optionType, optionLevel);
    if (const auto found = cache.find(key); found != cache.end())
        return found->second;

    const auto make = [type, level](int excellent, int optionType, int optionLevel)
    {
        ITEM item;
        std::memset(&item, 0, sizeof(ITEM));
        item.Type = static_cast<short>(type);
        item.Level = level;
        item.ExcellentFlags = static_cast<BYTE>(excellent);
        item.OptionType = static_cast<BYTE>(optionType & 0xF);
        item.OptionLevel = static_cast<BYTE>(optionLevel & 0xF);
        item.byColorState = ITEM_COLOR_NORMAL;
        for (int i = 0; i < MAX_SOCKETS; ++i)
        {
            item.bySocketOption[i] = SOCKET_EMPTY;
            item.SocketSeedID[i] = SOCKET_EMPTY;
        }
        SetItemAttributes(&item);
        return item;
    };
    const ITEM plain = make(0, 0, 0);
    const ITEM with = make(excellent, optionType, optionLevel);
    std::wstring text = fallback;
    for (int i = 0; i < with.SpecialNum && i < MAX_ITEM_SPECIAL; ++i)
    {
        bool had = false;
        for (int j = 0; j < plain.SpecialNum && j < MAX_ITEM_SPECIAL; ++j)
            had = had || plain.Special[j] == with.Special[i];
        if (had)
            continue;
        wchar_t buffer[256] = {};
        GetSpecialOptionText(type, buffer, with.Special[i], with.SpecialValue[i], 0);
        if (buffer[0] != 0)
        {
            text = buffer;
            break;
        }
    }
    cache.emplace(key, text);
    return text;
}

// An extra's text: an excellent or wing option is its bit of the excellent byte, a Fenrir's colour its flag there,
// a fixed option its bit of the option level.
std::wstring ExtraText(int type, int level, wchar_t kind, int bit, const std::wstring& fallback)
{
    if (kind == L'x')
        return GainedText(type, level, bit >= 0 && bit < 8 ? 1 << bit : 0, 0, 0, fallback);
    if (kind == L'r')
        return GainedText(type, level, bit, 0, 0, fallback);
    return GainedText(type, level, 0, 0, bit, fallback);
}

// "Sword of Destruction +13".
std::wstring ItemTitle(int group, int number, int level)
{
    const std::wstring name = ItemName(group, number);
    return level > 0 ? Printf(L"%ls +%d", name.c_str(), level) : name;
}

// An item a character has: an ancient one by its set name, as its tooltip calls it ("Hyon Gloves +13").
std::wstring ItemTitle(const ITEM& item)
{
    wchar_t ancient[64]{};
    if (item.AncientDiscriminator != 0 && g_csItemOption.GetAncientItemName(ancient, std::size(ancient), item.Type, item.AncientDiscriminator))
        return item.Level > 0 ? Printf(L"%ls +%d", ancient, static_cast<int>(item.Level)) : std::wstring(ancient);
    return ItemTitle(item.Type / MAX_ITEM_INDEX, item.Type % MAX_ITEM_INDEX, item.Level);
}

// How many inventory cells the item takes, across and down.
Rml::Vector2f ItemCells(int type)
{
    const auto* definition = g_ItemDatabase.Find(type);
    const float width = definition != nullptr && definition->width > 0 ? static_cast<float>(definition->width) : 1.f;
    const float height = definition != nullptr && definition->height > 0 ? static_cast<float>(definition->height) : 1.f;
    return {width, height};
}

// The extras' mask bits of the excellent (and wing) options.
int ExcellentMask(const GameLogic::GmPanel::ItemInfo& info)
{
    int mask = 0;
    for (const auto& extra : info.Extras)
    {
        if (extra.IsExcellent() && extra.Index >= 0 && extra.Index < 31)
            mask |= 1 << extra.Index;
    }
    return mask;
}

const GameLogic::GmPanel::ItemSetInfo* FindSet(const GameLogic::GmPanel::ItemInfo& info, int index)
{
    const auto found = std::find_if(info.Sets.begin(), info.Sets.end(), [&](const auto& set) { return set.Index == index; });
    return found == info.Sets.end() ? nullptr : &*found;
}

// The ancient step's choices in order: none, then every set with each of its bonus levels (a set without a bonus
// option once, with bonus 0).
std::vector<std::pair<int, int>> SetChoices(const GameLogic::GmPanel::ItemInfo& info)
{
    std::vector<std::pair<int, int>> choices{{0, 0}};
    for (const auto& set : info.Sets)
    {
        if (set.BonusLevels.empty())
            choices.emplace_back(set.Index, 0);
        for (const int level : set.BonusLevels)
            choices.emplace_back(set.Index, level);
    }
    return choices;
}

// "Hyon +10": the bonus option's level 1 is +5 to a stat, 2 is +10.
std::wstring SetText(const std::wstring& name, int bonus)
{
    return bonus > 0 ? Printf(L"%ls +%d", name.c_str(), bonus * 5) : name;
}

// Asks for the reason, then sends the request ("/gm mk ... <reason>").
void ConfirmItem(const std::wstring& request, const std::wstring& what, const std::wstring& where)
{
    ConfirmWithReason({{what, true}, {where, false}, {I18N::Game::GmPanelReasonRequired, false}}, request, MinItemReason, MaxReason,
                      "gm_panel_item");
}
} // namespace

void mu::ui::window::CGmPanelWindow::SyncItems(GmPanelRmlModel& updated)
{
    updated.modes = {
        Button(IT_CATALOG, I18N::Game::GmPanelModeCatalog, TAB_WIDTH, m_itMode == IT_CATALOG),
        Button(IT_MAKE, I18N::Game::GmPanelModeMake, TAB_WIDTH, m_itMode == IT_MAKE),
        Button(IT_SAVED, I18N::Game::GmPanelModeSaved, TAB_WIDTH, m_itMode == IT_SAVED),
    };

    // The server's answer about the item being made: the draft is fitted to it once.
    const auto& panel = Panel();
    if (m_draft.Picked() && panel.Item().Is(m_draft.Group, m_draft.Number) && m_draftInfoVersion != panel.ItemVersion())
        FitDraft();

    if (m_itMode == IT_CATALOG)
        SyncItemCatalog(updated);
    else if (m_itMode == IT_MAKE)
        SyncItemMake(updated);
    else
        SyncItemSaved(updated);
}

void mu::ui::window::CGmPanelWindow::SyncItemCatalog(GmPanelRmlModel& updated)
{
    if (m_itListDirty)
        BuildItemList();

    GmPanelStep group;
    group.label = Line(I18N::Game::GmPanelItemGroup, STEP_LABEL_WIDTH);
    group.value = Line(m_itSearch.empty() ? GroupName(m_itGroup) : std::wstring(I18N::Game::GmPanelItemAllGroups), STEP_VALUE_WIDTH, true);
    group.downId = IT_GROUP_DOWN;
    group.upId = IT_GROUP_UP;
    updated.steps = {group};
    updated.find = Button(0, I18N::Game::GmPanelFind, ACTION_WIDTH);
    updated.itemHint = Line(I18N::Game::GmPanelItemSearchHint, CONTENT_WIDTH);
    updated.listInfo = m_itTypes.empty()
        ? Line(I18N::Game::GmPanelItemEmpty, CONTENT_WIDTH)
        : Line(Printf(m_itMore ? I18N::Game::GmPanelItemMany : I18N::Game::GmPanelItemCount, static_cast<int>(m_itTypes.size())), CONTENT_WIDTH);

    const int picked = m_draft.Picked() ? m_draft.Group * MAX_ITEM_INDEX + m_draft.Number : -1;
    updated.rows = m_itRows;
    for (size_t i = 0; i < updated.rows.size() && i < m_itTypes.size(); ++i)
        updated.rows[i].selected = m_itTypes[i] == picked;
}

void mu::ui::window::CGmPanelWindow::SyncItemMake(GmPanelRmlModel& updated)
{
    const auto& info = Panel().Item();
    const bool ready = ItemInfoReady();
    updated.itemReady = ready;
    updated.itemLine = Line({}, ITEM_LINES_WIDTH);
    updated.itemHint = Line({}, ITEM_LINES_WIDTH);
    if (!m_draft.Picked())
    {
        updated.itemName = Line(I18N::Game::GmPanelItemNone, ITEM_LINES_WIDTH);
        return;
    }

    const ItemDraft& d = m_draft;
    updated.itemName = Line(ItemName(d.Group, d.Number), ITEM_LINES_WIDTH, true);
    if (!ready)
    {
        const bool waiting = std::chrono::steady_clock::now() - m_requestedAt < AnswerTimeout;
        updated.itemLine = Line(waiting ? I18N::Game::GmPanelAsking : I18N::Game::GmPanelNoAnswer, ITEM_LINES_WIDTH);
        return;
    }

    updated.itemLine = Line(info.Stack           ? Printf(I18N::Game::GmPanelItemStackLine, d.Group, d.Number, info.MaxDurability)
                            : info.MaxLevel > 0 ? Printf(I18N::Game::GmPanelItemInfoLine, d.Group, d.Number, info.MaxLevel)
                                                : Printf(L"(%d,%d)", d.Group, d.Number),
                            ITEM_LINES_WIDTH);
    updated.itemHint = Line(I18N::Game::GmPanelItemHover, ITEM_LINES_WIDTH);

    if (info.MaxLevel > 0)
    {
        GmPanelStep level;
        level.label = Line(I18N::Game::GmPanelItemLevel, STEP_LABEL_WIDTH);
        level.value = Line(Printf(L"+%d", d.Level), STEP_VALUE_WIDTH, true);
        level.downId = IT_LEVEL_DOWN;
        level.upId = IT_LEVEL_UP;
        level.downOff = d.Level <= 0;
        level.upOff = d.Level >= info.MaxLevel;
        updated.steps.push_back(std::move(level));
    }

    if (!info.Options.empty())
    {
        const auto& kind = info.Options[static_cast<size_t>(std::clamp(m_optionKind, 0, static_cast<int>(info.Options.size()) - 1))];
        // Several kinds (wings): which one the option is, a step of its own.
        if (info.Options.size() > 1)
        {
            GmPanelStep kinds;
            kinds.label = Line(I18N::Game::GmPanelItemKind, STEP_LABEL_WIDTH);
            // The kind as the tooltip writes it, at the option's level (the server sends the Number + 1).
            kinds.value = ClippedLine(GainedText(d.Group * MAX_ITEM_INDEX + d.Number, d.Level, 0, kind.Number - 1,
                                                 std::max(1, d.Option != 0 ? d.OptionLevel : 1), kind.Name),
                                      STEP_VALUE_WIDTH);
            kinds.downId = IT_OPTION_KIND_DOWN;
            kinds.upId = IT_OPTION_KIND;
            updated.steps.push_back(std::move(kinds));
        }

        GmPanelStep option;
        option.label = Line(I18N::Game::GmPanelItemOption, STEP_LABEL_WIDTH);
        option.value = Line(d.Option == 0 ? std::wstring(I18N::Game::GmPanelItemOff) : Printf(L"%d / %d", d.OptionLevel, kind.MaxLevel),
                            STEP_VALUE_WIDTH, true);
        option.downId = IT_OPTION_DOWN;
        option.upId = IT_OPTION_UP;
        option.downOff = d.Option == 0;
        option.upOff = d.Option != 0 && d.OptionLevel >= kind.MaxLevel;
        updated.steps.push_back(std::move(option));
    }

    if (!info.Sets.empty())
    {
        const auto choices = SetChoices(info);
        const auto at = std::find(choices.begin(), choices.end(), std::pair<int, int>{d.Set, d.Bonus});
        const int index = at == choices.end() ? 0 : static_cast<int>(at - choices.begin());
        GmPanelStep ancient;
        ancient.label = Line(I18N::Game::GmPanelItemAncient, STEP_LABEL_WIDTH);
        ancient.value = Line(d.Set == 0 ? std::wstring(I18N::Game::GmPanelItemOff) : SetText(d.SetName, d.Bonus), STEP_VALUE_WIDTH, true);
        ancient.downId = IT_SET_DOWN;
        ancient.upId = IT_SET_UP;
        ancient.downOff = index <= 0;
        ancient.upOff = index >= static_cast<int>(choices.size()) - 1;
        updated.steps.push_back(std::move(ancient));
    }

    updated.toggles = {
        Button(IT_LUCK, I18N::Game::GmPanelItemLuck, TAB_WIDTH, d.Luck, !info.Luck),
        Button(IT_SKILL, I18N::Game::GmPanelItemSkill, TAB_WIDTH, d.Skill, !info.Skill),
    };
    updated.itemCountLabel = Line(I18N::Game::GmPanelItemCountLabel, COUNT_LABEL_WIDTH);
    updated.itemDurLabel = Line(info.Stack ? I18N::Game::GmPanelItemPiecesLabel : I18N::Game::GmPanelItemDurLabel, DUR_LABEL_WIDTH);

    // The extras, a click switches one on or off; an excellent one is grey while a set is chosen (it would clear it).
    updated.listInfo = info.Extras.empty()
        ? Line(I18N::Game::GmPanelItemNoExtras, CONTENT_WIDTH)
        : Line(Printf(I18N::Game::GmPanelItemExtras, std::popcount(static_cast<unsigned>(d.Extras)), static_cast<int>(info.Extras.size())),
               CONTENT_WIDTH);
    for (size_t i = 0; i < info.Extras.size(); ++i)
    {
        const auto& extra = info.Extras[i];
        GmPanelRow row;
        row.index = static_cast<int>(i);
        row.title = ClippedLine(ExtraText(d.Group * MAX_ITEM_INDEX + d.Number, d.Level, extra.Kind, extra.Bit, extra.Name), ROW_WIDTH);
        row.selected = extra.Index >= 0 && extra.Index < 31 && (d.Extras & (1 << extra.Index)) != 0;
        row.dim = extra.IsExcellent() && d.Set != 0;
        updated.rows.push_back(std::move(row));
    }

    updated.actions = {
        Button(IT_CREATE, I18N::Game::GmPanelItemCreate, ITEM_BUTTON_WIDTH),
        Button(IT_SAVE, I18N::Game::GmPanelItemSave, ITEM_BUTTON_WIDTH),
    };
}

void mu::ui::window::CGmPanelWindow::SyncItemSaved(GmPanelRmlModel& updated)
{
    const auto& saved = GameConfig::GetInstance().GetGmItemFavourites();
    updated.listInfo = saved.empty() ? Line(I18N::Game::GmPanelItemSavedEmpty, CONTENT_WIDTH)
                                     : Line(Printf(I18N::Game::GmPanelItemSavedCount, static_cast<int>(saved.size())), CONTENT_WIDTH);
    for (size_t i = 0; i < saved.size(); ++i)
    {
        GmPanelRow row;
        row.index = static_cast<int>(i);
        ItemDraft draft;
        if (ParseSaved(saved[i], draft))
        {
            row.title = Line(ItemTitle(draft.Group, draft.Number, draft.Level), ROW_WIDTH, true);
            row.detail = Line(DraftSummary(draft), ROW_WIDTH);
        }
        else
        {
            row.title = Line(saved[i], ROW_WIDTH);
            row.dim = true;
        }
        row.selected = static_cast<int>(i) == m_savedSelected;
        updated.rows.push_back(std::move(row));
    }

    const bool picked = m_savedSelected >= 0 && m_savedSelected < static_cast<int>(saved.size());
    updated.actions = {
        Button(IT_LOAD, I18N::Game::GmPanelItemLoad, SWITCH_WIDTH, false, !picked),
        Button(IT_DELETE, I18N::Game::GmPanelItemDelete, SWITCH_WIDTH, false, !picked),
    };
}

void mu::ui::window::CGmPanelWindow::BuildItemList()
{
    m_itListDirty = false;
    m_itTypes.clear();
    m_itRows.clear();
    m_itMore = false;

    // By the group; or by the searched text in every group: "14 13" is that item, anything else a part of the
    // name as shown or in English.
    int group = -1;
    int number = -1;
    const bool byNumbers = ParseItemNumbers(m_itSearch, group, number);
    const std::wstring part = Folded(m_itSearch);
    for (const auto& definition : g_ItemDatabase.GetAllSlots())
    {
        if (!definition.Exists())
            continue;

        bool match = false;
        if (m_itSearch.empty())
            match = definition.group == m_itGroup;
        else if (byNumbers)
            match = definition.group == group && definition.number == number;
        else
            match = Folded(definition.name).find(part) != std::wstring::npos ||
                    Folded(StringUtils::NarrowToWide(definition.names.GetNeutral().c_str())).find(part) != std::wstring::npos;
        if (!match)
            continue;

        if (m_itTypes.size() >= MaxCatalogRows)
        {
            m_itMore = true;
            break;
        }
        m_itTypes.push_back(definition.group * MAX_ITEM_INDEX + definition.number);
    }

    // One line an item: its name and (group,number). Made once a list, not every frame (the names are measured).
    for (size_t i = 0; i < m_itTypes.size(); ++i)
    {
        const int g = m_itTypes[i] / MAX_ITEM_INDEX;
        const int n = m_itTypes[i] % MAX_ITEM_INDEX;
        GmPanelRow row;
        row.index = static_cast<int>(i);
        row.title = Line(Printf(L"%ls  (%d,%d)", ItemName(g, n).c_str(), g, n), ROW_WIDTH);
        m_itRows.push_back(std::move(row));
    }
}

void mu::ui::window::CGmPanelWindow::SearchItems()
{
    m_itSearch = OneLine(StringUtils::NarrowToWide(m_RmlView.GetModel().itemSearchText), MaxItemSearch);
    m_itListDirty = true;
}

void mu::ui::window::CGmPanelWindow::PickItem(int group, int number)
{
    m_draft = ItemDraft{};
    m_draft.Group = group;
    m_draft.Number = number;
    m_draftFresh = true;
    m_optionKind = 0;
    m_draftInfoVersion = -1;

    // One whole item unless the GM types more.
    auto& binder = m_RmlView.Binder();
    SyncField(binder, &GmPanelRmlModel::itemCountText, "item_count_text", Rml::String("1"));
    SyncField(binder, &GmPanelRmlModel::itemDurText, "item_dur_text", Rml::String());

    m_itMode = IT_MAKE;
    RequestCurrent();
}

void mu::ui::window::CGmPanelWindow::FitDraft()
{
    const auto& panel = Panel();
    const auto& info = panel.Item();
    m_draftInfoVersion = panel.ItemVersion();
    ItemDraft& d = m_draft;

    d.Level = std::clamp(d.Level, 0, std::max(0, info.MaxLevel));
    // A new item has the skill its kind comes with, as a dropped one.
    if (std::exchange(m_draftFresh, false))
        d.Skill = info.Skill;
    d.Luck = d.Luck && info.Luck;
    d.Skill = d.Skill && info.Skill;

    const auto option = std::find_if(info.Options.begin(), info.Options.end(), [&](const auto& o) { return o.Number == d.Option; });
    if (d.Option == 0 || option == info.Options.end())
    {
        d.Option = 0;
        d.OptionLevel = 0;
    }
    else
    {
        d.OptionLevel = std::clamp(d.OptionLevel, 1, std::max(1, option->MaxLevel));
        m_optionKind = static_cast<int>(option - info.Options.begin());
    }
    m_optionKind = std::clamp(m_optionKind, 0, std::max(0, static_cast<int>(info.Options.size()) - 1));

    int allowed = 0;
    for (const auto& extra : info.Extras)
    {
        if (extra.Index >= 0 && extra.Index < 31)
            allowed |= 1 << extra.Index;
    }
    d.Extras &= allowed;

    if (const auto* set = FindSet(info, d.Set))
    {
        if (set->BonusLevels.empty())
            d.Bonus = 0;
        else if (std::find(set->BonusLevels.begin(), set->BonusLevels.end(), d.Bonus) == set->BonusLevels.end())
            d.Bonus = set->BonusLevels.back();
        d.SetName = set->Name;
        // An item is excellent or ancient, not both (the server refuses it).
        d.Extras &= ~ExcellentMask(info);
    }
    else
    {
        d.Set = 0;
        d.Bonus = 0;
        d.SetName.clear();
    }
}

bool mu::ui::window::CGmPanelWindow::ItemInfoReady() const
{
    const auto& panel = Panel();
    return m_draft.Picked() && panel.Item().Is(m_draft.Group, m_draft.Number) && m_draftInfoVersion == panel.ItemVersion();
}

mu::ui::window::CGmPanelWindow::ItemDraft mu::ui::window::CGmPanelWindow::CurrentDraft() const
{
    ItemDraft draft = m_draft;
    const auto& model = m_RmlView.GetModel();
    const Rml::String count = UI::RmlBridge::KeepDigitsOnly(model.itemCountText);
    draft.Count = std::clamp(count.empty() ? 1 : std::atoi(count.c_str()), 1, MaxItemCount);
    const Rml::String durability = UI::RmlBridge::KeepDigitsOnly(model.itemDurText);
    draft.Durability = std::clamp(durability.empty() ? 0 : std::atoi(durability.c_str()), 0, 255);
    return draft;
}

void mu::ui::window::CGmPanelWindow::ToggleExtra(int row)
{
    if (!ItemInfoReady())
        return;
    const auto& info = Panel().Item();
    if (row < 0 || row >= static_cast<int>(info.Extras.size()))
        return;
    const auto& extra = info.Extras[static_cast<size_t>(row)];
    if (extra.Index < 0 || extra.Index >= 31)
        return;

    const int bit = 1 << extra.Index;
    m_draft.Extras ^= bit;
    if ((m_draft.Extras & bit) == 0)
        return;

    // An excellent option takes the set away; a Fenrir has one colour.
    if (extra.IsExcellent())
    {
        m_draft.Set = 0;
        m_draft.Bonus = 0;
        m_draft.SetName.clear();
    }
    if (extra.Kind == L'r')
    {
        for (const auto& other : info.Extras)
        {
            if (other.Kind == L'r' && other.Index != extra.Index && other.Index >= 0 && other.Index < 31)
                m_draft.Extras &= ~(1 << other.Index);
        }
    }
}

void mu::ui::window::CGmPanelWindow::DoItemsAction(int action)
{
    if (!Panel().IsAdmin())
        return;

    const auto& info = Panel().Item();
    const bool ready = ItemInfoReady();
    ItemDraft& d = m_draft;
    const int kinds = static_cast<int>(info.Options.size());
    switch (action)
    {
    case IT_GROUP_DOWN:
    case IT_GROUP_UP:
        // The arrows step through the groups; after a search, they first go back to the group.
        if (m_itSearch.empty())
            m_itGroup = (m_itGroup + (action == IT_GROUP_UP ? 1 : ItemGroups - 1)) % ItemGroups;
        m_itSearch.clear();
        SyncField(m_RmlView.Binder(), &GmPanelRmlModel::itemSearchText, "item_search_text", Rml::String());
        m_itListDirty = true;
        break;
    case IT_LEVEL_DOWN:
        if (ready && d.Level > 0)
            --d.Level;
        break;
    case IT_LEVEL_UP:
        if (ready && d.Level < info.MaxLevel)
            ++d.Level;
        break;
    case IT_OPTION_KIND:
    case IT_OPTION_KIND_DOWN:
        if (ready && kinds > 1)
        {
            m_optionKind = (std::clamp(m_optionKind, 0, kinds - 1) + (action == IT_OPTION_KIND ? 1 : kinds - 1)) % kinds;
            // A chosen option becomes the new kind, at its level if it has it.
            if (d.Option != 0)
            {
                const auto& kind = info.Options[static_cast<size_t>(m_optionKind)];
                d.Option = kind.Number;
                d.OptionLevel = std::clamp(d.OptionLevel, 1, std::max(1, kind.MaxLevel));
            }
        }
        break;
    case IT_OPTION_DOWN:
        if (ready && d.Option != 0)
        {
            if (d.OptionLevel > 1)
            {
                --d.OptionLevel;
            }
            else
            {
                d.Option = 0;
                d.OptionLevel = 0;
            }
        }
        break;
    case IT_OPTION_UP:
        if (ready && kinds > 0)
        {
            const auto& kind = info.Options[static_cast<size_t>(std::clamp(m_optionKind, 0, kinds - 1))];
            if (d.Option == 0)
            {
                d.Option = kind.Number;
                d.OptionLevel = 1;
            }
            else if (d.OptionLevel < kind.MaxLevel)
            {
                ++d.OptionLevel;
            }
        }
        break;
    case IT_SET_DOWN:
    case IT_SET_UP:
        if (ready && !info.Sets.empty())
        {
            const auto choices = SetChoices(info);
            const auto at = std::find(choices.begin(), choices.end(), std::pair<int, int>{d.Set, d.Bonus});
            int index = at == choices.end() ? 0 : static_cast<int>(at - choices.begin());
            index = std::clamp(index + (action == IT_SET_UP ? 1 : -1), 0, static_cast<int>(choices.size()) - 1);
            d.Set = choices[static_cast<size_t>(index)].first;
            d.Bonus = choices[static_cast<size_t>(index)].second;
            const auto* set = FindSet(info, d.Set);
            d.SetName = set != nullptr ? set->Name : std::wstring();
            if (set != nullptr)
                d.Extras &= ~ExcellentMask(info);
        }
        break;
    case IT_LUCK:
        if (ready && info.Luck)
            d.Luck = !d.Luck;
        break;
    case IT_SKILL:
        if (ready && info.Skill)
            d.Skill = !d.Skill;
        break;
    case IT_CREATE:
        CreateItem();
        break;
    case IT_SAVE:
        SaveDraft();
        break;
    case IT_LOAD:
        LoadSaved();
        break;
    case IT_DELETE:
        DeleteSaved();
        break;
    default:
        break;
    }
}

void mu::ui::window::CGmPanelWindow::CreateItem()
{
    if (!ItemInfoReady())
        return;

    const ItemDraft draft = CurrentDraft();
    const std::wstring what = Printf(L"%d x %ls", draft.Count, ItemTitle(draft.Group, draft.Number, draft.Level).c_str());
    GenericMenuConfig menu;
    menu.title = Printf(I18N::Game::GmPanelItemMakeTitle, draft.Count, ItemName(draft.Group, draft.Number).c_str());
    auto add = [&](const std::wstring& label, const std::wstring& to)
    {
        GenericMenuConfig::MenuButton button;
        button.label = label;
        button.onClick = [request = MakeRequest(draft, to), what, label] { ConfirmItem(request, what, label); };
        menu.buttons.push_back(std::move(button));
    };
    add(I18N::Game::GmPanelItemToMe, L"@me");
    // The player picked on the Players tab, while in game.
    if (const Target target = GetTarget(); target.Online && !target.Name.empty())
        add(Printf(I18N::Game::GmPanelItemToPlayer, target.Name.c_str()), target.Name);
    add(I18N::Game::GmPanelItemToGround, L"@ground");

    GenericMenuConfig::MenuButton cancel;
    cancel.label = I18N::Game::Cancel;
    cancel.compact = true;
    cancel.dismiss = true;
    menu.buttons.push_back(std::move(cancel));
    g_pGenericMenuDialog->Show(std::move(menu));
}

void mu::ui::window::CGmPanelWindow::SaveDraft()
{
    if (!m_draft.Picked())
        return;

    auto& config = GameConfig::GetInstance();
    std::vector<std::wstring> saved = config.GetGmItemFavourites();
    const std::wstring text = SavedText(CurrentDraft());
    if (std::find(saved.begin(), saved.end(), text) == saved.end())
    {
        if (saved.size() >= MaxSavedItems)
        {
            m_status = Printf(I18N::Game::GmPanelItemSavedFull, static_cast<int>(MaxSavedItems));
            m_statusBad = true;
            return;
        }
        saved.push_back(text);
        config.SetGmItemFavourites(saved);
    }
    m_status = Printf(I18N::Game::GmPanelItemSaved, ItemTitle(m_draft.Group, m_draft.Number, m_draft.Level).c_str());
    m_statusBad = false;
}

void mu::ui::window::CGmPanelWindow::LoadSaved()
{
    const auto& saved = GameConfig::GetInstance().GetGmItemFavourites();
    ItemDraft draft;
    if (m_savedSelected < 0 || m_savedSelected >= static_cast<int>(saved.size()) ||
        !ParseSaved(saved[static_cast<size_t>(m_savedSelected)], draft))
        return;

    // As saved; fitted again to what the server says the item can have.
    m_draft = draft;
    m_draftFresh = false;
    m_optionKind = 0;
    m_draftInfoVersion = -1;
    auto& binder = m_RmlView.Binder();
    SyncField(binder, &GmPanelRmlModel::itemCountText, "item_count_text", Rml::String(std::to_string(draft.Count)));
    SyncField(binder, &GmPanelRmlModel::itemDurText, "item_dur_text",
              draft.Durability > 0 ? Rml::String(std::to_string(draft.Durability)) : Rml::String());

    m_itMode = IT_MAKE;
    RequestCurrent();
}

void mu::ui::window::CGmPanelWindow::DeleteSaved()
{
    auto& config = GameConfig::GetInstance();
    std::vector<std::wstring> saved = config.GetGmItemFavourites();
    if (m_savedSelected < 0 || m_savedSelected >= static_cast<int>(saved.size()))
        return;

    saved.erase(saved.begin() + m_savedSelected);
    config.SetGmItemFavourites(saved);
    m_savedSelected = std::min(m_savedSelected, static_cast<int>(saved.size()) - 1);
}

std::wstring mu::ui::window::CGmPanelWindow::SavedText(const ItemDraft& draft)
{
    std::wstring setName = draft.SetName;
    std::replace(setName.begin(), setName.end(), L'|', L' ');
    return Printf(L"%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%ls", draft.Group, draft.Number, draft.Level, draft.Option, draft.OptionLevel,
                  draft.Luck ? 1 : 0, draft.Skill ? 1 : 0, draft.Extras, draft.Set, draft.Bonus, draft.Durability, draft.Count, setName.c_str());
}

bool mu::ui::window::CGmPanelWindow::ParseSaved(const std::wstring& text, ItemDraft& draft)
{
    std::vector<std::wstring> fields;
    for (size_t start = 0;;)
    {
        const size_t bar = text.find(L'|', start);
        fields.push_back(text.substr(start, bar == std::wstring::npos ? std::wstring::npos : bar - start));
        if (bar == std::wstring::npos)
            break;
        start = bar + 1;
    }
    if (fields.size() < 12)
        return false;

    ItemDraft parsed;
    parsed.Group = ToInt(fields[0]);
    parsed.Number = ToInt(fields[1]);
    parsed.Level = std::max(0, ToInt(fields[2]));
    parsed.Option = std::max(0, ToInt(fields[3]));
    parsed.OptionLevel = std::max(0, ToInt(fields[4]));
    parsed.Luck = ToInt(fields[5]) != 0;
    parsed.Skill = ToInt(fields[6]) != 0;
    parsed.Extras = std::max(0, ToInt(fields[7]));
    parsed.Set = std::max(0, ToInt(fields[8]));
    parsed.Bonus = std::max(0, ToInt(fields[9]));
    parsed.Durability = std::clamp(ToInt(fields[10]), 0, 255);
    parsed.Count = std::clamp(ToInt(fields[11]), 1, MaxItemCount);
    if (fields.size() > 12)
        parsed.SetName = fields[12];
    if (parsed.Group < 0 || parsed.Group >= ItemGroups || parsed.Number < 0 || parsed.Number >= MAX_ITEM_INDEX)
        return false;

    draft = std::move(parsed);
    return true;
}

std::wstring mu::ui::window::CGmPanelWindow::DraftSummary(const ItemDraft& draft)
{
    std::vector<std::wstring> parts;
    if (draft.Option != 0)
        parts.push_back(Printf(I18N::Game::GmPanelItemSumOption, draft.OptionLevel));
    if (draft.Luck)
        parts.emplace_back(I18N::Game::GmPanelItemSumLuck);
    if (draft.Skill)
        parts.emplace_back(I18N::Game::GmPanelItemSumSkill);
    if (draft.Extras != 0)
        parts.push_back(Printf(I18N::Game::GmPanelItemSumExtras, std::popcount(static_cast<unsigned>(draft.Extras))));
    if (draft.Set != 0)
        parts.push_back(SetText(draft.SetName, draft.Bonus));
    if (draft.Count > 1)
        parts.push_back(Printf(L"x%d", draft.Count));
    const std::wstring text = Joined(parts, L", ");
    return text.empty() ? std::wstring(I18N::Game::GmPanelItemSumPlain) : text;
}

std::wstring mu::ui::window::CGmPanelWindow::MakeRequest(const ItemDraft& draft, const std::wstring& to)
{
    return Printf(L"mk %ls %d %d %d %d %d %d %d %d %d %d %d %d", to.c_str(), draft.Group, draft.Number, draft.Level, draft.Option,
                  draft.Option != 0 ? draft.OptionLevel : 0, draft.Luck ? 1 : 0, draft.Skill ? 1 : 0, draft.Extras, draft.Set,
                  draft.Set != 0 ? draft.Bonus : 0, draft.Durability, draft.Count);
}

void mu::ui::window::CGmPanelWindow::UpdatePreview()
{
    if (!m_draft.Picked())
    {
        m_preview.reset();
        return;
    }

    const ItemDraft draft = CurrentDraft();
    const auto& panel = Panel();
    if (m_preview && draft == m_previewOf && m_previewInfoVersion == panel.ItemVersion())
        return;
    m_previewOf = draft;
    m_previewInfoVersion = panel.ItemVersion();

    // Built as the item manager builds one from the server's item (CItemMng::CreateItemByParameters), with the
    // options as OpenMU sends them: a normal option is its number and level; without one, the fixed options' bits
    // are the option level; an excellent or wing option is a bit of the excellent byte, a Fenrir's colour its flag.
    const auto& info = panel.Item();
    const bool known = info.Is(draft.Group, draft.Number);
    auto item = std::make_unique<ITEM>();
    std::memset(item.get(), 0, sizeof(ITEM));
    item->Type = static_cast<short>(draft.Group * MAX_ITEM_INDEX + draft.Number);
    item->Level = draft.Level;
    item->HasLuck = draft.Luck;
    item->HasSkill = draft.Skill;

    int excellent = 0;
    int fixed = 0;
    for (const auto& extra : info.Extras)
    {
        if (!known || extra.Index < 0 || extra.Index >= 31 || (draft.Extras & (1 << extra.Index)) == 0)
            continue;
        if (extra.Kind == L'x' && extra.Bit >= 0 && extra.Bit < 8)
            excellent |= 1 << extra.Bit;
        else if (extra.Kind == L'r')
            excellent |= extra.Bit;
        else if (extra.Kind == L'f')
            fixed |= extra.Bit;
    }
    if (draft.Option != 0)
    {
        item->OptionType = static_cast<BYTE>((draft.Option - 1) & 0xF); // the server sends Number + 1
        item->OptionLevel = static_cast<BYTE>(draft.OptionLevel);
    }
    else if (fixed != 0)
    {
        item->OptionLevel = static_cast<BYTE>(fixed & 0xF);
    }
    item->ExcellentFlags = static_cast<BYTE>(excellent);
    if (const auto* set = known && draft.Set != 0 ? FindSet(info, draft.Set) : nullptr)
    {
        item->AncientDiscriminator = static_cast<BYTE>(set->Discriminator);
        item->AncientBonusOption = static_cast<BYTE>(draft.Bonus);
    }
    item->byColorState = ITEM_COLOR_NORMAL;
    for (int i = 0; i < MAX_SOCKETS; ++i)
    {
        item->bySocketOption[i] = SOCKET_EMPTY;
        item->SocketSeedID[i] = SOCKET_EMPTY;
    }
    SetItemAttributes(item.get());

    // The durability as the server makes it: a stack's pieces (1 unless typed), or the worn item's full
    // durability (it grows with the level and the options), or less when typed.
    if (known && info.Stack)
    {
        item->Durability = static_cast<BYTE>(draft.Durability == 0 ? 1 : std::min(draft.Durability, std::max(info.MaxDurability, 1)));
    }
    else
    {
        const int full = std::min(static_cast<int>(CalcMaxDurability(item.get(), &ItemAttribute[item->Type], item->Level)), 255);
        item->Durability = static_cast<BYTE>(draft.Durability == 0 ? full : std::min(draft.Durability, std::max(full, 1)));
    }
    m_preview = std::move(item);
}

float mu::ui::window::CGmPanelWindow::ItemFieldOfView() const
{
    const tagITEM* item = ShownItem();
    Rml::Element* icon = GetField(IconId());
    Rml::Vector2f offset;
    Rml::Vector2f size;
    if (item == nullptr || icon == nullptr || !UI::RmlBridge::DrawnContentBox(*icon, offset, size))
        return 1.f;

    // At a field of view of 1 the item camera draws an inventory cell about a 24th of the window high (20 px of
    // 480); a wider view draws it smaller, so the item's cells fill the icon's box.
    const Rml::Vector2f cells = ItemCells(item->Type);
    const float cellPx = std::min(size.x / cells.x, size.y / cells.y);
    const float nativeCellPx = static_cast<float>(WindowHeight) / 24.f;
    return std::clamp(nativeCellPx / std::max(cellPx, 1.f), 0.5f, 4.f);
}

void mu::ui::window::CGmPanelWindow::RenderItemIcon()
{
    const tagITEM* item = IsVisible() ? ShownItem() : nullptr;
    if (item == nullptr)
        return;

    Rml::Element* icon = GetField(IconId());
    Rml::Vector2f offset;
    Rml::Vector2f size;
    if (icon == nullptr || !UI::RmlBridge::DrawnContentBox(*icon, offset, size))
        return;

    // The item's own box of cells, as big as fits and centred in the icon's (the model's anchor is a part of it).
    const Rml::Vector2f cells = ItemCells(item->Type);
    const float cell = std::min(size.x / cells.x, size.y / cells.y);
    const Rml::Vector2f box{cell * cells.x, cell * cells.y};
    ::RenderItem3D(offset.x + (size.x - box.x) / 2.f, offset.y + (size.y - box.y) / 2.f, box.x, box.y, item->Type, item->Level,
                   item->ExcellentFlags, item->AncientDiscriminator, false);
}

// The Character tab and the Log tab's undo (phase 4, admins only).
namespace
{
// OpenMU's HeroState in words.
std::wstring PkText(int state)
{
    const wchar_t* const names[] = {
        I18N::Game::GmPanelPk0, I18N::Game::GmPanelPk1, I18N::Game::GmPanelPk2, I18N::Game::GmPanelPk3,
        I18N::Game::GmPanelPk4, I18N::Game::GmPanelPk5, I18N::Game::GmPanelPk6,
    };
    return state >= 0 && state < static_cast<int>(std::size(names)) ? std::wstring(names[state]) : std::to_wstring(state);
}

// Where a character's item is: "worn: helm", "bag 37", "store 210", "vault 5" (the slot as the server counts it).
std::wstring PlaceText(wchar_t storage, int slot)
{
    if (storage == L'v')
        return Printf(I18N::Game::GmPanelInvVault, slot);
    if (slot >= 0 && slot < WornSlots)
    {
        const std::wstring names = I18N::Game::GmPanelInvWornSlots;
        std::wstring name = std::to_wstring(slot);
        size_t start = 0;
        for (int i = 0; i <= slot; ++i)
        {
            const size_t comma = names.find(L',', start);
            if (i == slot)
                name = names.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start);
            if (comma == std::wstring::npos)
                break;
            start = comma + 1;
        }
        return Printf(I18N::Game::GmPanelInvWorn, name.c_str());
    }
    return Printf(slot >= FirstStoreSlot ? I18N::Game::GmPanelInvStore : I18N::Game::GmPanelInvBag, slot);
}

// "Lorencia 130 120".
std::wstring PlaceOnMap(int map, int x, int y)
{
    return Printf(L"%ls %d %d", MapName(map).c_str(), x, y);
}

// A typed number, or -1 when the field is empty (unchanged).
long long TypedNumber(const Rml::String& typed)
{
    const Rml::String digits = UI::RmlBridge::KeepDigitsOnly(typed);
    if (digits.empty() || digits.size() > 12)
        return -1;
    return std::atoll(digits.c_str());
}
} // namespace

void mu::ui::window::CGmPanelWindow::SyncCharacter(GmPanelRmlModel& updated)
{
    updated.modes = {
        Button(CH_EDITOR, I18N::Game::GmPanelModeEditor, SWITCH_WIDTH, m_chMode == CH_EDITOR),
        Button(CH_ITEMS, I18N::Game::GmPanelModeInventory, SWITCH_WIDTH, m_chMode == CH_ITEMS),
    };
    updated.find = Button(0, I18N::Game::GmPanelCharOpen, ACTION_WIDTH);
    TakeCharacter();

    // The character's two lines: its name, account and whether it plays; its class and place.
    const auto& info = Panel().Character();
    const bool waiting = std::chrono::steady_clock::now() - m_requestedAt < AnswerTimeout;
    updated.selStats = Line({}, CONTENT_WIDTH);
    if (m_charName.empty())
    {
        updated.selName = Line(I18N::Game::GmPanelCharHint, CONTENT_WIDTH);
    }
    else if (!CharacterReady())
    {
        updated.selName = Line(waiting ? I18N::Game::GmPanelAsking : I18N::Game::GmPanelNoAnswer, CONTENT_WIDTH);
    }
    else
    {
        const wchar_t* where = info.Where == L'g'   ? I18N::Game::GmPanelCharWhereGame
                               : info.Where == L'a' ? I18N::Game::GmPanelCharWhereAccount
                                                    : I18N::Game::GmPanelCharWhereOffline;
        updated.selName = Line(Printf(where, info.Name.c_str(), info.Account.c_str()), CONTENT_WIDTH, true);
        // The client's name of the map; the server's while the client has none.
        std::wstring map = MapName(info.Map);
        if (map == std::to_wstring(info.Map) && !info.MapName.empty())
            map = info.MapName;
        updated.selStats = Line(Printf(I18N::Game::GmPanelCharLine, info.ClassName.c_str(), map.c_str(), info.X, info.Y), CONTENT_WIDTH);
    }

    if (m_chMode == CH_EDITOR)
        SyncCharEditor(updated);
    else
        SyncCharItems(updated);
}

void mu::ui::window::CGmPanelWindow::SyncCharEditor(GmPanelRmlModel& updated)
{
    const bool ready = CharacterReady();
    updated.charReady = ready;
    updated.actions = {
        Button(CH_SAVE, I18N::Game::GmPanelCharSave, SWITCH_WIDTH, false, !ready),
        Button(CH_REFRESH, I18N::Game::GmPanelRefresh, SWITCH_WIDTH, false, m_charName.empty()),
    };
    if (!ready)
        return;

    const auto& info = Panel().Character();
    updated.charHasCmd = info.Stat(L"cmd") != nullptr;
    auto label = [](const wchar_t* text) { return Line(text, PAIR_LABEL_WIDTH); };
    updated.charLabels = {label(I18N::Game::GmPanelCharLevel), label(I18N::Game::GmPanelCharResets), label(I18N::Game::GmPanelCharPoints),
                          label(I18N::Game::GmPanelCharStr),   label(I18N::Game::GmPanelCharAgi),    label(I18N::Game::GmPanelCharVit),
                          label(I18N::Game::GmPanelCharEne),   label(I18N::Game::GmPanelCharCmd),    label(I18N::Game::GmPanelCharZen)};

    // The PK status (and the kills, which stay); the class, within the character's line, only while the account is
    // offline; where to move the character: nowhere, to the GM or to a teleport place.
    GmPanelStep pk;
    pk.label = Line(I18N::Game::GmPanelCharPk, STEP_LABEL_WIDTH);
    pk.value = ClippedLine(Printf(I18N::Game::GmPanelPkKills, PkText(m_chPk).c_str(), info.PkCount), STEP_VALUE_WIDTH, true);
    pk.downId = CH_PK_DOWN;
    pk.upId = CH_PK_UP;
    pk.downOff = m_chPk <= 0;
    pk.upOff = m_chPk >= MaxPkState;

    const auto& classes = info.Classes;
    const auto chosen = std::find_if(classes.begin(), classes.end(), [&](const auto& c) { return c.Number == m_chClass; });
    const int classIndex = chosen == classes.end() ? -1 : static_cast<int>(chosen - classes.begin());
    GmPanelStep cls;
    cls.label = Line(I18N::Game::GmPanelCharClass, STEP_LABEL_WIDTH);
    cls.value = ClippedLine(chosen != classes.end() ? chosen->Name : m_chClass == info.Class ? info.ClassName : std::to_wstring(m_chClass),
                            STEP_VALUE_WIDTH, true);
    cls.downId = CH_CLASS_DOWN;
    cls.upId = CH_CLASS_UP;
    cls.downOff = info.Online() || classIndex <= 0;
    cls.upOff = info.Online() || classIndex < 0 || classIndex >= static_cast<int>(classes.size()) - 1;

    const auto& warps = Panel().Warps().Rows;
    if (m_chMove >= static_cast<int>(warps.size()))
        m_chMove = MOVE_STAY;
    GmPanelStep move;
    move.label = Line(I18N::Game::GmPanelCharMove, STEP_LABEL_WIDTH);
    move.value = ClippedLine(m_chMove == MOVE_STAY    ? std::wstring(I18N::Game::GmPanelCharStay)
                             : m_chMove == MOVE_TO_ME ? std::wstring(I18N::Game::GmPanelCharToMe)
                                                      : warps[static_cast<size_t>(m_chMove)].Name,
                             STEP_VALUE_WIDTH, true);
    // Stay, to me, then the teleport places in their order.
    const int at = m_chMove == MOVE_STAY ? 0 : m_chMove == MOVE_TO_ME ? 1 : m_chMove + 2;
    move.downId = CH_MOVE_DOWN;
    move.upId = CH_MOVE_UP;
    move.downOff = at <= 0;
    move.upOff = at >= static_cast<int>(warps.size()) + 1;
    updated.steps = {std::move(pk), std::move(cls), std::move(move)};
}

void mu::ui::window::CGmPanelWindow::SyncCharItems(GmPanelRmlModel& updated)
{
    TakeInventory();
    const auto& panel = Panel();
    const auto& inv = panel.Inventory();
    const bool ready = !m_charName.empty() && m_invSeenVersion == panel.InventoryVersion() && SameName(inv.Name, m_charName);
    const bool waiting = std::chrono::steady_clock::now() - m_requestedAt < AnswerTimeout;
    if (m_charName.empty())
        updated.listInfo = Line({}, CONTENT_WIDTH);
    else if (!ready)
        updated.listInfo = Line(waiting ? I18N::Game::GmPanelAsking : I18N::Game::GmPanelNoAnswer, CONTENT_WIDTH);
    else
        updated.listInfo = Line(Printf(I18N::Game::GmPanelInvInfo, static_cast<int>(inv.Items.size()), Thousands(inv.Zen).c_str(),
                                       Thousands(inv.VaultZen).c_str()),
                                CONTENT_WIDTH);

    const int picked = ready ? PickedItemIndex() : -1;
    if (ready)
    {
        updated.rows = m_invRows;
        for (size_t i = 0; i < updated.rows.size(); ++i)
            updated.rows[i].selected = static_cast<int>(i) == picked;
    }

    // The picked item: its icon (its tooltip while hovered), name, place and durability.
    updated.itemLine = Line({}, ITEM_LINES_WIDTH);
    updated.itemHint = Line({}, ITEM_LINES_WIDTH);
    bool vaultLocked = false;
    if (picked < 0)
    {
        updated.itemName = Line(ready ? std::wstring(I18N::Game::GmPanelInvPick) : std::wstring(), ITEM_LINES_WIDTH);
    }
    else
    {
        const auto& stored = inv.Items[static_cast<size_t>(picked)];
        const tagITEM* item = m_invItems[static_cast<size_t>(picked)].get();
        // A vault item is deleted only while the account is offline (the game server keeps the open vault).
        vaultLocked = stored.Storage == L'v' && inv.Where != L'o';
        updated.itemName = ClippedLine(ItemTitle(*item), ITEM_LINES_WIDTH, true);
        updated.itemLine = Line(Printf(I18N::Game::GmPanelInvDur, PlaceText(stored.Storage, stored.Slot).c_str(), static_cast<int>(item->Durability)),
                                ITEM_LINES_WIDTH);
        updated.itemHint = Line(vaultLocked ? I18N::Game::GmPanelInvVaultOnline : I18N::Game::GmPanelItemHover, ITEM_LINES_WIDTH);
    }

    updated.actions = {
        Button(CH_ITEM_DELETE, I18N::Game::GmPanelItemDelete, TAB_WIDTH, false, picked < 0 || vaultLocked),
        Button(CH_ITEM_COPY, I18N::Game::GmPanelInvCopy, TAB_WIDTH, false, picked < 0),
        Button(CH_REFRESH, I18N::Game::GmPanelRefresh, TAB_WIDTH, false, m_charName.empty()),
    };
}

void mu::ui::window::CGmPanelWindow::OpenCharacter()
{
    const std::wstring name = OneLine(StringUtils::NarrowToWide(m_RmlView.GetModel().charNameText), MaxSearch);
    if (name.empty())
    {
        m_status = I18N::Game::GmPanelCharNeedName;
        m_statusBad = true;
        return;
    }

    // Another character: the pick and the last answer were about the one before.
    if (!SameName(name, m_charName))
    {
        m_invSlot = -1;
        m_status.clear();
        m_statusBad = false;
    }
    m_charName = name;
    // The answers are taken again (they may be about this name already, then a newer one comes).
    m_chSeenVersion = -1;
    m_invSeenVersion = -1;
    RequestCurrent();
}

void mu::ui::window::CGmPanelWindow::TakeCharacter()
{
    const auto& panel = Panel();
    if (panel.CharacterVersion() == m_chSeenVersion)
        return;
    m_chSeenVersion = panel.CharacterVersion();
    const auto& info = panel.Character();
    if (m_charName.empty() || !SameName(info.Name, m_charName))
        return;

    // The values as they are now, typed over by the GM; the steps from them; no move.
    m_charName = info.Name;
    m_chPk = std::clamp(info.PkState, 0, MaxPkState);
    m_chClass = info.Class;
    m_chMove = MOVE_STAY;
    auto& binder = m_RmlView.Binder();
    auto text = [](long long value) { return Rml::String(std::to_string(value)); };
    auto stat = [&](const wchar_t* key)
    {
        const auto* found = info.Stat(key);
        return found != nullptr ? text(found->Value) : Rml::String();
    };
    SyncField(binder, &GmPanelRmlModel::chLvText, "ch_lv_text", text(info.Level));
    SyncField(binder, &GmPanelRmlModel::chRsText, "ch_rs_text", text(info.Resets));
    SyncField(binder, &GmPanelRmlModel::chPtText, "ch_pt_text", text(info.Points));
    SyncField(binder, &GmPanelRmlModel::chStrText, "ch_str_text", stat(L"str"));
    SyncField(binder, &GmPanelRmlModel::chAgiText, "ch_agi_text", stat(L"agi"));
    SyncField(binder, &GmPanelRmlModel::chVitText, "ch_vit_text", stat(L"vit"));
    SyncField(binder, &GmPanelRmlModel::chEneText, "ch_ene_text", stat(L"ene"));
    SyncField(binder, &GmPanelRmlModel::chCmdText, "ch_cmd_text", stat(L"cmd"));
    SyncField(binder, &GmPanelRmlModel::chZenText, "ch_zen_text", text(info.Zen));
}

void mu::ui::window::CGmPanelWindow::TakeInventory()
{
    const auto& panel = Panel();
    if (panel.InventoryVersion() == m_invSeenVersion)
        return;
    m_invSeenVersion = panel.InventoryVersion();
    m_invItems.clear();
    m_invRows.clear();
    const auto& inv = panel.Inventory();
    if (m_charName.empty() || !SameName(inv.Name, m_charName))
        return;

    for (size_t i = 0; i < inv.Items.size(); ++i)
    {
        const auto& stored = inv.Items[i];
        // Read as the item manager reads a server's item (ParseItemData, CItemMng::CreateItemByParameters), but owned
        // here: no key, not in the manager's list. Padded with zeros: the format's optional bytes follow its flags.
        std::array<BYTE, 32> data{};
        std::copy_n(stored.Data.begin(), std::min(stored.Data.size(), data.size()), data.begin());
        const ItemCreationParams p = ParseItemData(std::span<const BYTE>(data.data(), data.size()));

        auto item = std::make_unique<ITEM>();
        std::memset(item.get(), 0, sizeof(ITEM));
        item->bPeriodItem = p.WithExpiration;
        item->bExpiredPeriod = p.IsExpired;
        item->Type = static_cast<short>(p.Group * MAX_ITEM_INDEX + p.Number);
        item->Level = p.Level;
        item->Durability = p.Durability;
        item->HasLuck = p.WithLuck;
        item->HasSkill = p.WithSkill;
        item->OptionType = p.OptionType;
        item->OptionLevel = p.OptionLevel;
        item->ExcellentFlags = p.ExcellentFlags;
        item->AncientDiscriminator = p.AncientDiscriminator;
        item->AncientBonusOption = p.AncientBonusOption;
        item->Jewel_Of_Harmony_Option = p.HarmonyOptionType;
        item->Jewel_Of_Harmony_OptionLevel = p.HarmonyOptionLevel;
        item->option_380 = p.HasGuardianOption;
        item->SocketCount = p.SocketCount;
        item->SocketSeedSetOption = p.SocketBonusOption;
        for (int s = 0; s < MAX_SOCKETS; ++s)
        {
            item->bySocketOption[s] = p.SocketOptions[s];
            if (item->bySocketOption[s] == SOCKET_EMPTY)
            {
                item->SocketSeedID[s] = SOCKET_EMPTY;
            }
            else
            {
                item->SocketSeedID[s] = item->bySocketOption[s] % SEASON4A::MAX_SOCKET_OPTION;
                item->SocketSphereLv[s] = item->bySocketOption[s] / SEASON4A::MAX_SOCKET_OPTION + 1;
            }
        }
        item->byColorState = ITEM_COLOR_NORMAL;
        SetItemAttributes(item.get());

        // One line an item: its name and where it is; excellent and ancient ones stand out.
        GmPanelRow row;
        row.index = static_cast<int>(i);
        row.title = ClippedLine(Joined({ItemTitle(*item), PlaceText(stored.Storage, stored.Slot)}, L"  "), ROW_WIDTH);
        row.live = p.ExcellentFlags != 0 || p.AncientDiscriminator != 0;
        m_invRows.push_back(std::move(row));
        m_invItems.push_back(std::move(item));
    }

    // A picked item that is gone (deleted, moved) is picked no more.
    if (PickedItemIndex() < 0)
        m_invSlot = -1;
}

bool mu::ui::window::CGmPanelWindow::CharacterReady() const
{
    const auto& panel = Panel();
    return !m_charName.empty() && m_chSeenVersion == panel.CharacterVersion() && SameName(panel.Character().Name, m_charName);
}

void mu::ui::window::CGmPanelWindow::SaveCharacter()
{
    if (!CharacterReady())
        return;

    // What changed, as "/gm edit" takes it ("lv=400") and as the GM reads it ("Level: 380 -> 400"). An empty field
    // stays as it is.
    const auto& info = Panel().Character();
    const auto& model = m_RmlView.GetModel();
    std::vector<std::wstring> spec;
    std::vector<GenericDialogConfig::Line> lines;
    lines.push_back({Printf(I18N::Game::GmPanelCharSaveConfirm, info.Name.c_str()), true});
    auto change = [&](const std::wstring& key, const std::wstring& value, const wchar_t* label, const std::wstring& before,
                      const std::wstring& after)
    {
        spec.push_back(key + L"=" + value);
        lines.push_back({Printf(L"%ls: %ls -> %ls", label, before.c_str(), after.c_str()), false});
    };
    auto number = [&](const Rml::String& typed, const wchar_t* key, const wchar_t* label, long long before)
    {
        const long long value = TypedNumber(typed);
        if (value >= 0 && value != before)
            change(key, std::to_wstring(value), label, Thousands(before), Thousands(value));
    };
    number(model.chLvText, L"lv", I18N::Game::GmPanelCharLevel, info.Level);
    number(model.chRsText, L"rs", I18N::Game::GmPanelCharResets, info.Resets);
    number(model.chPtText, L"pt", I18N::Game::GmPanelCharPoints, info.Points);
    const std::tuple<const wchar_t*, const Rml::String*, const wchar_t*> stats[] = {
        {L"str", &model.chStrText, I18N::Game::GmPanelCharStr}, {L"agi", &model.chAgiText, I18N::Game::GmPanelCharAgi},
        {L"vit", &model.chVitText, I18N::Game::GmPanelCharVit}, {L"ene", &model.chEneText, I18N::Game::GmPanelCharEne},
        {L"cmd", &model.chCmdText, I18N::Game::GmPanelCharCmd},
    };
    for (const auto& [key, typed, label] : stats)
    {
        if (const auto* stat = info.Stat(key))
            number(*typed, key, label, stat->Value);
    }
    number(model.chZenText, L"zen", I18N::Game::GmPanelCharZen, info.Zen);
    if (m_chPk != info.PkState)
        change(L"pk", std::to_wstring(m_chPk), I18N::Game::GmPanelCharPk, PkText(info.PkState), PkText(m_chPk));
    if (m_chClass != info.Class && !info.Online())
    {
        const auto chosen = std::find_if(info.Classes.begin(), info.Classes.end(), [&](const auto& c) { return c.Number == m_chClass; });
        change(L"cls", std::to_wstring(m_chClass), I18N::Game::GmPanelCharClass, info.ClassName,
               chosen != info.Classes.end() ? chosen->Name : std::to_wstring(m_chClass));
    }
    if (m_chMove != MOVE_STAY)
    {
        int map = 0;
        int x = 0;
        int y = 0;
        const auto& warps = Panel().Warps().Rows;
        if (m_chMove == MOVE_TO_ME && Hero != nullptr)
        {
            map = gMapManager.WorldActive;
            x = static_cast<int>(Hero->PositionX);
            y = static_cast<int>(Hero->PositionY);
        }
        else if (m_chMove >= 0 && m_chMove < static_cast<int>(warps.size()))
        {
            const auto& warp = warps[static_cast<size_t>(m_chMove)];
            map = warp.Map;
            x = warp.X;
            y = warp.Y;
        }
        else
        {
            return;
        }
        change(L"pos", Printf(L"%d:%d:%d", map, x, y), I18N::Game::GmPanelCharPlace, PlaceOnMap(info.Map, info.X, info.Y), PlaceOnMap(map, x, y));
    }

    if (spec.empty())
    {
        m_status = I18N::Game::GmPanelCharNothing;
        m_statusBad = false;
        return;
    }

    // The reason shares the chat line with the changes: with too many at once, it wouldn't fit.
    const std::wstring request = L"edit " + info.Name + L" " + Joined(spec, L",");
    const int room = ReasonRoom(request);
    if (room < static_cast<int>(MinItemReason) + 2)
    {
        m_status = I18N::Game::GmPanelCharTooLong;
        m_statusBad = true;
        return;
    }

    if (info.Where == L'a')
        lines.push_back({I18N::Game::GmPanelCharSavedOnLogout, false});
    lines.push_back({I18N::Game::GmPanelReasonRequired, false});
    ConfirmWithReason(std::move(lines), request, MinItemReason, room, "gm_panel_edit");
}

void mu::ui::window::CGmPanelWindow::AskItemAction(bool remove)
{
    const int index = PickedItemIndex();
    if (index < 0)
        return;

    const auto& inv = Panel().Inventory();
    const auto& stored = inv.Items[static_cast<size_t>(index)];
    const tagITEM* item = m_invItems[static_cast<size_t>(index)].get();
    if (remove && stored.Storage == L'v' && inv.Where != L'o')
    {
        m_status = I18N::Game::GmPanelInvVaultOnline;
        m_statusBad = true;
        return;
    }

    const std::wstring title = ItemTitle(*item);
    const std::wstring request = Printf(L"%ls %ls %lc %d", remove ? L"itemdel" : L"itemcopy", inv.Name.c_str(), stored.Storage, stored.Slot);
    std::vector<GenericDialogConfig::Line> lines = {
        {Printf(remove ? I18N::Game::GmPanelInvDeleteConfirm : I18N::Game::GmPanelInvCopyConfirm, title.c_str(), inv.Name.c_str()), true},
        {PlaceText(stored.Storage, stored.Slot), false},
    };
    if (remove)
        lines.push_back({I18N::Game::GmPanelInvUndoHint, false});
    lines.push_back({I18N::Game::GmPanelReasonRequired, false});
    ConfirmWithReason(std::move(lines), request, MinItemReason, ReasonRoom(request), remove ? "gm_panel_itemdel" : "gm_panel_itemcopy", item);
}

void mu::ui::window::CGmPanelWindow::DoCharacterAction(int action)
{
    auto& panel = Panel();
    if (!panel.IsAdmin())
        return;

    const auto& info = panel.Character();
    const bool ready = CharacterReady();
    switch (action)
    {
    case CH_SAVE:
        SaveCharacter();
        break;
    case CH_REFRESH:
        RequestCurrent();
        break;
    case CH_PK_DOWN:
        if (ready && m_chPk > 0)
            --m_chPk;
        break;
    case CH_PK_UP:
        if (ready && m_chPk < MaxPkState)
            ++m_chPk;
        break;
    case CH_CLASS_DOWN:
    case CH_CLASS_UP:
    {
        if (!ready)
            break;
        if (info.Online())
        {
            m_status = I18N::Game::GmPanelCharClassOffline;
            m_statusBad = true;
            break;
        }
        const auto& classes = info.Classes;
        const auto chosen = std::find_if(classes.begin(), classes.end(), [&](const auto& c) { return c.Number == m_chClass; });
        if (chosen == classes.end())
            break;
        const int index = static_cast<int>(chosen - classes.begin()) + (action == CH_CLASS_UP ? 1 : -1);
        if (index >= 0 && index < static_cast<int>(classes.size()))
            m_chClass = classes[static_cast<size_t>(index)].Number;
        break;
    }
    case CH_MOVE_DOWN:
    case CH_MOVE_UP:
    {
        if (!ready)
            break;
        // Stay, to me, then the teleport places in their order.
        const int places = static_cast<int>(panel.Warps().Rows.size());
        int at = m_chMove == MOVE_STAY ? 0 : m_chMove == MOVE_TO_ME ? 1 : m_chMove + 2;
        at = std::clamp(at + (action == CH_MOVE_UP ? 1 : -1), 0, places + 1);
        m_chMove = at == 0 ? MOVE_STAY : at == 1 ? MOVE_TO_ME : at - 2;
        break;
    }
    case CH_ITEM_DELETE:
        AskItemAction(true);
        break;
    case CH_ITEM_COPY:
        AskItemAction(false);
        break;
    default:
        break;
    }
}

void mu::ui::window::CGmPanelWindow::DoLogAction(int action)
{
    const auto& panel = Panel();
    if (action != LOG_UNDO || !panel.IsAdmin())
        return;

    const auto& rows = panel.Log().Rows;
    const auto entry = std::find_if(rows.begin(), rows.end(), [&](const auto& e) { return e.Id > 0 && e.Id == m_logSelected; });
    if (entry == rows.end() || !entry->CanUndo())
        return;

    const std::wstring request = L"undo " + std::to_wstring(entry->Id);
    std::vector<GenericDialogConfig::Line> lines = {
        {Printf(I18N::Game::GmPanelUndoConfirm, entry->Id, entry->Op.c_str(), entry->Target.c_str()), true},
    };
    if (!entry->Detail.empty())
        lines.push_back({entry->Detail, false});
    lines.push_back({I18N::Game::GmPanelReasonOptional, false});
    ConfirmWithReason(std::move(lines), request, 0, ReasonRoom(request), "gm_panel_undo");
}

int mu::ui::window::CGmPanelWindow::PickedItemIndex() const
{
    const auto& panel = Panel();
    if (m_invSlot < 0 || m_invSeenVersion != panel.InventoryVersion())
        return -1;
    const auto& items = panel.Inventory().Items;
    for (size_t i = 0; i < items.size() && i < m_invItems.size(); ++i)
    {
        if (items[i].Storage == m_invStorage && items[i].Slot == m_invSlot)
            return static_cast<int>(i);
    }
    return -1;
}

tagITEM* mu::ui::window::CGmPanelWindow::ShownItem() const
{
    if (m_tab == TAB_ITEMS && m_itMode == IT_MAKE)
        return m_preview.get();
    if (m_tab == TAB_CHARACTER && m_chMode == CH_ITEMS)
    {
        const int index = PickedItemIndex();
        return index >= 0 ? m_invItems[static_cast<size_t>(index)].get() : nullptr;
    }
    return nullptr;
}

const char* mu::ui::window::CGmPanelWindow::IconId() const
{
    return m_tab == TAB_CHARACTER ? "gm_char_icon" : "gm_item_icon";
}
