#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Events/SoloKalimaWindow.h"

#include "Audio/DSPlaySound.h"
#include "GameLogic/Events/SoloKalima.h"
#include "UI/Core/WindowSystem.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/Text/CUIRenderText.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlSyncField.h"

#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <cstdarg>
#include <cwchar>

using namespace SEASON3B;
using namespace mu::ui::window;
using GameLogic::SoloKalima::Lair;

namespace
{
// While the window is open, the state is asked for again this often: the Combat Power and the tickets change.
constexpr auto RefreshInterval = std::chrono::seconds(5);
// Without an answer this long after opening, the server doesn't run Kundun's Lair.
constexpr auto AnswerTimeout = std::chrono::seconds(5);

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

// 5:12:30, 12:30 under an hour.
std::wstring FormatDuration(long long totalSeconds)
{
    const long long total = std::max<long long>(0, totalSeconds);
    const long long hours = total / 3600;
    const long long minutes = total % 3600 / 60;
    const long long seconds = total % 60;

    wchar_t text[32] = {};
    if (hours > 0)
        mu_swprintf(text, L"%lld:%02lld:%02lld", hours, minutes, seconds);
    else
        mu_swprintf(text, L"%lld:%02lld", minutes, seconds);
    return text;
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
SoloKalimaLine Line(const std::wstring& text, int width, bool bold = false)
{
    if (text.empty())
        return {};
    g_pRenderText->SetFont(bold ? g_hFontBold : g_hFont);
    const auto role = bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal;
    return {StringUtils::WideToNarrow(text.c_str()),
            UI::RmlBridge::NativeTextPxInBox(role, static_cast<float>(MeasureInReferenceUnits(text)), static_cast<float>(width))};
}

// The seconds to the next UTC day, counted down since the answer came.
long long SecondsToReset(const GameLogic::SoloKalima::Status& status)
{
    const auto since = std::chrono::steady_clock::now() - status.ReceivedAt;
    return status.SecondsToReset - std::chrono::duration_cast<std::chrono::seconds>(since).count();
}

std::wstring BestTime(int seconds)
{
    return seconds > 0 ? FormatDuration(seconds) : std::wstring(I18N::Game::SoloKalimaNoRecord);
}
} // namespace

mu::ui::window::CSoloKalimaWindow::CSoloKalimaWindow() = default;

mu::ui::window::CSoloKalimaWindow::~CSoloKalimaWindow()
{
    Release();
}

bool mu::ui::window::CSoloKalimaWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_SOLO_KALIMA, this);
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void mu::ui::window::CSoloKalimaWindow::Release()
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float mu::ui::window::CSoloKalimaWindow::GetLayerDepth()
{
    return LayerDepth;
}

float mu::ui::window::CSoloKalimaWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CSoloKalimaWindow::OpenningProcess()
{
    m_openedAt = std::chrono::steady_clock::now();
    m_seenEnterAnswers = Lair().EnterAnswers();
    // An answer from an earlier visit is no news any more.
    Lair().ClearEnterText();
    Lair().Request();
}

void mu::ui::window::CSoloKalimaWindow::ClosingProcess()
{
}

bool mu::ui::window::CSoloKalimaWindow::UpdateMouseEvent()
{
    // The rows, the checkboxes, the button and the corner close are RmlUi's (see Update()); a click on the window
    // stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool mu::ui::window::CSoloKalimaWindow::UpdateKeyEvent()
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_SOLO_KALIMA))
        return true;

    if (IsPress(VK_ESCAPE))
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_SOLO_KALIMA);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    return true;
}

int mu::ui::window::CSoloKalimaWindow::SelectedTier() const
{
    const auto& tiers = Lair().Tiers();
    const auto picked = std::find_if(tiers.begin(), tiers.end(), [this](const auto& t) { return t.Number == m_selectedTier; });
    if (picked != tiers.end())
        return picked->Number;

    int highestOpen = 0;
    for (const auto& tier : tiers)
    {
        if (tier.Open)
            highestOpen = std::max(highestOpen, tier.Number);
    }
    return highestOpen;
}

bool mu::ui::window::CSoloKalimaWindow::IsCursePicked(const std::wstring& key) const
{
    return std::find(m_pickedCurses.begin(), m_pickedCurses.end(), key) != m_pickedCurses.end();
}

void mu::ui::window::CSoloKalimaWindow::ToggleCurse(int index)
{
    const auto& curses = Lair().Curses();
    if (index < 0 || index >= static_cast<int>(curses.size()))
        return;

    const std::wstring& key = curses[static_cast<size_t>(index)].Key;
    const auto picked = std::find(m_pickedCurses.begin(), m_pickedCurses.end(), key);
    if (picked != m_pickedCurses.end())
        m_pickedCurses.erase(picked);
    else if (static_cast<int>(m_pickedCurses.size()) < Lair().GetStatus().MaxCurses)
        m_pickedCurses.push_back(key);
    else
        return;

    PlayBuffer(SOUND_CLICK01);
}

void mu::ui::window::CSoloKalimaWindow::KeepPickedCursesValid()
{
    const auto& curses = Lair().Curses();
    std::erase_if(m_pickedCurses,
                  [&curses](const std::wstring& key)
                  { return std::none_of(curses.begin(), curses.end(), [&key](const auto& c) { return c.Key == key; }); });

    const size_t limit = static_cast<size_t>(std::max(0, Lair().GetStatus().MaxCurses));
    if (m_pickedCurses.size() > limit)
        m_pickedCurses.resize(limit);
}

void mu::ui::window::CSoloKalimaWindow::HandleClicks()
{
    const bool exit = m_pendingExit;
    const bool enter = m_pendingEnter;
    const int select = m_pendingSelect;
    const int curse = m_pendingCurse;
    m_pendingExit = false;
    m_pendingEnter = false;
    m_pendingSelect = 0;
    m_pendingCurse = -1;
    if (!IsVisible())
        return;

    if (exit)
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_SOLO_KALIMA);
        PlayBuffer(SOUND_CLICK01);
        return;
    }

    auto& lair = Lair();
    if (lair.HasInfo())
        KeepPickedCursesValid();

    if (select > 0)
    {
        m_selectedTier = select;
        PlayBuffer(SOUND_CLICK01);
    }

    if (curse >= 0)
        ToggleCurse(curse);

    const int selected = SelectedTier();
    if (enter && selected > 0 && EnterBlocker(selected).empty())
    {
        lair.Enter(selected, m_pickedCurses);
        PlayBuffer(SOUND_CLICK01);
    }

    // The character went in: the warp follows, the window has done its job.
    if (lair.EnterAnswers() != m_seenEnterAnswers)
    {
        m_seenEnterAnswers = lair.EnterAnswers();
        if (lair.LastEnterSucceeded())
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_SOLO_KALIMA);
            return;
        }
    }

    if (std::chrono::steady_clock::now() - lair.LastRequest() >= RefreshInterval)
        lair.Request();
}

bool mu::ui::window::CSoloKalimaWindow::Update()
{
    HandleClicks();
    SyncRmlModel();
    return true;
}

bool mu::ui::window::CSoloKalimaWindow::Render()
{
    // Nothing native: the frame, the rows and the button are RmlUi. Kept because CObject requires it.
    return true;
}

void mu::ui::window::CSoloKalimaWindow::BindRmlModel(Rml::DataModelConstructor& c, SoloKalimaRmlModel& model)
{
    auto lineType = c.RegisterStruct<SoloKalimaLine>();
    lineType.RegisterMember("text", &SoloKalimaLine::text);
    lineType.RegisterMember("text_px", &SoloKalimaLine::textPx);
    c.Bind("title", &model.title);
    c.Bind("power", &model.power);
    c.Bind("ticket", &model.ticket);
    c.Bind("ticket_ok", &model.ticketOk);
    c.Bind("runs", &model.runs);
    c.Bind("runs_ok", &model.runsOk);
    c.Bind("record", &model.record);
    c.Bind("curses_title", &model.cursesTitle);
    c.Bind("hint", &model.hint);
    c.Bind("hint_bad", &model.hintBad);
    c.Bind("can_enter", &model.canEnter);
    c.Bind("enter_label", &model.enterLabel);

    auto rowType = c.RegisterStruct<SoloKalimaRow>();
    rowType.RegisterMember("tier", &SoloKalimaRow::tier);
    rowType.RegisterMember("title", &SoloKalimaRow::title);
    rowType.RegisterMember("cp", &SoloKalimaRow::cp);
    rowType.RegisterMember("kalima", &SoloKalimaRow::kalima);
    rowType.RegisterMember("detail", &SoloKalimaRow::detail);
    rowType.RegisterMember("open", &SoloKalimaRow::open);
    rowType.RegisterMember("selected", &SoloKalimaRow::selected);
    c.RegisterArray<std::vector<SoloKalimaRow>>();
    c.Bind("tiers", &model.tiers);

    auto curseType = c.RegisterStruct<SoloKalimaCurse>();
    curseType.RegisterMember("index", &SoloKalimaCurse::index);
    curseType.RegisterMember("text", &SoloKalimaCurse::text);
    curseType.RegisterMember("hint", &SoloKalimaCurse::hint);
    curseType.RegisterMember("checked", &SoloKalimaCurse::checked);
    curseType.RegisterMember("locked", &SoloKalimaCurse::locked);
    c.RegisterArray<std::vector<SoloKalimaCurse>>();
    c.Bind("curses", &model.curses);

    c.Bind("exit_tooltip", &model.exitTooltip);

    c.BindEventCallback("solo_kalima_exit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingExit = true; });
    c.BindEventCallback("solo_kalima_enter", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingEnter = true; });
    c.BindEventCallback("solo_kalima_select",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingSelect = args[0].Get<int>(0);
                        });
    c.BindEventCallback("solo_kalima_curse",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingCurse = args[0].Get<int>(-1);
                        });

    wchar_t closeText[256] = {};
    mu_swprintf_s(closeText, I18N::Game::CloseS, L"Esc");
    model.exitTooltip = StringUtils::WideToNarrow(closeText);
}

void mu::ui::window::CSoloKalimaWindow::SyncRmlModel()
{
    m_RmlView.Ensure();
    if (!m_RmlView.Document())
        return;

    // Layer depth UI::RmlBridge::ForegroundPanelLayerDepth: over the HUD and its logs.
    UI::RmlBridge::SyncDocumentVisibilityInFront(m_RmlView.Document(), IsVisible());
    if (!IsVisible())
        return;

    SyncContent();
}

std::wstring mu::ui::window::CSoloKalimaWindow::EnterBlocker(int selected) const
{
    const auto& lair = Lair();
    const auto& status = lair.GetStatus();
    const auto& tiers = lair.Tiers();
    const auto tier = std::find_if(tiers.begin(), tiers.end(), [selected](const auto& t) { return t.Number == selected; });

    if (status.InRun)
        return I18N::Game::SoloKalimaInRun;
    if (!status.Near)
        return I18N::Game::SoloKalimaComeToNpc;
    if (status.RunsLeft <= 0)
        return Printf(I18N::Game::SoloKalimaNoRuns, FormatDuration(SecondsToReset(status)).c_str());
    if (tier == tiers.end())
        return I18N::Game::SoloKalimaPickTier;
    if (!tier->Open && status.Enforced)
        return Printf(I18N::Game::SoloKalimaTierLocked, tier->Number, Thousands(tier->CombatPower).c_str(),
                      Thousands(tier->Health).c_str());
    if (status.Tickets <= 0)
        return I18N::Game::SoloKalimaNoTicket;
    return {};
}

void mu::ui::window::CSoloKalimaWindow::SyncTiers(SoloKalimaRmlModel& updated, int selected) const
{
    const auto& lair = Lair();
    const auto& status = lair.GetStatus();
    for (const auto& tier : lair.Tiers())
    {
        SoloKalimaRow row;
        row.tier = tier.Number;
        row.open = tier.Open || !status.Enforced;
        row.selected = tier.Number == selected;
        const std::wstring title = Printf(I18N::Game::SoloKalimaTierTitle, tier.Number, tier.Name.c_str());
        // Ten tiers since 2026-11-27: a row is one line, "N. Name" with its Combat Power in thousands at the right
        // ("75k", "14.7k"), as in the Tier Maps window (a closed tier is grey, no " · closed"); only the picked tier adds
        // its Kalima line and the detail line (CP, HP, best time), so the list, the record and the curses still stop above the foot.
        const int cpTenths = (tier.CombatPower + 50) / 100;
        row.cp = Line(cpTenths % 10 == 0 ? Printf(L"%dk", cpTenths / 10) : Printf(L"%d.%dk", cpTenths / 10, cpTenths % 10),
                      CP_WIDTH, true);
        row.title = Line(title, ROW_WIDTH - CP_WIDTH - CP_GAP, true);
        if (row.selected)
        {
            row.kalima = Line(Printf(I18N::Game::SoloKalimaTierKalima, tier.Kalima), ROW_WIDTH - SCROLLBAR_ROOM);
            row.detail = Line(Printf(I18N::Game::SoloKalimaTierDetail, Thousands(tier.CombatPower).c_str(),
                                     Thousands(tier.Health).c_str(), BestTime(tier.MyBestSeconds).c_str()),
                              ROW_WIDTH - SCROLLBAR_ROOM);
            updated.record = Line(tier.ServerBestSeconds > 0
                                      ? Printf(I18N::Game::SoloKalimaServerBest, tier.Number, FormatDuration(tier.ServerBestSeconds).c_str(),
                                               tier.ServerBestName.c_str())
                                      : Printf(I18N::Game::SoloKalimaNoServerBest, tier.Number),
                                  CONTENT_WIDTH);
        }
        updated.tiers.push_back(std::move(row));
    }
}

void mu::ui::window::CSoloKalimaWindow::SyncCurses(SoloKalimaRmlModel& updated) const
{
    const auto& lair = Lair();
    const auto& status = lair.GetStatus();
    if (lair.Curses().empty() || status.MaxCurses <= 0)
        return;

    const int picked = static_cast<int>(m_pickedCurses.size());
    updated.cursesTitle = Line(Printf(I18N::Game::SoloKalimaCurses, picked, status.MaxCurses), CONTENT_WIDTH, true);
    int index = 0;
    for (const auto& curse : lair.Curses())
    {
        SoloKalimaCurse row;
        row.index = index++;
        row.checked = IsCursePicked(curse.Key);
        row.locked = !row.checked && picked >= status.MaxCurses;
        const std::wstring text = Printf(I18N::Game::SoloKalimaCurse, curse.Name.c_str(), curse.Description.c_str());
        row.text = Line(text, CURSE_WIDTH - SCROLLBAR_ROOM);
        row.hint = StringUtils::WideToNarrow(text.c_str());
        updated.curses.push_back(std::move(row));
    }
}

void mu::ui::window::CSoloKalimaWindow::SyncContent()
{
    auto& lair = Lair();
    const auto& status = lair.GetStatus();

    SoloKalimaRmlModel updated;
    updated.title = Line(I18N::Game::SoloKalimaTitle, WINDOW_WIDTH, true);

    if (!lair.HasInfo())
    {
        const bool waiting = std::chrono::steady_clock::now() - m_openedAt < AnswerTimeout;
        updated.power = Line(waiting ? I18N::Game::SoloKalimaAsking : I18N::Game::SoloKalimaNoAnswer, CONTENT_WIDTH);
    }
    else
    {
        updated.power = Line(Printf(I18N::Game::SoloKalimaPower, Thousands(status.CombatPower).c_str(), Thousands(status.Health).c_str()),
                             CONTENT_WIDTH, true);
        updated.ticketOk = status.Tickets > 0;
        updated.ticket = Line(status.Tickets > 0 ? Printf(I18N::Game::SoloKalimaTickets, status.Tickets)
                                                 : std::wstring(I18N::Game::SoloKalimaNoTicket),
                              CONTENT_WIDTH);
        updated.runsOk = status.RunsLeft > 0;
        updated.runs = Line(Printf(I18N::Game::SoloKalimaRunsLeft, status.RunsLeft, FormatDuration(SecondsToReset(status)).c_str()),
                            CONTENT_WIDTH);

        const int selected = SelectedTier();
        SyncTiers(updated, selected);
        SyncCurses(updated);

        // The button works when the server would let the character in; otherwise the line above it says why.
        const std::wstring reason = EnterBlocker(selected);
        if (lair.IsEntering())
        {
            updated.hint = Line(I18N::Game::SoloKalimaEntering, CONTENT_WIDTH);
        }
        else if (!reason.empty())
        {
            // A reason the client sees itself is the current one: it goes before an older refusal of the server.
            updated.hint = Line(reason, CONTENT_WIDTH);
            updated.hintBad = true;
        }
        else if (!lair.LastEnterText().empty() && !lair.LastEnterSucceeded())
        {
            // A refusal only the server knows the reason of (busy, a problem with the map), kept until the next Enter.
            updated.hint = Line(lair.LastEnterText(), CONTENT_WIDTH);
            updated.hintBad = true;
        }

        updated.canEnter = reason.empty() && !lair.IsEntering();
        // No tier open and none picked: no number to name.
        updated.enterLabel = selected > 0 ? Line(Printf(I18N::Game::SoloKalimaEnter, selected), ENTER_WIDTH, true)
                                          : Line(I18N::Game::SoloKalimaEnterAny, ENTER_WIDTH, true);
    }

    auto& binder = m_RmlView.Binder();
    SyncFieldFrom(binder, &SoloKalimaRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &SoloKalimaRmlModel::power, "power", updated);
    SyncFieldFrom(binder, &SoloKalimaRmlModel::ticket, "ticket", updated);
    SyncFieldFrom(binder, &SoloKalimaRmlModel::ticketOk, "ticket_ok", updated);
    SyncFieldFrom(binder, &SoloKalimaRmlModel::runs, "runs", updated);
    SyncFieldFrom(binder, &SoloKalimaRmlModel::runsOk, "runs_ok", updated);
    SyncFieldFrom(binder, &SoloKalimaRmlModel::tiers, "tiers", updated);
    SyncFieldFrom(binder, &SoloKalimaRmlModel::record, "record", updated);
    SyncFieldFrom(binder, &SoloKalimaRmlModel::cursesTitle, "curses_title", updated);
    SyncFieldFrom(binder, &SoloKalimaRmlModel::curses, "curses", updated);
    SyncFieldFrom(binder, &SoloKalimaRmlModel::hint, "hint", updated);
    SyncFieldFrom(binder, &SoloKalimaRmlModel::hintBad, "hint_bad", updated);
    SyncFieldFrom(binder, &SoloKalimaRmlModel::canEnter, "can_enter", updated);
    SyncFieldFrom(binder, &SoloKalimaRmlModel::enterLabel, "enter_label", updated);
}
