#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Events/TierMapsWindow.h"

#include "Audio/DSPlaySound.h"
#include "GameLogic/Events/TierMaps.h"
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
using GameLogic::TierMaps::Maps;

namespace
{
// While the window is open, the state is asked for again this often: members come and go, runs get used.
constexpr auto RefreshInterval = std::chrono::seconds(5);
// Without an answer this long after opening, the server doesn't send the Tier Maps.
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
} // namespace

mu::ui::window::CTierMapsWindow::CTierMapsWindow() = default;

mu::ui::window::CTierMapsWindow::~CTierMapsWindow()
{
    Release();
}

bool mu::ui::window::CTierMapsWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_TIER_MAPS, this);
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void mu::ui::window::CTierMapsWindow::Release()
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float mu::ui::window::CTierMapsWindow::GetLayerDepth()
{
    return LayerDepth;
}

float mu::ui::window::CTierMapsWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CTierMapsWindow::OpenningProcess()
{
    m_openedAt = std::chrono::steady_clock::now();
    m_seenEnterAnswers = Maps().EnterAnswers();
    Maps().Request();
}

void mu::ui::window::CTierMapsWindow::ClosingProcess()
{
}

bool mu::ui::window::CTierMapsWindow::UpdateMouseEvent()
{
    // The rows, the button and the corner close are RmlUi's (see Update()); a click on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool mu::ui::window::CTierMapsWindow::UpdateKeyEvent()
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_TIER_MAPS))
        return true;

    if (IsPress(VK_ESCAPE))
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_TIER_MAPS);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    return true;
}

int mu::ui::window::CTierMapsWindow::SelectedTier() const
{
    const auto& tiers = Maps().Tiers();
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

bool mu::ui::window::CTierMapsWindow::Update()
{
    const bool exit = m_pendingExit;
    const bool enter = m_pendingEnter;
    const int select = m_pendingSelect;
    m_pendingExit = false;
    m_pendingEnter = false;
    m_pendingSelect = 0;
    if (IsVisible())
    {
        auto& maps = Maps();
        if (exit)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_TIER_MAPS);
            PlayBuffer(SOUND_CLICK01);
        }
        else
        {
            if (select > 0)
            {
                m_selectedTier = select;
                PlayBuffer(SOUND_CLICK01);
            }

            if (enter && SelectedTier() > 0)
            {
                maps.Enter(SelectedTier());
                PlayBuffer(SOUND_CLICK01);
            }

            // The party went in: the warp follows, the window has done its job.
            if (maps.EnterAnswers() != m_seenEnterAnswers)
            {
                m_seenEnterAnswers = maps.EnterAnswers();
                if (maps.LastEnterSucceeded())
                {
                    g_pNewUISystem->Hide(mu::ui::window::INTERFACE_TIER_MAPS);
                    return true;
                }
            }

            if (std::chrono::steady_clock::now() - maps.LastRequest() >= RefreshInterval)
                maps.Request();
        }
    }

    SyncRmlModel();
    return true;
}

bool mu::ui::window::CTierMapsWindow::Render()
{
    // Nothing native: the frame, the rows and the button are RmlUi. Kept because CObject requires it.
    return true;
}

void mu::ui::window::CTierMapsWindow::BindRmlModel(Rml::DataModelConstructor& c, TierMapsRmlModel& model)
{
    auto lineType = c.RegisterStruct<TierMapsLine>();
    lineType.RegisterMember("text", &TierMapsLine::text);
    lineType.RegisterMember("text_px", &TierMapsLine::textPx);
    c.Bind("title", &model.title);
    c.Bind("today", &model.today);
    c.Bind("power", &model.power);
    c.Bind("ticket", &model.ticket);
    c.Bind("ticket_ok", &model.ticketOk);
    c.Bind("reset", &model.reset);
    c.Bind("members_title", &model.membersTitle);
    c.Bind("hint", &model.hint);
    c.Bind("hint_bad", &model.hintBad);
    c.Bind("can_enter", &model.canEnter);
    c.Bind("enter_label", &model.enterLabel);

    auto rowType = c.RegisterStruct<TierMapsRow>();
    rowType.RegisterMember("tier", &TierMapsRow::tier);
    rowType.RegisterMember("title", &TierMapsRow::title);
    rowType.RegisterMember("detail", &TierMapsRow::detail);
    rowType.RegisterMember("open", &TierMapsRow::open);
    rowType.RegisterMember("selected", &TierMapsRow::selected);
    c.RegisterArray<std::vector<TierMapsRow>>();
    c.Bind("tiers", &model.tiers);

    auto memberType = c.RegisterStruct<TierMapsMember>();
    memberType.RegisterMember("text", &TierMapsMember::text);
    memberType.RegisterMember("ready", &TierMapsMember::ready);
    c.RegisterArray<std::vector<TierMapsMember>>();
    c.Bind("members", &model.members);

    c.Bind("exit_tooltip", &model.exitTooltip);

    c.BindEventCallback("tier_maps_exit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingExit = true; });
    c.BindEventCallback("tier_maps_enter", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingEnter = true; });
    c.BindEventCallback("tier_maps_select",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingSelect = args[0].Get<int>(0);
                        });

    wchar_t closeText[256] = {};
    mu_swprintf_s(closeText, I18N::Game::CloseS, L"Esc");
    model.exitTooltip = StringUtils::WideToNarrow(closeText);
}

void mu::ui::window::CTierMapsWindow::SyncRmlModel()
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

void mu::ui::window::CTierMapsWindow::SyncContent()
{
    // A line of the window's own text, at the size the native renderer would have shrunk it to for its box.
    auto line = [](const std::wstring& text, int width, bool bold = false) -> TierMapsLine
    {
        if (text.empty())
            return {};
        g_pRenderText->SetFont(bold ? g_hFontBold : g_hFont);
        const auto role = bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal;
        return {StringUtils::WideToNarrow(text.c_str()),
                UI::RmlBridge::NativeTextPxInBox(role, static_cast<float>(MeasureInReferenceUnits(text)),
                                                 static_cast<float>(width))};
    };

    auto& maps = Maps();
    const auto& status = maps.GetStatus();
    const auto now = std::chrono::steady_clock::now();

    TierMapsRmlModel updated;
    updated.title = line(I18N::Game::TierMapsTitle, WINDOW_WIDTH, true);

    if (!maps.HasInfo())
    {
        updated.today = line(now - m_openedAt < AnswerTimeout ? I18N::Game::TierMapsAsking : I18N::Game::TierMapsNoAnswer,
                             CONTENT_WIDTH);
    }
    else
    {
        updated.today = line(Printf(I18N::Game::TierMapsToday, status.Layout.c_str(), status.Boss.c_str()), CONTENT_WIDTH, true);
        updated.power = line(status.PartySize > 1
                                 ? Printf(I18N::Game::TierMapsPartyPower, Thousands(status.AverageCombatPower).c_str(),
                                          Thousands(status.LowestCombatPower).c_str())
                                 : Printf(I18N::Game::TierMapsOwnPower, Thousands(status.AverageCombatPower).c_str()),
                             CONTENT_WIDTH);
        updated.ticketOk = status.Ticket;
        updated.ticket = line(status.Ticket ? I18N::Game::TierMapsTicketReady : I18N::Game::TierMapsTicketMissing, CONTENT_WIDTH);
        const long long toReset = status.SecondsToReset
            - std::chrono::duration_cast<std::chrono::seconds>(now - status.ReceivedAt).count();
        updated.reset = line(Printf(I18N::Game::TierMapsReset, status.RunsPerDay, FormatDuration(toReset).c_str()), CONTENT_WIDTH);

        const int selected = SelectedTier();
        for (const auto& tier : maps.Tiers())
        {
            TierMapsRow row;
            row.tier = tier.Number;
            row.open = tier.Open;
            row.selected = tier.Number == selected;
            row.title = line(Printf(I18N::Game::TierMapsTierTitle, tier.Number, tier.Name.c_str()), ROW_WIDTH, true);
            const std::wstring record = tier.RecordSeconds > 0
                ? Printf(I18N::Game::TierMapsRecord, FormatDuration(tier.RecordSeconds).c_str())
                : std::wstring(I18N::Game::TierMapsNoRecord);
            row.detail = line(Printf(I18N::Game::TierMapsTierDetail, Thousands(tier.CombatPower).c_str(), tier.Boss.c_str(), record.c_str()),
                              ROW_WIDTH);
            updated.tiers.push_back(std::move(row));
        }

        updated.membersTitle = line(I18N::Game::TierMapsMembers, CONTENT_WIDTH, true);
        const auto selectedTier = std::find_if(maps.Tiers().begin(), maps.Tiers().end(),
                                               [selected](const auto& t) { return t.Number == selected; });
        const int memberMinimum = selectedTier == maps.Tiers().end()
            ? 0
            : (selectedTier->CombatPower * status.MemberSharePercent + 99) / 100;
        bool everyoneReady = true;
        for (const auto& member : maps.Members())
        {
            TierMapsMember row;
            const bool strongEnough = !status.Enforced || member.CombatPower >= memberMinimum;
            row.ready = member.Near && member.RunsLeft > 0 && strongEnough;
            everyoneReady = everyoneReady && row.ready;
            row.text = line(member.Near
                                ? Printf(I18N::Game::TierMapsMember, member.Name.c_str(), Thousands(member.CombatPower).c_str(), member.RunsLeft)
                                : Printf(I18N::Game::TierMapsMemberAway, member.Name.c_str()),
                            CONTENT_WIDTH);
            updated.members.push_back(std::move(row));
        }

        // The button works when the server would let the party in; otherwise the line under it says why.
        const bool tierOpen = selectedTier != maps.Tiers().end() && (selectedTier->Open || !status.Enforced);
        const wchar_t* reason = nullptr;
        if (status.InRun)
            reason = I18N::Game::TierMapsInRun;
        else if (!status.Near)
            reason = I18N::Game::TierMapsComeToNpc;
        else if (!status.Leader)
            reason = I18N::Game::TierMapsLeaderOnly;
        else if (selected == 0 || !tierOpen)
            reason = I18N::Game::TierMapsPickTier;
        else if (!status.Ticket)
            reason = I18N::Game::TierMapsTicketMissing;
        else if (!everyoneReady)
            reason = I18N::Game::TierMapsNotReady;

        if (maps.IsEntering())
        {
            updated.hint = line(I18N::Game::TierMapsEntering, CONTENT_WIDTH);
        }
        else if (!maps.LastEnterText().empty() && !maps.LastEnterSucceeded())
        {
            updated.hint = line(maps.LastEnterText(), CONTENT_WIDTH);
            updated.hintBad = true;
        }
        else if (reason != nullptr)
        {
            updated.hint = line(reason, CONTENT_WIDTH);
            updated.hintBad = true;
        }

        updated.canEnter = reason == nullptr && !maps.IsEntering();
        updated.enterLabel = line(Printf(I18N::Game::TierMapsEnter, std::max(1, selected)), ROW_WIDTH, true);
    }

    auto& binder = m_RmlView.Binder();
    SyncFieldFrom(binder, &TierMapsRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &TierMapsRmlModel::today, "today", updated);
    SyncFieldFrom(binder, &TierMapsRmlModel::power, "power", updated);
    SyncFieldFrom(binder, &TierMapsRmlModel::ticket, "ticket", updated);
    SyncFieldFrom(binder, &TierMapsRmlModel::ticketOk, "ticket_ok", updated);
    SyncFieldFrom(binder, &TierMapsRmlModel::reset, "reset", updated);
    SyncFieldFrom(binder, &TierMapsRmlModel::tiers, "tiers", updated);
    SyncFieldFrom(binder, &TierMapsRmlModel::membersTitle, "members_title", updated);
    SyncFieldFrom(binder, &TierMapsRmlModel::members, "members", updated);
    SyncFieldFrom(binder, &TierMapsRmlModel::hint, "hint", updated);
    SyncFieldFrom(binder, &TierMapsRmlModel::hintBad, "hint_bad", updated);
    SyncFieldFrom(binder, &TierMapsRmlModel::canEnter, "can_enter", updated);
    SyncFieldFrom(binder, &TierMapsRmlModel::enterLabel, "enter_label", updated);
}
