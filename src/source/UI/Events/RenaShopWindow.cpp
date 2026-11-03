#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Events/RenaShopWindow.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Events/RenaShop.h"
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
#include <utility>

using namespace SEASON3B;
using namespace mu::ui::window;
using GameLogic::RenaShop::Line;
using GameLogic::RenaShop::Prize;
using GameLogic::RenaShop::Rena;

namespace
{
// While the window is open, the data is asked for again this often: the bag changes, the week moves on.
constexpr auto RefreshInterval = std::chrono::seconds(5);
// Without an answer this long after asking, the server does not send the Rena shop.
constexpr auto AnswerTimeout = std::chrono::seconds(5);
// The line above the buttons takes this many lines at most: the server's refusals are longer than one.
constexpr size_t MaxHintLines = 3;
// The second button buys / spins this many at once.
constexpr int BuyMoreCount = 5;
constexpr int SpinMoreCount = 10;
// The last spins listed on the Golden Arrow tab.
constexpr size_t MaxResultRows = 4;

enum eROW_KIND
{
    ROW_PLAIN = 0,
    ROW_TITLE = 1,
    ROW_GOOD = 2,
    ROW_BAD = 3,
    ROW_NAME = 4,
    ROW_MORE = 5,
    ROW_DIM = 6,
};

enum eHINT_KIND
{
    HINT_INFO = 0,
    HINT_GOOD = 1,
    HINT_BAD = 2,
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
RenaShopLine MakeLine(const std::wstring& text, int width, bool bold = false)
{
    if (text.empty())
        return {};
    g_pRenderText->SetFont(bold ? g_hFontBold : g_hFont);
    const auto role = bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal;
    return {StringUtils::WideToNarrow(text.c_str()),
            UI::RmlBridge::NativeTextPxInBox(role, static_cast<float>(MeasureInReferenceUnits(text)),
                                             static_cast<float>(width))};
}

// The text word by word on lines of the width, maxLines at most (the last one takes the rest).
std::vector<std::wstring> WrapWords(const std::wstring& text, int width, size_t maxLines)
{
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
        if (!current.empty() && MeasureInReferenceUnits(candidate) > width && lines.size() + 1 < maxLines)
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
    return lines;
}

// The line above the buttons: the last answer (green or red), or why the buttons are off.
struct Hint
{
    std::wstring text;
    int kind = HINT_INFO;
};

void SetHint(RenaShopRmlModel& updated, const Hint& hint)
{
    for (const auto& part : WrapWords(hint.text, CRenaShopWindow::CONTENT_WIDTH, MaxHintLines))
        updated.hint.push_back(MakeLine(part, CRenaShopWindow::CONTENT_WIDTH));
    updated.hintKind = hint.kind;
}

void AddRow(std::vector<RenaShopRow>& rows, const std::wstring& text, int kind, int item = -1, bool selected = false)
{
    const bool bold = kind == ROW_TITLE || kind == ROW_NAME;
    rows.push_back({MakeLine(text, CRenaShopWindow::ROW_WIDTH, bold), kind, item, selected});
}

void ReplaceAll(std::wstring& text, std::wstring_view from, std::wstring_view to)
{
    for (size_t at = text.find(from); at != std::wstring::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
}

// "Bless", "Box +4", "Bless x10": the server's item names, shorter for the narrow window.
std::wstring ShortName(std::wstring name, int count)
{
    ReplaceAll(name, L"Box of Kundun ", L"Box ");
    ReplaceAll(name, L"Jewel of ", L"");
    const std::wstring suffix = L" x" + std::to_wstring(count);
    const bool named = name.size() >= suffix.size() && name.ends_with(suffix); // "Large Healing Potion +13 x50"
    return count > 1 && !named ? name + suffix : name;
}

// "Box +4 · 3 Rena". A line's zen is what it gives (its name says so), never a price.
std::wstring ShopLineText(const Line& line)
{
    return Printf(I18N::Game::RenaShopLineRena, ShortName(line.Name, line.Count).c_str(),
                  Thousands(line.Price).c_str());
}

// A line out of the balance's reach is grey.
int ShopLineKind(const Line& line)
{
    return Rena().MostBuyable(line) > 0 || line.Price <= 0 ? ROW_PLAIN : ROW_DIM;
}

std::wstring PrizeText(const Prize& prize)
{
    if (prize.Group < 0 && prize.ZenMax > 0)
        return Printf(I18N::Game::RenaShopPrizeZen, Thousands(prize.ZenMin).c_str(), Thousands(prize.ZenMax).c_str(),
                      prize.Chance.c_str());
    return Printf(I18N::Game::RenaShopPrize, ShortName(prize.Name, prize.Count).c_str(), prize.Chance.c_str());
}

void AddShopRows(std::vector<RenaShopRow>& rows, const std::wstring& pick)
{
    const auto& status = Rena().GetStatus();
    if (status.Waiting > 0)
        AddRow(rows, Printf(I18N::Game::RenaShopWaiting, status.Waiting), ROW_BAD);

    const auto& lines = Rena().Lines();
    for (size_t i = 0; i < lines.size(); ++i)
        AddRow(rows, ShopLineText(lines[i]), ShopLineKind(lines[i]), static_cast<int>(i), lines[i].Key == pick);
    if (lines.empty())
        AddRow(rows, I18N::Game::RenaShopEmpty, ROW_PLAIN);
}

// True when the prize with the key is announced to everyone.
bool IsAnnounced(const std::wstring& key)
{
    for (const auto& prize : Rena().Prizes())
    {
        if (prize.Key == key)
            return prize.Announce;
    }
    return false;
}

void AddArrowRows(std::vector<RenaShopRow>& rows)
{
    const auto& store = Rena();
    AddRow(rows, Printf(I18N::Game::RenaShopSpinPrice, Thousands(store.GetStatus().SpinPrice).c_str()), ROW_TITLE);
    for (const auto& prize : store.Prizes())
        AddRow(rows, PrizeText(prize), prize.Announce ? ROW_NAME : ROW_PLAIN);
    AddRow(rows, I18N::Game::RenaShopAnnounced, ROW_DIM);

    AddRow(rows, I18N::Game::RenaShopResultsTitle, ROW_TITLE);
    const auto& results = store.Results();
    for (size_t i = 0; i < results.size() && i < MaxResultRows; ++i)
        AddRow(rows, results[i].Text, IsAnnounced(results[i].Key) ? ROW_NAME : ROW_GOOD);
    if (results.empty())
        AddRow(rows, I18N::Game::RenaShopNoResults, ROW_DIM);
}

void AddProgressRows(std::vector<RenaShopRow>& rows)
{
    const auto& store = Rena();
    const auto& status = store.GetStatus();
    AddRow(rows, Printf(I18N::Game::RenaShopHanded, Thousands(status.HandedCharacter).c_str()), ROW_TITLE);
    const bool russian = GameLogic::Titles::Store::Russian();
    for (const auto& milestone : store.Milestones())
    {
        const std::wstring& title = russian && !milestone.Russian.empty() ? milestone.Russian : milestone.English;
        AddRow(rows, Printf(I18N::Game::RenaShopMilestone, Thousands(milestone.Count).c_str(), title.c_str()),
               milestone.Reached ? ROW_GOOD : ROW_DIM);
    }

    const std::wstring week = Thousands(status.HandedWeek);
    AddRow(rows, status.WeekPlace > 0 ? Printf(I18N::Game::RenaShopWeek, week.c_str(), status.WeekPlace)
                                      : Printf(I18N::Game::RenaShopWeekNoPlace, week.c_str()),
           ROW_TITLE);
    for (const auto& entry : store.Week())
        AddRow(rows, Printf(I18N::Game::RenaShopTopLine, entry.Place, entry.Name.c_str(), Thousands(entry.Handed).c_str()),
               ROW_PLAIN);
    if (store.Week().empty())
        AddRow(rows, I18N::Game::RenaShopTopEmpty, ROW_DIM);
}

void BindHeaderFields(Rml::DataModelConstructor& c, RenaShopRmlModel& model)
{
    auto lineType = c.RegisterStruct<RenaShopLine>();
    lineType.RegisterMember("text", &RenaShopLine::text);
    lineType.RegisterMember("text_px", &RenaShopLine::textPx);
    c.Bind("title", &model.title);
    c.Bind("balance", &model.balance);
    c.Bind("bag", &model.bag);
    c.RegisterArray<std::vector<RenaShopLine>>();
    c.Bind("hint", &model.hint);
    c.Bind("hint_kind", &model.hintKind);
    c.Bind("can_give", &model.canGive);
    c.Bind("give_label", &model.giveLabel);
    c.Bind("exit_tooltip", &model.exitTooltip);
}

void BindListFields(Rml::DataModelConstructor& c, RenaShopRmlModel& model)
{
    auto tabType = c.RegisterStruct<RenaShopTab>();
    tabType.RegisterMember("tab", &RenaShopTab::tab);
    tabType.RegisterMember("label", &RenaShopTab::label);
    tabType.RegisterMember("selected", &RenaShopTab::selected);
    c.RegisterArray<std::vector<RenaShopTab>>();
    c.Bind("tabs", &model.tabs);

    auto rowType = c.RegisterStruct<RenaShopRow>();
    rowType.RegisterMember("text", &RenaShopRow::text);
    rowType.RegisterMember("kind", &RenaShopRow::kind);
    rowType.RegisterMember("item", &RenaShopRow::item);
    rowType.RegisterMember("selected", &RenaShopRow::selected);
    c.RegisterArray<std::vector<RenaShopRow>>();
    c.Bind("rows", &model.rows);
}

void BindButtonFields(Rml::DataModelConstructor& c, RenaShopRmlModel& model)
{
    c.Bind("show_buttons", &model.showButtons);
    c.Bind("can_first", &model.canFirst);
    c.Bind("can_second", &model.canSecond);
    c.Bind("second_count", &model.secondCount);
    c.Bind("first_label", &model.firstLabel);
    c.Bind("second_label", &model.secondLabel);
}
} // namespace

mu::ui::window::CRenaShopWindow::CRenaShopWindow() = default;

mu::ui::window::CRenaShopWindow::~CRenaShopWindow()
{
    Release();
}

bool mu::ui::window::CRenaShopWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_RENA_SHOP, this);
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void mu::ui::window::CRenaShopWindow::Release()
{
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float mu::ui::window::CRenaShopWindow::GetLayerDepth()
{
    return LayerDepth;
}

float mu::ui::window::CRenaShopWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CRenaShopWindow::OpenningProcess()
{
    m_openedAt = std::chrono::steady_clock::now();
    m_lastRefresh = m_openedAt;
    m_seenAnswers = Rena().Answers();
    m_showAnswer = false;
    Rena().Request();
}

void mu::ui::window::CRenaShopWindow::ClosingProcess()
{
}

bool mu::ui::window::CRenaShopWindow::UpdateMouseEvent()
{
    // The tabs, the rows, the buttons and the corner close are RmlUi's (see Update()); a click on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool mu::ui::window::CRenaShopWindow::UpdateKeyEvent()
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_RENA_SHOP))
        return true;

    if (IsPress(VK_ESCAPE))
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_RENA_SHOP);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    return true;
}

bool mu::ui::window::CRenaShopWindow::Update()
{
    const bool exit = std::exchange(m_pendingExit, false);
    const bool give = std::exchange(m_pendingGive, false);
    const int tab = std::exchange(m_pendingTab, -1);
    const int pick = std::exchange(m_pendingPick, -1);
    const int act = std::exchange(m_pendingAct, 0);
    if (IsVisible() && exit)
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_RENA_SHOP);
        PlayBuffer(SOUND_CLICK01);
    }
    else if (IsVisible())
    {
        SelectTab(tab);
        TakeActions(give, pick, act);
        TakeAnswers();
        RefreshFromServer();
    }

    SyncRmlModel();
    return true;
}

void mu::ui::window::CRenaShopWindow::SelectTab(int tab)
{
    if (tab < 0 || tab >= TAB_COUNT || tab == m_tab)
        return;

    m_tab = tab;
    m_showAnswer = false;
    PlayBuffer(SOUND_CLICK01);
}

void mu::ui::window::CRenaShopWindow::TakeActions(bool give, int pick, int act)
{
    auto& store = Rena();
    const auto& lines = store.Lines();
    if (pick >= 0 && pick < static_cast<int>(lines.size()) && m_tab == TAB_SHOP)
    {
        m_pick = lines[static_cast<size_t>(pick)].Key;
        m_showAnswer = false;
        PlayBuffer(SOUND_CLICK01);
    }

    // The buttons are off when the action cannot go through, but the click is checked here all the same.
    if (store.IsBusy() || !store.GetStatus().Near)
        return;

    if (give && store.GetStatus().InBag > 0)
    {
        m_showAnswer = false;
        store.Give();
        PlayBuffer(SOUND_CLICK01);
    }
    else if (act > 0 && m_tab == TAB_SHOP)
    {
        const Line* line = PickedLine();
        const int most = line != nullptr ? store.MostBuyable(*line) : 0;
        const int count = (std::min)(act, most);
        if (count < 1 || (act > 1 && most < 2)) // the second button is off with one to buy
            return;
        m_showAnswer = false;
        store.Buy(line->Key, count);
        PlayBuffer(SOUND_CLICK01);
    }
    else if (act > 0 && m_tab == TAB_ARROW)
    {
        const int most = store.MostSpins();
        const int count = (std::min)(act, most);
        if (count < 1 || (act > 1 && most < 2)) // the second button is off with one spin
            return;
        m_showAnswer = false;
        store.Spin(count);
        PlayBuffer(SOUND_CLICK01);
    }
}

void mu::ui::window::CRenaShopWindow::TakeAnswers()
{
    if (Rena().Answers() == m_seenAnswers)
        return;

    m_seenAnswers = Rena().Answers();
    m_showAnswer = true;
}

void mu::ui::window::CRenaShopWindow::RefreshFromServer()
{
    const auto now = std::chrono::steady_clock::now();
    if (now - m_lastRefresh < RefreshInterval)
        return;

    m_lastRefresh = now;
    Rena().Request();
}

const Line* mu::ui::window::CRenaShopWindow::PickedLine() const
{
    if (m_pick.empty())
        return nullptr;
    for (const auto& line : Rena().Lines())
    {
        if (line.Key == m_pick)
            return &line;
    }
    return nullptr;
}

bool mu::ui::window::CRenaShopWindow::Render()
{
    // Nothing native: the frame, the lines and the buttons are RmlUi. Kept because CObject requires it.
    return true;
}

void mu::ui::window::CRenaShopWindow::BindRmlModel(Rml::DataModelConstructor& c, RenaShopRmlModel& model)
{
    BindHeaderFields(c, model);
    BindListFields(c, model);
    BindButtonFields(c, model);

    c.BindEventCallback("rena_shop_exit", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingExit = true; });
    c.BindEventCallback("rena_shop_give", [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&)
                        { m_pendingGive = true; });
    c.BindEventCallback("rena_shop_tab",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingTab = args[0].Get<int>(-1);
                        });
    c.BindEventCallback("rena_shop_row",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingPick = args[0].Get<int>(-1);
                        });
    c.BindEventCallback("rena_shop_act",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingAct = args[0].Get<int>(0);
                        });

    wchar_t closeText[256] = {};
    mu_swprintf_s(closeText, I18N::Game::CloseS, L"Esc");
    model.exitTooltip = StringUtils::WideToNarrow(closeText);
}

void mu::ui::window::CRenaShopWindow::SyncRmlModel()
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

void mu::ui::window::CRenaShopWindow::SyncContent()
{
    const auto now = std::chrono::steady_clock::now();
    RenaShopRmlModel updated;
    SyncHeader(updated, now);
    if (Rena().HasInfo())
    {
        AddRows(updated);
        SyncFoot(updated);
    }
    PublishModel(updated);
}

void mu::ui::window::CRenaShopWindow::SyncHeader(RenaShopRmlModel& updated,
                                                 std::chrono::steady_clock::time_point now) const
{
    updated.title = MakeLine(I18N::Game::RenaShopTitle, WINDOW_WIDTH, true);
    const wchar_t* tabLabels[TAB_COUNT] = {I18N::Game::RenaShopTabShop, I18N::Game::RenaShopTabArrow,
                                           I18N::Game::RenaShopTabProgress};
    for (int i = 0; i < TAB_COUNT; ++i)
        updated.tabs.push_back({i, MakeLine(tabLabels[i], TAB_WIDTH, true), i == m_tab});

    const auto& store = Rena();
    if (!store.HasInfo())
    {
        updated.balance = MakeLine(now - m_openedAt < AnswerTimeout ? I18N::Game::RenaShopAsking : I18N::Game::RenaShopNoAnswer,
                                   CONTENT_WIDTH);
        return;
    }

    const auto& status = store.GetStatus();
    updated.balance = MakeLine(Printf(I18N::Game::RenaShopBalance, Thousands(status.Balance).c_str()), CONTENT_WIDTH, true);
    updated.bag = MakeLine(Printf(I18N::Game::RenaShopBag, Thousands(status.InBag).c_str()), CONTENT_WIDTH);
    updated.canGive = status.Near && status.InBag > 0 && !store.IsBusy();
    updated.giveLabel = MakeLine(Printf(I18N::Game::RenaShopGive, Thousands(status.InBag).c_str()), ROW_WIDTH, true);
}

void mu::ui::window::CRenaShopWindow::AddRows(RenaShopRmlModel& updated) const
{
    switch (m_tab)
    {
    case TAB_SHOP:
        AddShopRows(updated.rows, m_pick);
        break;
    case TAB_ARROW:
        AddArrowRows(updated.rows);
        break;
    default:
        AddProgressRows(updated.rows);
        break;
    }
}

void mu::ui::window::CRenaShopWindow::SyncFoot(RenaShopRmlModel& updated) const
{
    const auto& store = Rena();
    const Line* line = PickedLine();
    if (store.IsBusy())
        SetHint(updated, {I18N::Game::RenaShopWorking, HINT_INFO});
    else if (m_showAnswer && !store.LastAnswerText().empty())
        SetHint(updated, {store.LastAnswerText(), store.LastAnswerSucceeded() ? HINT_GOOD : HINT_BAD});
    else if (!store.GetStatus().Near)
        SetHint(updated, {I18N::Game::RenaShopNotNear, HINT_BAD});
    else if (m_tab == TAB_SHOP && line == nullptr)
        SetHint(updated, {I18N::Game::RenaShopPick, HINT_INFO});
    else if (m_tab == TAB_SHOP && store.MostBuyable(*line) <= 0)
        SetHint(updated, {I18N::Game::RenaShopNoRena, HINT_BAD});
    else if (m_tab == TAB_ARROW && store.MostSpins() <= 0)
        SetHint(updated, {I18N::Game::RenaShopNoRena, HINT_BAD});

    if (m_tab == TAB_SHOP)
        SyncShopButtons(updated);
    else if (m_tab == TAB_ARROW)
        SyncSpinButtons(updated);
}

void mu::ui::window::CRenaShopWindow::SyncShopButtons(RenaShopRmlModel& updated) const
{
    // The buttons buy the picked line: one, and as many as the second says (what the balance allows).
    const auto& store = Rena();
    const Line* line = PickedLine();
    const int most = line != nullptr ? store.MostBuyable(*line) : 0;
    updated.showButtons = true;
    updated.canFirst = store.GetStatus().Near && most > 0 && !store.IsBusy();
    updated.canSecond = updated.canFirst && most > 1;
    // Disabled (nothing picked, or only one affordable), it shows the full step.
    updated.secondCount = most > 1 ? (std::min)(most, BuyMoreCount) : BuyMoreCount;
    updated.firstLabel = MakeLine(Printf(I18N::Game::RenaShopBuy, 1), BUY_WIDTH, true);
    updated.secondLabel = MakeLine(Printf(I18N::Game::RenaShopBuy, updated.secondCount), BUY_WIDTH, true);
}

void mu::ui::window::CRenaShopWindow::SyncSpinButtons(RenaShopRmlModel& updated) const
{
    const auto& store = Rena();
    const int most = store.MostSpins();
    updated.showButtons = true;
    updated.canFirst = store.GetStatus().Near && most > 0 && !store.IsBusy();
    updated.canSecond = updated.canFirst && most > 1;
    updated.secondCount = most > 1 ? (std::min)(most, SpinMoreCount) : SpinMoreCount;
    updated.firstLabel = MakeLine(Printf(I18N::Game::RenaShopSpin, 1), BUY_WIDTH, true);
    updated.secondLabel = MakeLine(Printf(I18N::Game::RenaShopSpin, updated.secondCount), BUY_WIDTH, true);
}

void mu::ui::window::CRenaShopWindow::PublishModel(RenaShopRmlModel& updated)
{
    auto& binder = m_RmlView.Binder();
    SyncFieldFrom(binder, &RenaShopRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::balance, "balance", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::bag, "bag", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::tabs, "tabs", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::rows, "rows", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::hint, "hint", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::hintKind, "hint_kind", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::canGive, "can_give", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::giveLabel, "give_label", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::showButtons, "show_buttons", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::canFirst, "can_first", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::canSecond, "can_second", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::secondCount, "second_count", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::firstLabel, "first_label", updated);
    SyncFieldFrom(binder, &RenaShopRmlModel::secondLabel, "second_label", updated);
}
