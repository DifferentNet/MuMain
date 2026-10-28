#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Events/EndlessTowerWindow.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Events/EndlessTower.h"
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
using GameLogic::EndlessTower::Tower;

namespace
{
// While the window is open, the state is asked for again this often: members come and go, runs get used.
constexpr auto RefreshInterval = std::chrono::seconds(5);
// Without an answer this long after opening, the server doesn't send the Endless Tower.
constexpr auto AnswerTimeout = std::chrono::seconds(5);
// How far a line that continues the one above is indented (reference px, as .et-row.more in the theme).
constexpr int ContinuationIndent = 8;

enum eROW_KIND
{
    ROW_PLAIN = 0,
    ROW_TITLE = 1,
    ROW_GOOD = 2,
    ROW_BAD = 3,
    ROW_NAME = 4,
    ROW_MORE = 5,
};

int MeasureInReferenceUnits(const std::wstring& text)
{
    return g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size())).cx;
}

// 5000000 -> "5,000,000".
std::wstring Thousands(long long value)
{
    std::wstring digits = std::to_wstring(value < 0 ? -value : value);
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

void ReplaceAll(std::wstring& text, std::wstring_view from, std::wstring_view to)
{
    for (size_t at = text.find(from); at != std::wstring::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
}

// The server's item names, shorter for the narrow window: "Box of Kundun +4" -> "Box +4", "Jewel of Bless" -> "Bless".
std::wstring ShortRewardText(std::wstring text)
{
    ReplaceAll(text, L"Box of Kundun ", L"Box ");
    ReplaceAll(text, L"Jewel of ", L"");
    return text;
}

// "a, b, c" -> {"a", "b", "c"}.
std::vector<std::wstring> SplitItems(const std::wstring& text)
{
    std::vector<std::wstring> items;
    size_t start = 0;
    while (true)
    {
        const size_t comma = text.find(L", ", start);
        items.push_back(text.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start));
        if (comma == std::wstring::npos)
            break;
        start = comma + 2;
    }
    return items;
}
} // namespace

mu::ui::window::CEndlessTowerWindow::CEndlessTowerWindow() = default;

mu::ui::window::CEndlessTowerWindow::~CEndlessTowerWindow()
{
    Release();
}

bool mu::ui::window::CEndlessTowerWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_ENDLESS_TOWER, this);
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void mu::ui::window::CEndlessTowerWindow::Release()
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float mu::ui::window::CEndlessTowerWindow::GetLayerDepth()
{
    return LayerDepth;
}

float mu::ui::window::CEndlessTowerWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CEndlessTowerWindow::OpenningProcess()
{
    m_openedAt = std::chrono::steady_clock::now();
    m_lastRefresh = m_openedAt;
    m_seenEnterAnswers = Tower().EnterAnswers();
    Tower().Request();
}

void mu::ui::window::CEndlessTowerWindow::ClosingProcess()
{
}

bool mu::ui::window::CEndlessTowerWindow::UpdateMouseEvent()
{
    // The tabs, the button and the corner close are RmlUi's (see Update()); a click on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool mu::ui::window::CEndlessTowerWindow::UpdateKeyEvent()
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_ENDLESS_TOWER))
        return true;

    if (IsPress(VK_ESCAPE))
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_ENDLESS_TOWER);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    return true;
}

bool mu::ui::window::CEndlessTowerWindow::Update()
{
    const bool exit = m_pendingExit;
    const bool enter = m_pendingEnter;
    const int tab = m_pendingTab;
    m_pendingExit = false;
    m_pendingEnter = false;
    m_pendingTab = -1;
    if (IsVisible())
    {
        auto& tower = Tower();
        if (exit)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_ENDLESS_TOWER);
            PlayBuffer(SOUND_CLICK01);
        }
        else
        {
            if (tab >= 0 && tab < TAB_COUNT && tab != m_tab)
            {
                m_tab = tab;
                PlayBuffer(SOUND_CLICK01);
            }

            if (enter)
            {
                tower.Enter();
                PlayBuffer(SOUND_CLICK01);
            }

            // The party went in: the warp follows, the window has done its job.
            if (tower.EnterAnswers() != m_seenEnterAnswers)
            {
                m_seenEnterAnswers = tower.EnterAnswers();
                if (tower.LastEnterSucceeded())
                {
                    g_pNewUISystem->Hide(mu::ui::window::INTERFACE_ENDLESS_TOWER);
                    return true;
                }
            }

            const auto now = std::chrono::steady_clock::now();
            if (now - m_lastRefresh >= RefreshInterval)
            {
                m_lastRefresh = now;
                tower.Request();
            }
        }
    }

    SyncRmlModel();
    return true;
}

bool mu::ui::window::CEndlessTowerWindow::Render()
{
    // Nothing native: the frame, the lines and the button are RmlUi. Kept because CObject requires it.
    return true;
}

void mu::ui::window::CEndlessTowerWindow::BindRmlModel(Rml::DataModelConstructor& c, EndlessTowerRmlModel& model)
{
    auto lineType = c.RegisterStruct<EndlessTowerLine>();
    lineType.RegisterMember("text", &EndlessTowerLine::text);
    lineType.RegisterMember("text_px", &EndlessTowerLine::textPx);
    c.Bind("title", &model.title);
    c.Bind("entry", &model.entry);
    c.Bind("party", &model.party);
    c.Bind("party_ok", &model.partyOk);
    c.Bind("reset", &model.reset);
    c.Bind("best", &model.best);
    c.RegisterArray<std::vector<EndlessTowerLine>>();
    c.Bind("hint", &model.hint);
    c.Bind("hint_bad", &model.hintBad);
    c.Bind("can_enter", &model.canEnter);
    c.Bind("enter_label", &model.enterLabel);

    auto tabType = c.RegisterStruct<EndlessTowerTab>();
    tabType.RegisterMember("tab", &EndlessTowerTab::tab);
    tabType.RegisterMember("label", &EndlessTowerTab::label);
    tabType.RegisterMember("selected", &EndlessTowerTab::selected);
    c.RegisterArray<std::vector<EndlessTowerTab>>();
    c.Bind("tabs", &model.tabs);

    auto rowType = c.RegisterStruct<EndlessTowerRow>();
    rowType.RegisterMember("text", &EndlessTowerRow::text);
    rowType.RegisterMember("kind", &EndlessTowerRow::kind);
    c.RegisterArray<std::vector<EndlessTowerRow>>();
    c.Bind("rows", &model.rows);

    c.Bind("exit_tooltip", &model.exitTooltip);

    c.BindEventCallback("endless_tower_exit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingExit = true; });
    c.BindEventCallback("endless_tower_enter", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingEnter = true; });
    c.BindEventCallback("endless_tower_tab",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingTab = args[0].Get<int>(-1);
                        });

    wchar_t closeText[256] = {};
    mu_swprintf_s(closeText, I18N::Game::CloseS, L"Esc");
    model.exitTooltip = StringUtils::WideToNarrow(closeText);
}

void mu::ui::window::CEndlessTowerWindow::SyncRmlModel()
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

void mu::ui::window::CEndlessTowerWindow::SyncContent()
{
    // A line of the window's own text, at the size the native renderer would have shrunk it to for its box.
    auto line = [](const std::wstring& text, int width, bool bold = false) -> EndlessTowerLine
    {
        if (text.empty())
            return {};
        g_pRenderText->SetFont(bold ? g_hFontBold : g_hFont);
        const auto role = bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal;
        return {StringUtils::WideToNarrow(text.c_str()),
                UI::RmlBridge::NativeTextPxInBox(role, static_cast<float>(MeasureInReferenceUnits(text)),
                                                 static_cast<float>(width))};
    };

    auto& tower = Tower();
    const auto& status = tower.GetStatus();
    const auto now = std::chrono::steady_clock::now();

    EndlessTowerRmlModel updated;
    updated.title = line(I18N::Game::EndlessTowerTitle, WINDOW_WIDTH, true);

    const wchar_t* tabLabels[TAB_COUNT] = {I18N::Game::EndlessTowerTabParty, I18N::Game::EndlessTowerTabBosses,
                                           I18N::Game::EndlessTowerTabRewards, I18N::Game::EndlessTowerTabTop};
    for (int i = 0; i < TAB_COUNT; ++i)
        updated.tabs.push_back({i, line(tabLabels[i], TAB_WIDTH, true), i == m_tab});

    auto addRow = [&](const std::wstring& text, int kind)
    {
        const bool bold = kind == ROW_TITLE || kind == ROW_NAME;
        const int width = kind == ROW_MORE ? ROW_WIDTH - ContinuationIndent : ROW_WIDTH;
        updated.rows.push_back({line(text, width, bold), kind});
    };

    // The line above the button word by word on lines of the content's width, 3 at most: the server's refusals
    // ("Everyone has to stand at Lugard: ancientElf isn't here.") are longer than one line.
    auto hintLines = [&](const std::wstring& text)
    {
        constexpr size_t MaxHintLines = 3;
        g_pRenderText->SetFont(g_hFont);
        std::vector<std::wstring> lines;
        std::wstring current;
        size_t start = 0;
        while (start < text.size())
        {
            size_t end = text.find(L' ', start);
            if (end == std::wstring::npos)
                end = text.size();
            const std::wstring word = text.substr(start, end - start);
            start = end + 1;
            if (word.empty())
                continue;
            const std::wstring candidate = current.empty() ? word : current + L" " + word;
            if (!current.empty() && MeasureInReferenceUnits(candidate) > CONTENT_WIDTH && lines.size() + 1 < MaxHintLines)
            {
                lines.push_back(current);
                current = word;
            }
            else
            {
                current = candidate;
            }
        }
        if (!current.empty())
            lines.push_back(current);
        for (const auto& part : lines)
            updated.hint.push_back(line(part, CONTENT_WIDTH));
    };

    // "Wave 80+: Bless x6, Soul x5, Life x3, Creation x2" on as many lines as it needs, the items whole.
    auto addReward = [&](int fromWave, const std::wstring& text)
    {
        g_pRenderText->SetFont(g_hFont);
        std::wstring current = Printf(I18N::Game::EndlessTowerFromWave, fromWave, L"");
        bool first = true;
        bool empty = true;
        for (const auto& item : SplitItems(ShortRewardText(text)))
        {
            const std::wstring candidate = empty ? current + item : current + L", " + item;
            const int width = first ? ROW_WIDTH : ROW_WIDTH - ContinuationIndent;
            if (!empty && MeasureInReferenceUnits(candidate) > width)
            {
                addRow(current + L",", first ? ROW_PLAIN : ROW_MORE);
                first = false;
                current = item;
            }
            else
            {
                current = candidate;
            }
            empty = false;
        }
        addRow(current, first ? ROW_PLAIN : ROW_MORE);
    };

    if (!tower.HasInfo())
    {
        updated.entry = line(now - m_openedAt < AnswerTimeout ? I18N::Game::EndlessTowerAsking : I18N::Game::EndlessTowerNoAnswer,
                             CONTENT_WIDTH);
    }
    else
    {
        const std::wstring price = Thousands(status.Price);
        updated.entry = line(Printf(I18N::Game::EndlessTowerEntry, price.c_str(), status.RunsPerDay), CONTENT_WIDTH, true);
        updated.partyOk = status.PartySize >= status.MinimumParty && status.PartySize <= status.MaximumParty;
        updated.party = line(Printf(I18N::Game::EndlessTowerParty, status.PartySize, status.MinimumParty, status.MaximumParty),
                             CONTENT_WIDTH);
        const long long toReset = status.SecondsToReset
            - std::chrono::duration_cast<std::chrono::seconds>(now - status.ReceivedAt).count();
        updated.reset = line(Printf(I18N::Game::EndlessTowerReset, FormatDuration(toReset).c_str()), CONTENT_WIDTH);

        int ownBest = 0;
        for (const auto& member : tower.Members())
        {
            if (Hero != nullptr && wcscmp(member.Name.c_str(), Hero->ID) == 0)
                ownBest = member.BestHeight;
        }
        const int topHeight = tower.Top().empty() ? 0 : tower.Top().front().Height;
        const std::wstring ownText = ownBest > 0 ? Printf(I18N::Game::EndlessTowerWave, ownBest) : std::wstring(I18N::Game::EndlessTowerNone);
        const std::wstring topText = topHeight > 0 ? Printf(I18N::Game::EndlessTowerWave, topHeight) : std::wstring(I18N::Game::EndlessTowerNone);
        updated.best = line(Printf(I18N::Game::EndlessTowerBest, ownText.c_str(), topText.c_str()), CONTENT_WIDTH);

        bool someoneAway = false;
        bool someoneBroke = false;
        bool someoneOut = false;
        for (const auto& member : tower.Members())
        {
            someoneAway = someoneAway || !member.Near;
            someoneBroke = someoneBroke || !member.HasZen;
            someoneOut = someoneOut || member.RunsLeft <= 0;
        }

        switch (m_tab)
        {
        case TAB_PARTY:
            addRow(Printf(I18N::Game::EndlessTowerPayNote, price.c_str()), ROW_PLAIN);
            for (const auto& member : tower.Members())
            {
                if (!member.Near)
                    addRow(Printf(I18N::Game::EndlessTowerMemberAway, member.Name.c_str()), ROW_BAD);
                else if (member.RunsLeft <= 0)
                    addRow(Printf(I18N::Game::EndlessTowerMemberNoRuns, member.Name.c_str()), ROW_BAD);
                else if (!member.HasZen)
                    addRow(Printf(I18N::Game::EndlessTowerMemberNoZen, member.Name.c_str(), price.c_str()), ROW_BAD);
                else if (member.BestHeight > 0)
                    addRow(Printf(I18N::Game::EndlessTowerMember, member.Name.c_str(), member.RunsLeft, status.RunsPerDay,
                                  member.BestHeight),
                           ROW_GOOD);
                else
                    addRow(Printf(I18N::Game::EndlessTowerMemberNew, member.Name.c_str(), member.RunsLeft, status.RunsPerDay), ROW_GOOD);
            }
            break;
        case TAB_BOSSES:
            addRow(I18N::Game::EndlessTowerBossTitle, ROW_TITLE);
            for (const auto& reward : tower.BossRewards())
                addReward(reward.FromWave, reward.Text);
            if (tower.BossRewards().empty())
                addRow(I18N::Game::EndlessTowerNoRewards, ROW_PLAIN);
            break;
        case TAB_REWARDS:
            addRow(I18N::Game::EndlessTowerHeightTitle, ROW_TITLE);
            for (const auto& reward : tower.HeightRewards())
                addReward(reward.FromWave, reward.Text);
            if (tower.HeightRewards().empty())
                addRow(I18N::Game::EndlessTowerNoRewards, ROW_PLAIN);
            break;
        default:
            addRow(I18N::Game::EndlessTowerTopTitle, ROW_TITLE);
            for (const auto& entry : tower.Top())
            {
                addRow(Printf(I18N::Game::EndlessTowerTopName, entry.Place, entry.Name.c_str(), entry.ClassName.c_str()), ROW_NAME);
                addRow(Printf(I18N::Game::EndlessTowerTopDetail, entry.Height, FormatDuration(entry.Seconds).c_str(), entry.PartySize),
                       ROW_PLAIN);
            }
            if (tower.Top().empty())
                addRow(I18N::Game::EndlessTowerTopEmpty, ROW_PLAIN);
            break;
        }

        // The button works when the server would let the party in; otherwise the line above it says why.
        std::wstring reason;
        if (status.InRun)
            reason = I18N::Game::EndlessTowerInRun;
        else if (!status.Near)
            reason = I18N::Game::EndlessTowerComeToNpc;
        else if (!status.Leader)
            reason = I18N::Game::EndlessTowerLeaderOnly;
        else if (!updated.partyOk)
            reason = Printf(I18N::Game::EndlessTowerPartySize, status.MinimumParty, status.MaximumParty);
        else if (someoneAway)
            reason = I18N::Game::EndlessTowerNotNear;
        else if (someoneOut)
            reason = I18N::Game::EndlessTowerNoRuns;
        else if (someoneBroke)
            reason = Printf(I18N::Game::EndlessTowerNoZen, price.c_str());

        if (tower.IsEntering())
        {
            hintLines(I18N::Game::EndlessTowerEntering);
        }
        else if (!tower.LastEnterText().empty() && !tower.LastEnterSucceeded())
        {
            hintLines(tower.LastEnterText());
            updated.hintBad = true;
        }
        else if (!reason.empty())
        {
            hintLines(reason);
            updated.hintBad = true;
        }

        updated.canEnter = reason.empty() && !tower.IsEntering();
        updated.enterLabel = line(Printf(I18N::Game::EndlessTowerEnter, price.c_str()), ROW_WIDTH, true);
    }

    auto& binder = m_RmlView.Binder();
    SyncFieldFrom(binder, &EndlessTowerRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::entry, "entry", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::party, "party", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::partyOk, "party_ok", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::reset, "reset", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::best, "best", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::tabs, "tabs", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::rows, "rows", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::hint, "hint", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::hintBad, "hint_bad", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::canEnter, "can_enter", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::enterLabel, "enter_label", updated);
}
