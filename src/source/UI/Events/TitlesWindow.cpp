#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Events/TitlesWindow.h"

#include "Audio/DSPlaySound.h"
#include "GameLogic/Events/Titles.h"
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
using GameLogic::Titles::Titles;

namespace
{
// While the window is open, the list is asked for again this often: titles come and go with the weeks.
constexpr auto RefreshInterval = std::chrono::seconds(60);
// Without an answer this long after opening, the server doesn't send titles.
constexpr auto AnswerTimeout = std::chrono::seconds(5);

int MeasureInReferenceUnits(const std::wstring& text)
{
    return g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size())).cx;
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

// "3 d 4 h left", "4 h 10 min left", "10 min left".
std::wstring TimeLeft(long long seconds)
{
    const long long minutes = std::max<long long>(1, (seconds + 59) / 60);
    if (minutes >= 24 * 60)
        return Printf(I18N::Game::TitlesLeftDays, static_cast<int>(minutes / (24 * 60)), static_cast<int>(minutes / 60 % 24));
    if (minutes >= 60)
        return Printf(I18N::Game::TitlesLeftHours, static_cast<int>(minutes / 60), static_cast<int>(minutes % 60));
    return Printf(I18N::Game::TitlesLeftMinutes, static_cast<int>(minutes));
}

// "3", "2.5": a percent with a dot, whatever the C locale says.
std::wstring Percent(float value)
{
    const int tenths = static_cast<int>(value * 10.f + 0.5f);
    return tenths % 10 == 0 ? std::to_wstring(tenths / 10) : std::to_wstring(tenths / 10) + L"." + std::to_wstring(tenths % 10);
}

// The bonus of a title while it is worn: "+3% HP, -1% damage taken, +1% damage".
std::wstring BonusText(const GameLogic::Titles::Entry& entry)
{
    std::wstring text;
    auto add = [&text](wchar_t sign, float value, const wchar_t* what)
    {
        if (value <= 0.f)
            return;
        if (!text.empty())
            text += L", ";
        text += sign + Percent(value) + L"% " + what;
    };
    add(L'+', entry.BonusHealth, I18N::Game::TitlesBonusHealth);
    add(L'-', entry.BonusTaken, I18N::Game::TitlesBonusTaken);
    add(L'+', entry.BonusDamage, I18N::Game::TitlesBonusDamage);
    return text;
}

// How long a title not owned yet would last.
const wchar_t* TermText(GameLogic::Titles::Term term)
{
    switch (term)
    {
    case GameLogic::Titles::Term::Week:
        return I18N::Game::TitlesTermWeek;
    case GameLogic::Titles::Term::Season:
        return I18N::Game::TitlesTermSeason;
    case GameLogic::Titles::Term::WhileHeld:
        return I18N::Game::TitlesWhileHeld;
    default:
        return I18N::Game::TitlesForever;
    }
}
} // namespace

mu::ui::window::CTitlesWindow::CTitlesWindow() = default;

mu::ui::window::CTitlesWindow::~CTitlesWindow()
{
    Release();
}

bool mu::ui::window::CTitlesWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_TITLES, this);
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void mu::ui::window::CTitlesWindow::Release()
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float mu::ui::window::CTitlesWindow::GetLayerDepth()
{
    return LayerDepth;
}

float mu::ui::window::CTitlesWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CTitlesWindow::OpenningProcess()
{
    m_openedAt = std::chrono::steady_clock::now();
    m_lastRequest = m_openedAt;
    m_tab = 0;
    Titles().RequestList();
}

void mu::ui::window::CTitlesWindow::ClosingProcess()
{
}

bool mu::ui::window::CTitlesWindow::UpdateMouseEvent()
{
    // The tabs, the rows and the corner close are RmlUi's (see Update()); a click on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool mu::ui::window::CTitlesWindow::UpdateKeyEvent()
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_TITLES))
        return true;

    if (IsPress(VK_ESCAPE))
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_TITLES);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    return true;
}

bool mu::ui::window::CTitlesWindow::Update()
{
    const bool exit = m_pendingExit;
    const int tab = m_pendingTab;
    const int wear = m_pendingWear;
    m_pendingExit = false;
    m_pendingTab = -1;
    m_pendingWear = -1;
    if (IsVisible())
    {
        auto& store = Titles();
        const auto now = std::chrono::steady_clock::now();
        if (exit)
        {
            g_pNewUISystem->Hide(mu::ui::window::INTERFACE_TITLES);
            PlayBuffer(SOUND_CLICK01);
        }
        else
        {
            if (tab >= 0 && tab <= 1 && tab != m_tab)
            {
                m_tab = tab;
                PlayBuffer(SOUND_CLICK01);
            }

            if (wear >= 0 && wear < static_cast<int>(m_rowIds.size()) && m_rowOwned[wear])
            {
                // "No title" also when nothing is worn yet: it stops a new title from going on by itself.
                const std::wstring& id = m_rowIds[wear];
                if (id != store.Wearing() || (id.empty() && store.Automatic()))
                {
                    store.Wear(id);
                    m_lastRequest = now;
                }
                PlayBuffer(SOUND_CLICK01);
            }

            if (now - m_lastRequest >= RefreshInterval)
            {
                store.RequestList();
                m_lastRequest = now;
            }
        }
    }

    SyncRmlModel();
    return true;
}

bool mu::ui::window::CTitlesWindow::Render()
{
    // Nothing native: the frame, the tabs and the rows are RmlUi. Kept because CObject requires it.
    return true;
}

void mu::ui::window::CTitlesWindow::BindRmlModel(Rml::DataModelConstructor& c, TitlesRmlModel& model)
{
    auto lineType = c.RegisterStruct<TitlesLine>();
    lineType.RegisterMember("text", &TitlesLine::text);
    lineType.RegisterMember("text_px", &TitlesLine::textPx);
    c.Bind("title", &model.title);
    c.Bind("status", &model.status);
    c.Bind("detail", &model.detail);
    c.Bind("footer", &model.footer);

    auto tabType = c.RegisterStruct<TitlesTab>();
    tabType.RegisterMember("label", &TitlesTab::label);
    tabType.RegisterMember("selected", &TitlesTab::selected);
    c.RegisterArray<std::vector<TitlesTab>>();
    c.Bind("tabs", &model.tabs);

    auto rowType = c.RegisterStruct<TitlesRow>();
    rowType.RegisterMember("index", &TitlesRow::index);
    rowType.RegisterMember("name", &TitlesRow::name);
    rowType.RegisterMember("detail", &TitlesRow::detail);
    rowType.RegisterMember("bonus", &TitlesRow::bonus);
    rowType.RegisterMember("hint", &TitlesRow::hint);
    rowType.RegisterMember("mark", &TitlesRow::mark);
    rowType.RegisterMember("rarity", &TitlesRow::rarity);
    rowType.RegisterMember("owned", &TitlesRow::owned);
    rowType.RegisterMember("worn", &TitlesRow::worn);
    c.RegisterArray<std::vector<TitlesRow>>();
    c.Bind("rows", &model.rows);

    c.Bind("exit_tooltip", &model.exitTooltip);

    c.BindEventCallback("titles_exit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingExit = true; });
    c.BindEventCallback("titles_tab",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingTab = args[0].Get<int>(-1);
                        });
    c.BindEventCallback("titles_wear",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingWear = args[0].Get<int>(-1);
                        });

    wchar_t closeText[256] = {};
    mu_swprintf_s(closeText, I18N::Game::CloseS, L"Esc");
    model.exitTooltip = StringUtils::WideToNarrow(closeText);
}

void mu::ui::window::CTitlesWindow::SyncRmlModel()
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

void mu::ui::window::CTitlesWindow::SyncContent()
{
    // A line of the window's own text, at the size the native renderer would have shrunk it to for its box.
    auto line = [](const std::wstring& text, int width, bool bold = false) -> TitlesLine
    {
        if (text.empty())
            return {};
        g_pRenderText->SetFont(bold ? g_hFontBold : g_hFont);
        const auto role = bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal;
        return {StringUtils::WideToNarrow(text.c_str()),
                UI::RmlBridge::NativeTextPxInBox(role, static_cast<float>(MeasureInReferenceUnits(text)),
                                                 static_cast<float>(width))};
    };
    // A text that wraps in its box (a hint): the native size, never shrunk.
    auto wrapped = [](const std::wstring& text) -> TitlesLine
    {
        if (text.empty())
            return {};
        return {StringUtils::WideToNarrow(text.c_str()), UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Normal)};
    };

    auto& store = Titles();
    const auto now = std::chrono::steady_clock::now();

    TitlesRmlModel updated;
    updated.title = line(I18N::Game::TitlesTitle, WINDOW_WIDTH, true);
    updated.tabs.push_back({line(I18N::Game::TitlesTabMine, TAB_WIDTH), m_tab == 0});
    updated.tabs.push_back({line(I18N::Game::TitlesTabAll, TAB_WIDTH), m_tab == 1});

    std::vector<std::wstring> ids;
    std::vector<bool> owned;
    if (!store.HasList())
    {
        updated.status = line(now - m_openedAt < AnswerTimeout ? I18N::Game::TitlesAsking : I18N::Game::TitlesNoAnswer,
                              CONTENT_WIDTH);
    }
    else
    {
        const auto& list = store.List();
        const auto worn = std::find_if(list.begin(), list.end(),
                                       [&store](const auto& e) { return e.Owned && e.Id == store.Wearing(); });
        updated.status = line(worn != list.end() ? Printf(I18N::Game::TitlesWearing, worn->Name.c_str())
                                                 : std::wstring(I18N::Game::TitlesWearingNone),
                              CONTENT_WIDTH, true);
        updated.detail = line(store.Automatic() ? I18N::Game::TitlesAutomatic : I18N::Game::TitlesClickHint, CONTENT_WIDTH);

        // Seconds since the list came: the time left counts down between the answers.
        const long long elapsed = static_cast<long long>((GetTickCount64() - store.ListTime()) / 1000);
        const TitlesLine wornMark = line(I18N::Game::TitlesWorn, MARK_WIDTH, true);

        if (m_tab == 0)
        {
            TitlesRow none;
            none.index = 0;
            none.name = line(I18N::Game::TitlesNone, ROW_WIDTH, true);
            none.detail = line(I18N::Game::TitlesNoneDetail, ROW_WIDTH - MARK_WIDTH);
            none.rarity = -1;
            none.owned = true;
            none.worn = worn == list.end();
            if (none.worn)
                none.mark = wornMark;
            updated.rows.push_back(std::move(none));
            ids.emplace_back();
            owned.push_back(true);
        }

        int ownedCount = 0;
        for (const auto& entry : list)
        {
            if (entry.Owned)
                ++ownedCount;
            if (m_tab == 0 && !entry.Owned)
                continue;

            TitlesRow row;
            row.index = static_cast<int>(updated.rows.size());
            row.name = line(entry.Name, ROW_WIDTH, true);
            row.rarity = static_cast<int>(entry.Color);
            row.owned = entry.Owned;
            row.worn = entry.Owned && entry.Id == store.Wearing();
            if (row.worn)
                row.mark = wornMark;
            if (entry.HasBonus())
                row.bonus = wrapped(BonusText(entry));

            if (!entry.Owned)
            {
                row.detail = line(TermText(entry.Lasts), ROW_WIDTH - MARK_WIDTH);
                row.hint = wrapped(entry.Hint);
            }
            else if (entry.SecondsLeft < 0)
            {
                row.detail = line(I18N::Game::TitlesWhileHeld, ROW_WIDTH - MARK_WIDTH);
            }
            else if (entry.SecondsLeft == 0)
            {
                row.detail = line(I18N::Game::TitlesForever, ROW_WIDTH - MARK_WIDTH);
            }
            else
            {
                row.detail = line(TimeLeft(std::max<long long>(0, entry.SecondsLeft - elapsed)), ROW_WIDTH - MARK_WIDTH);
            }

            updated.rows.push_back(std::move(row));
            ids.push_back(entry.Id);
            owned.push_back(entry.Owned);
        }

        if (m_tab == 0 && ownedCount == 0)
            updated.footer = wrapped(I18N::Game::TitlesEmpty);
        else if (m_tab == 1)
            updated.footer = line(Printf(I18N::Game::TitlesOwnedCount, ownedCount, static_cast<int>(list.size())), CONTENT_WIDTH);
    }

    m_rowIds = std::move(ids);
    m_rowOwned = std::move(owned);

    auto& binder = m_RmlView.Binder();
    SyncFieldFrom(binder, &TitlesRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &TitlesRmlModel::tabs, "tabs", updated);
    SyncFieldFrom(binder, &TitlesRmlModel::status, "status", updated);
    SyncFieldFrom(binder, &TitlesRmlModel::detail, "detail", updated);
    SyncFieldFrom(binder, &TitlesRmlModel::rows, "rows", updated);
    SyncFieldFrom(binder, &TitlesRmlModel::footer, "footer", updated);
}
