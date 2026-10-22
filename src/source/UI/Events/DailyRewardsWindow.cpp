#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Events/DailyRewardsWindow.h"

#include "Audio/DSPlaySound.h"
#include "GameLogic/Events/DailyRewards.h"
#include "UI/Core/WindowSystem.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/Text/CUIRenderText.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlSyncField.h"

#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <ctime>

using namespace SEASON3B;
using namespace mu::ui::window;
using GameLogic::DailyRewards::Rewards;

namespace
{
// While the window is open, the state is asked for again this often.
constexpr auto RefreshInterval = std::chrono::seconds(30);
// After the take button, the server's answer to it is asked for this long after.
constexpr auto AfterTakeDelay = std::chrono::seconds(2);
// When today should have counted, or a new UTC day began, asked again at most this often until it shows.
constexpr auto CatchUpInterval = std::chrono::seconds(10);
// The server counts the online time once a minute, so today counts up to a minute after the time is reached; it is
// asked for from this long after.
constexpr int CountedCheckAfterSeconds = 20;
// Without an answer this long after opening, the server doesn't send daily rewards.
constexpr auto AnswerTimeout = std::chrono::seconds(5);

int MeasureInReferenceUnits(const std::wstring& text)
{
    return g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size())).cx;
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

// The UTC day a time falls on, as days since the epoch.
long long UtcDay(std::chrono::system_clock::time_point at)
{
    return std::chrono::duration_cast<std::chrono::seconds>(at.time_since_epoch()).count() / 86400;
}

// Seconds until the next UTC day begins, when the next daily reward can count.
long long SecondsToNextUtcDay(std::chrono::system_clock::time_point now)
{
    const long long seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return 86400 - seconds % 86400;
}

long long SecondsSince(std::chrono::steady_clock::time_point then, std::chrono::steady_clock::time_point now)
{
    return std::chrono::duration_cast<std::chrono::seconds>(now - then).count();
}

// "Jewel of Chaos x2, Jewel of Life" -> its items.
std::vector<std::wstring> SplitItems(const std::wstring& reward)
{
    std::vector<std::wstring> items;
    size_t start = 0;
    while (start <= reward.size())
    {
        const size_t comma = reward.find(L", ", start);
        items.push_back(reward.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start));
        if (comma == std::wstring::npos)
            break;
        start = comma + 2;
    }
    return items;
}

std::wstring Format(const wchar_t* format, int value)
{
    wchar_t text[256] = {};
    mu_swprintf(text, format, value);
    return text;
}
} // namespace

mu::ui::window::CDailyRewardsWindow::CDailyRewardsWindow() = default;

mu::ui::window::CDailyRewardsWindow::~CDailyRewardsWindow()
{
    Release();
}

bool mu::ui::window::CDailyRewardsWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_DAILY_REWARDS, this);
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void mu::ui::window::CDailyRewardsWindow::Release()
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float mu::ui::window::CDailyRewardsWindow::GetLayerDepth()
{
    return LayerDepth;
}

float mu::ui::window::CDailyRewardsWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CDailyRewardsWindow::OpenningProcess()
{
    m_openedAt = std::chrono::steady_clock::now();
    m_refreshDue = {};
    Rewards().Request();
}

void mu::ui::window::CDailyRewardsWindow::ClosingProcess()
{
}

bool mu::ui::window::CDailyRewardsWindow::UpdateMouseEvent()
{
    // The buttons and the corner close are RmlUi's (see Update()); a click on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool mu::ui::window::CDailyRewardsWindow::UpdateKeyEvent()
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_DAILY_REWARDS))
        return true;

    if (IsPress(VK_ESCAPE))
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_DAILY_REWARDS);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    return true;
}

bool mu::ui::window::CDailyRewardsWindow::Update()
{
    const bool exit = m_pendingExit;
    const bool take = m_pendingTake;
    m_pendingExit = false;
    m_pendingTake = false;
    if (IsVisible())
    {
        if (exit)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_DAILY_REWARDS);
            PlayBuffer(SOUND_CLICK01);
        }
        else
        {
            if (take)
            {
                Rewards().TakeWaiting();
                PlayBuffer(SOUND_CLICK01);
                m_refreshDue = std::chrono::steady_clock::now() + AfterTakeDelay;
            }

            RefreshIfDue();
        }
    }

    SyncRmlModel();
    return true;
}

bool mu::ui::window::CDailyRewardsWindow::Render()
{
    // Nothing native: the frame, the table and the buttons are RmlUi. Kept because CObject requires it.
    return true;
}

void mu::ui::window::CDailyRewardsWindow::RefreshIfDue()
{
    auto& rewards = Rewards();
    const auto now = std::chrono::steady_clock::now();

    // A refresh asked for: the store drops requests that follow each other too closely, so it is asked until
    // one went out after the time it was due.
    if (m_refreshDue != std::chrono::steady_clock::time_point{})
    {
        if (now < m_refreshDue)
            return;
        if (rewards.LastRequest() < m_refreshDue)
            rewards.Request();
        if (rewards.LastRequest() >= m_refreshDue)
            m_refreshDue = {};
        return;
    }

    if (now - rewards.LastRequest() >= RefreshInterval)
    {
        rewards.Request();
        return;
    }

    if (!rewards.HasInfo() || now - rewards.LastRequest() < CatchUpInterval)
        return;

    const auto& status = rewards.GetStatus();
    const bool newUtcDay = UtcDay(std::chrono::system_clock::now()) != UtcDay(status.ReceivedWall);
    const bool shouldHaveCounted = !status.CountedToday
        && status.SecondsToday + SecondsSince(status.ReceivedAt, now) >= status.SecondsNeeded + CountedCheckAfterSeconds;
    if (newUtcDay || shouldHaveCounted)
        rewards.Request();
}

void mu::ui::window::CDailyRewardsWindow::BindRmlModel(Rml::DataModelConstructor& c, DailyRewardsRmlModel& model)
{
    auto lineType = c.RegisterStruct<DailyRewardsLine>();
    lineType.RegisterMember("text", &DailyRewardsLine::text);
    lineType.RegisterMember("text_px", &DailyRewardsLine::textPx);
    c.RegisterArray<std::vector<DailyRewardsLine>>();
    c.Bind("title", &model.title);
    c.Bind("status", &model.status);
    c.Bind("detail", &model.detail);
    c.Bind("footer", &model.footer);
    c.Bind("can_take", &model.canTake);
    c.Bind("take_label", &model.takeLabel);

    auto dayType = c.RegisterStruct<DailyRewardsDay>();
    dayType.RegisterMember("label", &DailyRewardsDay::label);
    dayType.RegisterMember("items", &DailyRewardsDay::items);
    dayType.RegisterMember("done", &DailyRewardsDay::done);
    dayType.RegisterMember("today", &DailyRewardsDay::today);
    c.RegisterArray<std::vector<DailyRewardsDay>>();
    c.Bind("days", &model.days);

    c.Bind("exit_tooltip", &model.exitTooltip);

    c.BindEventCallback("daily_rewards_exit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingExit = true; });
    c.BindEventCallback("daily_rewards_take", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingTake = true; });

    wchar_t closeText[256] = {};
    mu_swprintf_s(closeText, I18N::Game::CloseS, L"N");
    model.exitTooltip = StringUtils::WideToNarrow(closeText);
}

void mu::ui::window::CDailyRewardsWindow::SyncRmlModel()
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

void mu::ui::window::CDailyRewardsWindow::SyncContent()
{
    // A line of the window's own text, at the size the native renderer would have shrunk it to for its box.
    auto line = [](const std::wstring& text, int width, bool bold = false) -> DailyRewardsLine
    {
        if (text.empty())
            return {};
        g_pRenderText->SetFont(bold ? g_hFontBold : g_hFont);
        const auto role = bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal;
        return {StringUtils::WideToNarrow(text.c_str()),
                UI::RmlBridge::NativeTextPxInBox(role, static_cast<float>(MeasureInReferenceUnits(text)),
                                                 static_cast<float>(width))};
    };

    auto& rewards = Rewards();
    const auto& status = rewards.GetStatus();
    const auto now = std::chrono::steady_clock::now();

    DailyRewardsRmlModel updated;
    updated.title = line(I18N::Game::DailyRewardsTitle, WINDOW_WIDTH, true);

    if (!rewards.HasInfo())
    {
        updated.status = line(now - m_openedAt < AnswerTimeout ? I18N::Game::DailyRewardsAsking
                                                                : I18N::Game::DailyRewardsNoAnswer,
                              CONTENT_WIDTH);
    }
    else
    {
        const int days = std::max<int>(1, status.DaysInCycle);
        // Days of this cycle that counted, and the day that counts today (or did): after the last day of a cycle,
        // a day that hasn't counted yet starts the next one.
        const int completed = status.CountedToday ? status.StreakDay : status.StreakDay % days;
        const int todayDay = status.CountedToday ? status.StreakDay : completed + 1;

        wchar_t text[256] = {};
        if (status.CountedToday)
        {
            mu_swprintf(text, I18N::Game::DailyRewardsReceived, todayDay);
            updated.status = line(text, CONTENT_WIDTH, true);
            mu_swprintf(text, I18N::Game::DailyRewardsNextIn,
                        FormatDuration(SecondsToNextUtcDay(std::chrono::system_clock::now())).c_str());
            updated.detail = line(text, CONTENT_WIDTH);
        }
        else
        {
            const long long seconds = status.SecondsToday + SecondsSince(status.ReceivedAt, now);
            const int needed = std::max(1, status.SecondsNeeded / 60);
            const int played = static_cast<int>(std::min<long long>(seconds / 60, needed));
            mu_swprintf(text, I18N::Game::DailyRewardsPlayed, todayDay, played, needed);
            updated.status = line(text, CONTENT_WIDTH, true);
            if (seconds >= status.SecondsNeeded)
            {
                updated.detail = line(I18N::Game::DailyRewardsComing, CONTENT_WIDTH);
            }
            else
            {
                const int left = static_cast<int>((status.SecondsNeeded - seconds + 59) / 60);
                updated.detail = line(Format(I18N::Game::DailyRewardsLeft, left), CONTENT_WIDTH);
            }
        }

        for (const auto& day : rewards.Days())
        {
            DailyRewardsDay row;
            row.label = line(Format(I18N::Game::DailyRewardsDay, day.Number), LABEL_WIDTH, true);
            for (const auto& item : SplitItems(day.Reward))
                row.items.push_back(line(item, ITEMS_WIDTH));
            row.done = day.Number <= completed;
            row.today = day.Number == todayDay;
            updated.days.push_back(std::move(row));
        }

        if (status.Waiting > 0)
        {
            updated.footer = line(Format(I18N::Game::DailyRewardsWaiting, status.Waiting), CONTENT_WIDTH, true);
            updated.canTake = true;
            updated.takeLabel = line(I18N::Game::DailyRewardsTake, TABLE_WIDTH, true);
        }
        else
        {
            updated.footer = line(I18N::Game::DailyRewardsHint, CONTENT_WIDTH);
        }
    }

    auto& binder = m_RmlView.Binder();
    SyncFieldFrom(binder, &DailyRewardsRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &DailyRewardsRmlModel::status, "status", updated);
    SyncFieldFrom(binder, &DailyRewardsRmlModel::detail, "detail", updated);
    SyncFieldFrom(binder, &DailyRewardsRmlModel::days, "days", updated);
    SyncFieldFrom(binder, &DailyRewardsRmlModel::footer, "footer", updated);
    SyncFieldFrom(binder, &DailyRewardsRmlModel::canTake, "can_take", updated);
    SyncFieldFrom(binder, &DailyRewardsRmlModel::takeLabel, "take_label", updated);
}
