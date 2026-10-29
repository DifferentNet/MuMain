#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Events/EndlessTowerWindow.h"

#include "Audio/DSPlaySound.h"
#include "Engine/Object/ZzzCharacter.h"
#include "GameLogic/Events/EndlessTower.h"
#include "GameLogic/Events/EndlessTowerShop.h"
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
using GameLogic::EndlessTower::Reward;
using GameLogic::EndlessTower::ShopItem;
using GameLogic::EndlessTower::ShopStatus;
using GameLogic::EndlessTower::Tower;
using GameLogic::EndlessTower::TowerShop;

namespace
{
// While the window is open, the state is asked for again this often: members come and go, runs get used.
constexpr auto RefreshInterval = std::chrono::seconds(5);
// Without an answer this long after asking, the server doesn't send the Endless Tower (or its shop).
constexpr auto AnswerTimeout = std::chrono::seconds(5);
// How far a line that continues the one above is indented (reference px, as .et-row.more in the theme).
constexpr int ContinuationIndent = 8;
// The line above the button takes this many lines at most: the server's refusals are longer than one.
constexpr size_t MaxHintLines = 3;
// The shop's second button buys up to this many at once.
constexpr int BuyMoreCount = 10;
constexpr long long SecondsPerHour = 3600;
constexpr long long SecondsPerDay = 24 * SecondsPerHour;

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
    const long long hours = total / SecondsPerHour;
    const long long minutes = total % SecondsPerHour / 60;
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

// A line of the window's own text, at the size the native renderer would have shrunk it to for its box.
EndlessTowerLine MakeLine(const std::wstring& text, int width, bool bold = false)
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

// The line above the button(s), red when it says why they are off.
struct Hint
{
    std::wstring text;
    bool bad = false;
};

void SetHint(EndlessTowerRmlModel& updated, const Hint& hint)
{
    for (const auto& part : WrapWords(hint.text, CEndlessTowerWindow::CONTENT_WIDTH, MaxHintLines))
        updated.hint.push_back(MakeLine(part, CEndlessTowerWindow::CONTENT_WIDTH));
    updated.hintBad = hint.bad;
}

void AddRow(std::vector<EndlessTowerRow>& rows, const std::wstring& text, int kind, int item = -1, bool selected = false)
{
    const bool bold = kind == ROW_TITLE || kind == ROW_NAME;
    const int width = kind == ROW_MORE ? CEndlessTowerWindow::ROW_WIDTH - ContinuationIndent : CEndlessTowerWindow::ROW_WIDTH;
    rows.push_back({MakeLine(text, width, bold), kind, item, selected});
}

// "Wave 80+: Bless x6, Soul x5, Life x3, Creation x2" on as many lines as it needs, the items whole.
void AddReward(std::vector<EndlessTowerRow>& rows, int fromWave, const std::wstring& text)
{
    g_pRenderText->SetFont(g_hFont);
    std::wstring current = Printf(I18N::Game::EndlessTowerFromWave, fromWave, L"");
    bool first = true;
    bool empty = true;
    for (const auto& item : SplitItems(ShortRewardText(text)))
    {
        const std::wstring candidate = empty ? current + item : current + L", " + item;
        const int width = first ? CEndlessTowerWindow::ROW_WIDTH : CEndlessTowerWindow::ROW_WIDTH - ContinuationIndent;
        if (!empty && MeasureInReferenceUnits(candidate) > width)
        {
            AddRow(rows, current + L",", first ? ROW_PLAIN : ROW_MORE);
            first = false;
            current = item;
        }
        else
        {
            current = candidate;
        }
        empty = false;
    }
    AddRow(rows, current, first ? ROW_PLAIN : ROW_MORE);
}

void AddRewardRows(std::vector<EndlessTowerRow>& rows, const wchar_t* title, const std::vector<Reward>& rewards)
{
    AddRow(rows, title, ROW_TITLE);
    for (const auto& reward : rewards)
        AddReward(rows, reward.FromWave, reward.Text);
    if (rewards.empty())
        AddRow(rows, I18N::Game::EndlessTowerNoRewards, ROW_PLAIN);
}

// Who of the party can go in, and why not.
void AddPartyRows(std::vector<EndlessTowerRow>& rows)
{
    const auto& tower = Tower();
    const auto& status = tower.GetStatus();
    const std::wstring price = Thousands(status.Price);
    AddRow(rows, Printf(I18N::Game::EndlessTowerPayNote, price.c_str()), ROW_PLAIN);
    for (const auto& member : tower.Members())
    {
        const wchar_t* name = member.Name.c_str();
        if (!member.Near)
            AddRow(rows, Printf(I18N::Game::EndlessTowerMemberAway, name), ROW_BAD);
        else if (member.RunsLeft <= 0)
            AddRow(rows, Printf(I18N::Game::EndlessTowerMemberNoRuns, name), ROW_BAD);
        else if (!member.HasZen)
            AddRow(rows, Printf(I18N::Game::EndlessTowerMemberNoZen, name, price.c_str()), ROW_BAD);
        else if (member.BestHeight > 0)
            AddRow(rows, Printf(I18N::Game::EndlessTowerMember, name, member.RunsLeft, status.RunsPerDay, member.BestHeight),
                   ROW_GOOD);
        else
            AddRow(rows, Printf(I18N::Game::EndlessTowerMemberNew, name, member.RunsLeft, status.RunsPerDay), ROW_GOOD);
    }
}

void AddTopRows(std::vector<EndlessTowerRow>& rows)
{
    const auto& top = Tower().Top();
    AddRow(rows, I18N::Game::EndlessTowerTopTitle, ROW_TITLE);
    for (const auto& entry : top)
    {
        AddRow(rows, Printf(I18N::Game::EndlessTowerTopName, entry.Place, entry.Name.c_str(), entry.ClassName.c_str()), ROW_NAME);
        AddRow(rows, Printf(I18N::Game::EndlessTowerTopDetail, entry.Height, FormatDuration(entry.Seconds).c_str(), entry.PartySize),
               ROW_PLAIN);
    }
    if (top.empty())
        AddRow(rows, I18N::Game::EndlessTowerTopEmpty, ROW_PLAIN);
}

// "Best: wave 41, top: wave 67", from the own entry among the party's members and the top's first.
std::wstring BestText()
{
    const auto& tower = Tower();
    int ownBest = 0;
    for (const auto& member : tower.Members())
    {
        if (Hero != nullptr && wcscmp(member.Name.c_str(), Hero->ID) == 0)
            ownBest = member.BestHeight;
    }
    const int topHeight = tower.Top().empty() ? 0 : tower.Top().front().Height;
    const auto wave = [](int height)
    { return height > 0 ? Printf(I18N::Game::EndlessTowerWave, height) : std::wstring(I18N::Game::EndlessTowerNone); };
    return Printf(I18N::Game::EndlessTowerBest, wave(ownBest).c_str(), wave(topHeight).c_str());
}

// Why the server would refuse to take the party in, empty when it would let it.
std::wstring EnterRefusal(bool partyOk)
{
    const auto& tower = Tower();
    const auto& status = tower.GetStatus();
    bool someoneAway = false;
    bool someoneBroke = false;
    bool someoneOut = false;
    for (const auto& member : tower.Members())
    {
        someoneAway = someoneAway || !member.Near;
        someoneBroke = someoneBroke || !member.HasZen;
        someoneOut = someoneOut || member.RunsLeft <= 0;
    }

    if (status.InRun)
        return I18N::Game::EndlessTowerInRun;
    if (!status.Near)
        return I18N::Game::EndlessTowerComeToNpc;
    if (!status.Leader)
        return I18N::Game::EndlessTowerLeaderOnly;
    if (!partyOk)
        return Printf(I18N::Game::EndlessTowerPartySize, status.MinimumParty, status.MaximumParty);
    if (someoneAway)
        return I18N::Game::EndlessTowerNotNear;
    if (someoneOut)
        return I18N::Game::EndlessTowerNoRuns;
    if (someoneBroke)
        return Printf(I18N::Game::EndlessTowerNoZen, Thousands(status.Price).c_str());
    return {};
}

bool LimitReached(const ShopItem& item)
{
    return item.Limit > 0 && item.Bought >= item.Limit;
}

// "Bless", "Box +4", "Bless x10" for an item sold by tens.
std::wstring ShopItemName(const ShopItem& item)
{
    std::wstring name = ShortRewardText(item.Name);
    if (item.Count > 1)
        name += L" x" + std::to_wstring(item.Count);
    return name;
}

// "Box +4 · 250 pts · 1/4": the price, and the purchases made this month against the limit.
std::wstring ShopItemText(const ShopItem& item)
{
    const std::wstring name = ShopItemName(item);
    const std::wstring price = Thousands(item.Price);
    if (item.Limit > 0)
        return Printf(I18N::Game::EndlessTowerShopItemLimit, name.c_str(), price.c_str(), item.Bought, item.Limit);
    return Printf(I18N::Game::EndlessTowerShopItem, name.c_str(), price.c_str());
}

// Bought out red, out of the points' reach grey.
int ShopItemKind(const ShopItem& item)
{
    if (LimitReached(item))
        return ROW_BAD;
    return TowerShop().MostBuyable(item) > 0 ? ROW_PLAIN : ROW_DIM;
}

// When the month's limits start again, counted down from the server's answer.
std::wstring RenewText(const ShopStatus& status, std::chrono::steady_clock::time_point now)
{
    const long long passed = std::chrono::duration_cast<std::chrono::seconds>(now - status.ReceivedAt).count();
    const long long left = std::max<long long>(0, status.SecondsToMonth - passed);
    if (left >= SecondsPerDay)
        return Printf(I18N::Game::EndlessTowerShopRenewDays, static_cast<int>(left / SecondsPerDay),
                      static_cast<int>(left % SecondsPerDay / SecondsPerHour));
    return Printf(I18N::Game::EndlessTowerShopRenew, FormatDuration(left).c_str());
}

// What the picked item costs, how many are left this month and where it goes.
std::wstring PickInfo(const ShopItem& item)
{
    const std::wstring name = ShopItemName(item);
    const std::wstring price = Thousands(item.Price);
    std::wstring text = item.Limit > 0
        ? Printf(I18N::Game::EndlessTowerShopInfo, name.c_str(), price.c_str(), item.Limit - item.Bought)
        : Printf(I18N::Game::EndlessTowerShopInfoNoLimit, name.c_str(), price.c_str());
    return text + L" " + (item.Bank ? I18N::Game::EndlessTowerShopToBank : I18N::Game::EndlessTowerShopToBag);
}

// The line above the shop's buttons: the answer to the last purchase, the purchase on its way, why the buttons are
// off, or what the picked item costs.
Hint ShopHint(const ShopItem* item, bool showAnswer)
{
    const auto& shop = TowerShop();
    if (showAnswer && !shop.LastBuyText().empty())
        return {shop.LastBuyText(), !shop.LastBuySucceeded()};
    if (shop.IsBuying())
        return {I18N::Game::EndlessTowerShopBuying, false};
    if (!shop.HasShop())
        return {};
    if (!shop.GetStatus().Near)
        return {I18N::Game::EndlessTowerComeToNpc, true};
    if (item == nullptr)
        return {I18N::Game::EndlessTowerShopPick, false};
    if (LimitReached(*item))
        return {I18N::Game::EndlessTowerShopLimitReached, true};
    if (shop.MostBuyable(*item) <= 0)
        return {I18N::Game::EndlessTowerShopNoPoints, true};
    return {PickInfo(*item), false};
}

void BindHeaderFields(Rml::DataModelConstructor& c, EndlessTowerRmlModel& model)
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
    c.Bind("exit_tooltip", &model.exitTooltip);
}

void BindListFields(Rml::DataModelConstructor& c, EndlessTowerRmlModel& model)
{
    auto tabType = c.RegisterStruct<EndlessTowerTab>();
    tabType.RegisterMember("tab", &EndlessTowerTab::tab);
    tabType.RegisterMember("label", &EndlessTowerTab::label);
    tabType.RegisterMember("selected", &EndlessTowerTab::selected);
    c.RegisterArray<std::vector<EndlessTowerTab>>();
    c.Bind("tabs", &model.tabs);

    auto rowType = c.RegisterStruct<EndlessTowerRow>();
    rowType.RegisterMember("text", &EndlessTowerRow::text);
    rowType.RegisterMember("kind", &EndlessTowerRow::kind);
    rowType.RegisterMember("item", &EndlessTowerRow::item);
    rowType.RegisterMember("selected", &EndlessTowerRow::selected);
    c.RegisterArray<std::vector<EndlessTowerRow>>();
    c.Bind("rows", &model.rows);
}

void BindShopFields(Rml::DataModelConstructor& c, EndlessTowerRmlModel& model)
{
    c.Bind("show_buy", &model.showBuy);
    c.Bind("can_buy", &model.canBuy);
    c.Bind("can_buy_more", &model.canBuyMore);
    c.Bind("buy_more_count", &model.buyMoreCount);
    c.Bind("buy_one_label", &model.buyOneLabel);
    c.Bind("buy_more_label", &model.buyMoreLabel);
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
    m_seenBuyAnswers = TowerShop().BuyAnswers();
    m_showBuyAnswer = false;
    Tower().Request();
    if (m_tab == TAB_SHOP)
        AskShop();
}

void mu::ui::window::CEndlessTowerWindow::ClosingProcess()
{
}

bool mu::ui::window::CEndlessTowerWindow::UpdateMouseEvent()
{
    // The tabs, the rows, the buttons and the corner close are RmlUi's (see Update()); a click on the window stops here.
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
    const bool exit = std::exchange(m_pendingExit, false);
    const bool enter = std::exchange(m_pendingEnter, false);
    const int tab = std::exchange(m_pendingTab, -1);
    const int pick = std::exchange(m_pendingPick, -1);
    const int buy = std::exchange(m_pendingBuy, 0);
    if (IsVisible() && exit)
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_ENDLESS_TOWER);
        PlayBuffer(SOUND_CLICK01);
    }
    else if (IsVisible())
    {
        SelectTab(tab);
        if (TakeTowerActions(enter))
            return true;
        TakeShopActions(pick, buy);
        RefreshFromServer();
    }

    SyncRmlModel();
    return true;
}

void mu::ui::window::CEndlessTowerWindow::SelectTab(int tab)
{
    if (tab < 0 || tab >= TAB_COUNT || tab == m_tab)
        return;

    m_tab = tab;
    PlayBuffer(SOUND_CLICK01);
    if (m_tab == TAB_SHOP)
        AskShop();
}

bool mu::ui::window::CEndlessTowerWindow::TakeTowerActions(bool enter)
{
    auto& tower = Tower();
    if (enter && m_tab != TAB_SHOP)
    {
        tower.Enter();
        PlayBuffer(SOUND_CLICK01);
    }

    // The party went in: the warp follows, the window has done its job.
    if (tower.EnterAnswers() == m_seenEnterAnswers)
        return false;
    m_seenEnterAnswers = tower.EnterAnswers();
    if (!tower.LastEnterSucceeded())
        return false;

    g_pNewUISystem->Hide(mu::ui::window::INTERFACE_ENDLESS_TOWER);
    return true;
}

void mu::ui::window::CEndlessTowerWindow::TakeShopActions(int pick, int buy)
{
    auto& shop = TowerShop();
    if (pick >= 0 && pick < static_cast<int>(shop.Items().size()))
    {
        m_shopPick = shop.Items()[static_cast<size_t>(pick)].Key;
        m_showBuyAnswer = false;
        PlayBuffer(SOUND_CLICK01);
    }

    if (shop.BuyAnswers() != m_seenBuyAnswers)
    {
        m_seenBuyAnswers = shop.BuyAnswers();
        m_showBuyAnswer = true;
    }

    // The buttons are off when the purchase can't go through, but the click is checked here all the same.
    const ShopItem* item = PickedItem();
    if (buy <= 0 || m_tab != TAB_SHOP || item == nullptr || shop.IsBuying() || !shop.GetStatus().Near)
        return;
    const int count = (std::min)(buy, shop.MostBuyable(*item));
    if (count < 1)
        return;

    m_showBuyAnswer = false;
    shop.Buy(item->Key, count);
    PlayBuffer(SOUND_CLICK01);
}

void mu::ui::window::CEndlessTowerWindow::RefreshFromServer()
{
    const auto now = std::chrono::steady_clock::now();
    if (now - m_lastRefresh < RefreshInterval)
        return;

    m_lastRefresh = now;
    // On the shop, asking again also hands over items that waited for room in the bag.
    if (m_tab == TAB_SHOP)
        TowerShop().Request();
    else
        Tower().Request();
}

void mu::ui::window::CEndlessTowerWindow::AskShop()
{
    m_shopAskedAt = std::chrono::steady_clock::now();
    TowerShop().Request();
}

const ShopItem* mu::ui::window::CEndlessTowerWindow::PickedItem() const
{
    if (m_shopPick.empty())
        return nullptr;
    for (const auto& item : TowerShop().Items())
    {
        if (item.Key == m_shopPick)
            return &item;
    }
    return nullptr;
}

bool mu::ui::window::CEndlessTowerWindow::Render()
{
    // Nothing native: the frame, the lines and the buttons are RmlUi. Kept because CObject requires it.
    return true;
}

void mu::ui::window::CEndlessTowerWindow::BindRmlModel(Rml::DataModelConstructor& c, EndlessTowerRmlModel& model)
{
    BindHeaderFields(c, model);
    BindListFields(c, model);
    BindShopFields(c, model);

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
    c.BindEventCallback("endless_tower_row",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingPick = args[0].Get<int>(-1);
                        });
    c.BindEventCallback("endless_tower_buy",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args)
                        {
                            if (args.size() == 1)
                                m_pendingBuy = args[0].Get<int>(0);
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
    const auto now = std::chrono::steady_clock::now();
    EndlessTowerRmlModel updated;
    SyncHeader(updated, now);
    if (m_tab == TAB_SHOP)
    {
        AddShopRows(updated, now);
        SyncShopFoot(updated);
    }
    else if (Tower().HasInfo())
    {
        AddTowerRows(updated);
        SyncEnterFoot(updated);
    }
    PublishModel(updated);
}

void mu::ui::window::CEndlessTowerWindow::SyncHeader(EndlessTowerRmlModel& updated,
                                                     std::chrono::steady_clock::time_point now) const
{
    updated.title = MakeLine(I18N::Game::EndlessTowerTitle, WINDOW_WIDTH, true);
    const wchar_t* tabLabels[TAB_COUNT] = {I18N::Game::EndlessTowerTabParty, I18N::Game::EndlessTowerTabBosses,
                                           I18N::Game::EndlessTowerTabRewards, I18N::Game::EndlessTowerTabTop,
                                           I18N::Game::EndlessTowerTabShop};
    for (int i = 0; i < TAB_COUNT; ++i)
        updated.tabs.push_back({i, MakeLine(tabLabels[i], TAB_WIDTH, true), i == m_tab});

    const auto& tower = Tower();
    if (!tower.HasInfo())
    {
        updated.entry = MakeLine(now - m_openedAt < AnswerTimeout ? I18N::Game::EndlessTowerAsking : I18N::Game::EndlessTowerNoAnswer,
                                 CONTENT_WIDTH);
        return;
    }

    const auto& status = tower.GetStatus();
    const std::wstring price = Thousands(status.Price);
    updated.entry = MakeLine(Printf(I18N::Game::EndlessTowerEntry, price.c_str(), status.RunsPerDay), CONTENT_WIDTH, true);
    updated.partyOk = status.PartySize >= status.MinimumParty && status.PartySize <= status.MaximumParty;
    updated.party = MakeLine(Printf(I18N::Game::EndlessTowerParty, status.PartySize, status.MinimumParty, status.MaximumParty),
                             CONTENT_WIDTH);
    const long long toReset = status.SecondsToReset
        - std::chrono::duration_cast<std::chrono::seconds>(now - status.ReceivedAt).count();
    updated.reset = MakeLine(Printf(I18N::Game::EndlessTowerReset, FormatDuration(toReset).c_str()), CONTENT_WIDTH);
    updated.best = MakeLine(BestText(), CONTENT_WIDTH);
}

void mu::ui::window::CEndlessTowerWindow::AddTowerRows(EndlessTowerRmlModel& updated) const
{
    const auto& tower = Tower();
    switch (m_tab)
    {
    case TAB_PARTY:
        AddPartyRows(updated.rows);
        break;
    case TAB_BOSSES:
        AddRewardRows(updated.rows, I18N::Game::EndlessTowerBossTitle, tower.BossRewards());
        break;
    case TAB_REWARDS:
        AddRewardRows(updated.rows, I18N::Game::EndlessTowerHeightTitle, tower.HeightRewards());
        break;
    default:
        AddTopRows(updated.rows);
        break;
    }
}

void mu::ui::window::CEndlessTowerWindow::SyncEnterFoot(EndlessTowerRmlModel& updated) const
{
    // The button works when the server would let the party in; otherwise the line above it says why.
    const auto& tower = Tower();
    const std::wstring reason = EnterRefusal(updated.partyOk);
    if (tower.IsEntering())
        SetHint(updated, {I18N::Game::EndlessTowerEntering, false});
    else if (!tower.LastEnterText().empty() && !tower.LastEnterSucceeded())
        SetHint(updated, {tower.LastEnterText(), true});
    else if (!reason.empty())
        SetHint(updated, {reason, true});

    updated.canEnter = reason.empty() && !tower.IsEntering();
    const std::wstring price = Thousands(tower.GetStatus().Price);
    updated.enterLabel = MakeLine(Printf(I18N::Game::EndlessTowerEnter, price.c_str()), ROW_WIDTH, true);
}

void mu::ui::window::CEndlessTowerWindow::AddShopRows(EndlessTowerRmlModel& updated,
                                                      std::chrono::steady_clock::time_point now) const
{
    auto& rows = updated.rows;
    const auto& shop = TowerShop();
    if (!shop.HasShop())
    {
        AddRow(rows, now - m_shopAskedAt < AnswerTimeout ? I18N::Game::EndlessTowerShopAsking : I18N::Game::EndlessTowerShopNoAnswer,
               ROW_PLAIN);
        return;
    }

    const auto& status = shop.GetStatus();
    AddRow(rows, Printf(I18N::Game::EndlessTowerShopBalance, Thousands(status.Balance).c_str()), ROW_TITLE);
    AddRow(rows, Printf(I18N::Game::EndlessTowerShopEarn, status.PointsPerWave, status.PointsPerBoss), ROW_PLAIN);
    AddRow(rows, RenewText(status, now), ROW_PLAIN);
    if (status.Waiting > 0)
        AddRow(rows, Printf(I18N::Game::EndlessTowerShopWaiting, status.Waiting), ROW_BAD);

    const auto& items = shop.Items();
    for (size_t i = 0; i < items.size(); ++i)
        AddRow(rows, ShopItemText(items[i]), ShopItemKind(items[i]), static_cast<int>(i), items[i].Key == m_shopPick);
    if (items.empty())
        AddRow(rows, I18N::Game::EndlessTowerShopEmpty, ROW_PLAIN);
}

void mu::ui::window::CEndlessTowerWindow::SyncShopFoot(EndlessTowerRmlModel& updated) const
{
    // The buttons buy the picked item: one, and as many as the second says (up to 10, what the points and the limit
    // allow). Enter stays hidden here.
    const auto& shop = TowerShop();
    const ShopItem* item = PickedItem();
    const int most = item != nullptr ? shop.MostBuyable(*item) : 0;
    SetHint(updated, ShopHint(item, m_showBuyAnswer));

    updated.showBuy = true;
    updated.canBuy = shop.HasShop() && shop.GetStatus().Near && most > 0 && !shop.IsBuying();
    updated.canBuyMore = updated.canBuy && most > 1;
    // Disabled (nothing picked, or only one affordable), it shows the full step.
    updated.buyMoreCount = most > 1 ? std::min(most, BuyMoreCount) : BuyMoreCount;
    updated.buyOneLabel = MakeLine(Printf(I18N::Game::EndlessTowerShopBuy, 1), BUY_WIDTH, true);
    updated.buyMoreLabel = MakeLine(Printf(I18N::Game::EndlessTowerShopBuy, updated.buyMoreCount), BUY_WIDTH, true);
}

void mu::ui::window::CEndlessTowerWindow::PublishModel(EndlessTowerRmlModel& updated)
{
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
    SyncFieldFrom(binder, &EndlessTowerRmlModel::showBuy, "show_buy", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::canBuy, "can_buy", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::canBuyMore, "can_buy_more", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::buyMoreCount, "buy_more_count", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::buyOneLabel, "buy_one_label", updated);
    SyncFieldFrom(binder, &EndlessTowerRmlModel::buyMoreLabel, "buy_more_label", updated);
}
