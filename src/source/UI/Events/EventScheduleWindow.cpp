#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Events/EventScheduleWindow.h"

#include "Audio/DSPlaySound.h"
#include "GameLogic/Events/EventTimes.h"
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
using GameLogic::EventTimes::Phase;
using GameLogic::EventTimes::Times;

namespace
{
// While the window is open, the times are asked for again this often.
constexpr auto RefreshInterval = std::chrono::seconds(60);
// After an event started or ended, the next answer brings its new time; asked for this long after.
constexpr auto AfterChangeDelay = std::chrono::seconds(2);
// Without an answer this long after opening, the server doesn't send event times.
constexpr auto AnswerTimeout = std::chrono::seconds(5);
// An event that starts within this time is marked as soon.
constexpr auto SoonThreshold = std::chrono::minutes(5);

int MeasureInReferenceUnits(const std::wstring& text)
{
    return g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size())).cx;
}

// 1:05:09, 5:09 under an hour, and the days in front of longer times (2d 3:05:09).
std::wstring FormatDuration(std::chrono::seconds left)
{
    long long total = std::max<long long>(0, left.count());
    const long long days = total / 86400;
    total %= 86400;
    const long long hours = total / 3600;
    const long long minutes = total % 3600 / 60;
    const long long seconds = total % 60;

    wchar_t text[48] = {};
    if (days > 0)
        mu_swprintf(text, L"%lldd %lld:%02lld:%02lld", days, hours, minutes, seconds);
    else if (hours > 0)
        mu_swprintf(text, L"%lld:%02lld:%02lld", hours, minutes, seconds);
    else
        mu_swprintf(text, L"%lld:%02lld", minutes, seconds);
    return text;
}

// When it starts on the player's own clock: 18:00, or 24.10 20:45 for an event that isn't daily.
std::wstring FormatLocalTime(std::chrono::system_clock::time_point at, bool withDate)
{
    const std::time_t time = std::chrono::system_clock::to_time_t(at);
    std::tm local = {};
    localtime_s(&local, &time);

    wchar_t text[32] = {};
    if (withDate)
        mu_swprintf(text, L"%02d.%02d %02d:%02d", local.tm_mday, local.tm_mon + 1, local.tm_hour, local.tm_min);
    else
        mu_swprintf(text, L"%02d:%02d", local.tm_hour, local.tm_min);
    return text;
}
} // namespace

mu::ui::window::CEventScheduleWindow::CEventScheduleWindow() = default;

mu::ui::window::CEventScheduleWindow::~CEventScheduleWindow()
{
    Release();
}

bool mu::ui::window::CEventScheduleWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_EVENT_SCHEDULE, this);
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void mu::ui::window::CEventScheduleWindow::Release()
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float mu::ui::window::CEventScheduleWindow::GetLayerDepth()
{
    return LayerDepth;
}

float mu::ui::window::CEventScheduleWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CEventScheduleWindow::OpenningProcess()
{
    m_openedAt = std::chrono::steady_clock::now();
    Times().Request();
}

void mu::ui::window::CEventScheduleWindow::ClosingProcess()
{
}

bool mu::ui::window::CEventScheduleWindow::UpdateMouseEvent()
{
    // The exit button and the corner close are RmlUi's (see Update()); a click on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool mu::ui::window::CEventScheduleWindow::UpdateKeyEvent()
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_EVENT_SCHEDULE))
        return true;

    if (IsPress(VK_ESCAPE))
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_EVENT_SCHEDULE);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    return true;
}

bool mu::ui::window::CEventScheduleWindow::Update()
{
    const bool exit = m_pendingExit;
    m_pendingExit = false;
    if (IsVisible())
    {
        if (exit)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_EVENT_SCHEDULE);
            PlayBuffer(SOUND_CLICK01);
        }
        else
        {
            RefreshIfDue();
        }
    }

    SyncRmlModel();
    return true;
}

bool mu::ui::window::CEventScheduleWindow::Render()
{
    // Nothing native: the frame, the rows and the exit button are RmlUi. Kept because CObject requires it.
    return true;
}

void mu::ui::window::CEventScheduleWindow::RefreshIfDue()
{
    auto& times = Times();
    const auto now = std::chrono::steady_clock::now();
    if (now - times.LastRequest() >= RefreshInterval)
    {
        times.Request();
        return;
    }

    for (const auto& entry : times.Entries())
    {
        if (entry.Deadline + AfterChangeDelay <= now && times.LastRequest() < entry.Deadline + AfterChangeDelay)
        {
            times.Request();
            return;
        }
    }
}

void mu::ui::window::CEventScheduleWindow::BindRmlModel(Rml::DataModelConstructor& c, EventScheduleRmlModel& model)
{
    auto lineType = c.RegisterStruct<EventScheduleLine>();
    lineType.RegisterMember("text", &EventScheduleLine::text);
    lineType.RegisterMember("text_px", &EventScheduleLine::textPx);
    c.Bind("title", &model.title);
    c.Bind("message", &model.message);

    auto rowType = c.RegisterStruct<EventScheduleRow>();
    rowType.RegisterMember("name", &EventScheduleRow::name);
    rowType.RegisterMember("name_px", &EventScheduleRow::nameTextPx);
    rowType.RegisterMember("time", &EventScheduleRow::time);
    rowType.RegisterMember("time_px", &EventScheduleRow::timeTextPx);
    rowType.RegisterMember("active", &EventScheduleRow::active);
    rowType.RegisterMember("soon", &EventScheduleRow::soon);
    c.RegisterArray<std::vector<EventScheduleRow>>();
    c.Bind("rows", &model.rows);
    c.Bind("exit_tooltip", &model.exitTooltip);

    c.BindEventCallback("event_schedule_exit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingExit = true; });

    wchar_t closeText[256] = {};
    mu_swprintf_s(closeText, I18N::Game::CloseS, L"O");
    model.exitTooltip = StringUtils::WideToNarrow(closeText);
}

void mu::ui::window::CEventScheduleWindow::SyncRmlModel()
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

void mu::ui::window::CEventScheduleWindow::SyncContent()
{
    // A line of the window's own text, at the size the native renderer would have shrunk it to for its box.
    auto line = [](const std::wstring& text, int width, bool bold = false) -> EventScheduleLine
    {
        if (text.empty())
            return {};
        g_pRenderText->SetFont(bold ? g_hFontBold : g_hFont);
        const auto role = bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal;
        return {StringUtils::WideToNarrow(text.c_str()),
                UI::RmlBridge::NativeTextPxInBox(role, static_cast<float>(MeasureInReferenceUnits(text)),
                                                 static_cast<float>(width))};
    };

    auto& times = Times();
    const auto now = std::chrono::steady_clock::now();

    EventScheduleLine message;
    if (!times.HasTimes())
    {
        message = line(now - m_openedAt < AnswerTimeout ? I18N::Game::EventScheduleAsking
                                                         : I18N::Game::EventScheduleNoAnswer,
                       CONTENT_WIDTH);
    }
    else if (times.Entries().empty())
    {
        message = line(I18N::Game::EventScheduleNone, CONTENT_WIDTH);
    }

    std::vector<EventScheduleRow> rows;
    for (const auto& entry : times.Entries())
    {
        if (rows.size() >= VISIBLE_ROWS)
            break;

        const auto left = std::chrono::duration_cast<std::chrono::seconds>(entry.Deadline - now);
        const std::wstring duration = FormatDuration(left);
        wchar_t timeText[128] = {};
        switch (entry.State)
        {
        case Phase::Running:
            mu_swprintf(timeText, I18N::Game::EventScheduleRunning, duration.c_str());
            break;
        case Phase::EntranceOpen:
            mu_swprintf(timeText, I18N::Game::EventScheduleEntranceOpen, duration.c_str());
            break;
        default:
            mu_swprintf(timeText, I18N::Game::EventScheduleStartsIn, duration.c_str(),
                        FormatLocalTime(entry.WallDeadline, entry.Weekly).c_str());
            break;
        }

        EventScheduleRow row;
        const auto name = line(entry.Name, CONTENT_WIDTH, true);
        row.name = name.text;
        row.nameTextPx = name.textPx;
        const auto time = line(timeText, CONTENT_WIDTH);
        row.time = time.text;
        row.timeTextPx = time.textPx;
        row.active = entry.State != Phase::Next;
        row.soon = entry.State == Phase::Next && left <= SoonThreshold;
        rows.push_back(std::move(row));
    }

    EventScheduleRmlModel& model = m_RmlView.GetModel();
    SyncField(m_RmlView.Binder(), &EventScheduleRmlModel::title, "title",
              line(I18N::Game::EventScheduleTitle, WINDOW_WIDTH, true));
    SyncField(m_RmlView.Binder(), &EventScheduleRmlModel::message, "message", std::move(message));
    if (model.rows != rows)
    {
        model.rows = std::move(rows);
        m_RmlView.MarkDirty("rows");
    }
}
