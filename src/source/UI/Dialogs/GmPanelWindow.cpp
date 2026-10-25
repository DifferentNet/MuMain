#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Dialogs/GmPanelWindow.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Commands/GmPanel.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Dialogs/GenericConfirmDialog.h"
#include "UI/Dialogs/GenericMenuDialog.h"
#include "World/MapInfra/MapManager.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/RmlUi/RmlUiRuntime.h"
#include "Render/Text/CUIRenderText.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/RmlBridge/RmlNumericInputFilter.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlSyncField.h"

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cstdarg>
#include <ctime>
#include <cwchar>
#include <cwctype>
#include <utility>

using namespace SEASON3B;
using namespace mu::ui::window;
using GameLogic::GmPanel::Panel;

namespace
{
// While the players tab shows who is in game, it asks again this often.
constexpr auto PlayersRefresh = std::chrono::seconds(10);
// Without an answer this long after asking, the server doesn't send it.
constexpr auto AnswerTimeout = std::chrono::seconds(5);
// The longest texts the server gets, in characters: the chat packet holds about 240 bytes, Cyrillic takes two.
constexpr int MaxReason = 60;
constexpr int MaxNotice = 100;
constexpr int MaxSearch = 10;

int MeasureInReferenceUnits(const std::wstring& text)
{
    return g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size())).cx;
}

// 21450 -> "21,450".
std::wstring Thousands(int value)
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
    RequestCurrent();
    m_focusSearchPending = m_tab == TAB_PLAYERS && m_mode == MODE_SEARCH;
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

    // Enter in a field: the search, or the teleport to the typed coordinates.
    const bool typing = m_RmlView.Document() != nullptr && RmlUiRuntime::Instance().GetKeyOwningDocument() == m_RmlView.Document();
    if (typing && IsPress(VK_RETURN))
    {
        if (m_tab == TAB_PLAYERS && m_mode == MODE_SEARCH)
            m_pendingFind = true;
        else if (m_tab == TAB_WORLD)
            m_pendingGo = true;
        return false;
    }

    return true;
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
    else
    {
        panel.Send(L"log");
    }
}

void mu::ui::window::CGmPanelWindow::SwitchTab(int tab)
{
    if (tab < TAB_PLAYERS || tab > TAB_LOG)
        return;
    m_tab = tab;
    RequestCurrent();
    m_focusSearchPending = m_tab == TAB_PLAYERS && m_mode == MODE_SEARCH;
}

void mu::ui::window::CGmPanelWindow::SwitchMode(int mode)
{
    if (mode < MODE_ONLINE || mode > MODE_BANS)
        return;
    m_mode = mode;
    RequestCurrent();
    m_focusSearchPending = m_mode == MODE_SEARCH;
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
    if (mode >= 0 && mode != m_mode)
        SwitchMode(mode);

    if (row >= 0)
    {
        auto& panel = Panel();
        const size_t index = static_cast<size_t>(row);
        if (m_mode == MODE_ONLINE && index < panel.Players().Rows.size())
            m_selected = panel.Players().Rows[index].Name;
        else if (m_mode == MODE_SEARCH && index < panel.Found().Rows.size())
            m_selected = panel.Found().Rows[index].Name;
        else if (m_mode == MODE_BANS && index < panel.Penalties().Rows.size())
            m_selected = panel.Penalties().Rows[index].Name;
    }

    if (find)
        Search();
    if (go)
        GoToCoordinates();
    if (warp >= 0)
        Send(L"warp " + std::to_wstring(warp));
    if (action > 0)
        DoAction(action);
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

        HandleClicks();
        TakeResult();

        if (IsVisible() && m_tab == TAB_PLAYERS && m_mode == MODE_ONLINE &&
            std::chrono::steady_clock::now() - m_playersAt >= PlayersRefresh)
        {
            panel.Send(L"players");
            m_playersAt = std::chrono::steady_clock::now();
        }
    }

    SyncRmlModel();
    return true;
}

bool mu::ui::window::CGmPanelWindow::Render()
{
    // Nothing native: the frame, the lists and the buttons are RmlUi. Kept because CObject requires it.
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
    c.RegisterArray<std::vector<GmPanelRow>>();

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
                            if (m_tab == TAB_PLAYERS && m_mode == MODE_SEARCH)
                                m_pendingFind = true;
                            else if (m_tab == TAB_WORLD)
                                m_pendingGo = true;
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
        if (Rml::Element* field = GetField("gm_search"))
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
    const auto& log = Panel().Log();
    const auto now = std::chrono::steady_clock::now();
    updated.logRefresh = Button(0, I18N::Game::GmPanelRefresh, SWITCH_WIDTH);
    updated.logInfo = !log.Received
        ? Line(now - m_requestedAt < AnswerTimeout ? I18N::Game::GmPanelAsking : I18N::Game::GmPanelNoAnswer, CONTENT_WIDTH)
        : log.Rows.empty() ? Line(I18N::Game::GmPanelEmptyLog, CONTENT_WIDTH)
                           : Line(Printf(I18N::Game::GmPanelLogCount, static_cast<int>(log.Rows.size())), CONTENT_WIDTH);
    for (size_t i = 0; i < log.Rows.size(); ++i)
    {
        const auto& e = log.Rows[i];
        GmPanelRow row;
        row.index = static_cast<int>(i);
        // "24.10 23:35 kick Name" over "GmName: reason", so the longer part has the whole second line.
        row.title = Line(Printf(I18N::Game::GmPanelLogTitle, LocalTime(e.Time).c_str(), e.Op.c_str(), e.Target.c_str()), ROW_WIDTH, true);
        row.detail = Line(e.Detail.empty() ? e.GmName : Printf(I18N::Game::GmPanelLogDetail, e.GmName.c_str(), e.Detail.c_str()), ROW_WIDTH);
        row.alert = e.Op == L"ban" || e.Op == L"kick" || e.Op == L"mute";
        updated.log.push_back(std::move(row));
    }
}

void mu::ui::window::CGmPanelWindow::SyncContent()
{
    auto& panel = Panel();

    GmPanelRmlModel updated;
    updated.title = Line(panel.IsAdmin() ? I18N::Game::GmPanelTitleAdmin : I18N::Game::GmPanelTitleMod, WINDOW_WIDTH, true);
    updated.tab = m_tab;
    updated.tabs = {
        Button(TAB_PLAYERS, I18N::Game::GmPanelTabPlayers, TAB_WIDTH, m_tab == TAB_PLAYERS),
        Button(TAB_WORLD, I18N::Game::GmPanelTabWorld, TAB_WIDTH, m_tab == TAB_WORLD),
        Button(TAB_LOG, I18N::Game::GmPanelTabLog, TAB_WIDTH, m_tab == TAB_LOG),
    };
    updated.mode = m_mode;

    if (m_tab == TAB_PLAYERS)
        SyncPlayers(updated);
    else if (m_tab == TAB_WORLD)
        SyncWorld(updated);
    else
        SyncLog(updated);

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
    SyncFieldFrom(binder, &GmPanelRmlModel::status, "status", updated);
    SyncFieldFrom(binder, &GmPanelRmlModel::statusBad, "status_bad", updated);
    // The typed texts (search, map, x, y) are the player's: the model owns them, nothing writes them here.
}
