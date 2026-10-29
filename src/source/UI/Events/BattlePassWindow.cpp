#include "stdafx.h"
#include "I18N/All.h"

#include "UI/Events/BattlePassWindow.h"

#include "Audio/DSPlaySound.h"
#include "Character/CharacterManager.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzInventory.h"
#include "GameLogic/Events/BattlePass.h"
#include "GameLogic/Events/SeasonMarks.h"
#include "UI/Core/WindowSystem.h"
#include "UI/Inventory/ItemMng.h"
#include "UI/Tooltip/LegacyTextListTooltip.h"

#include "Core/Utilities/StringUtils.h"
#include "Render/Text/CUIRenderText.h"
#include "UI/RmlBridge/RmlDocumentVisibility.h"
#include "UI/RmlBridge/RmlElementBox.h"
#include "UI/RmlBridge/RmlNativeTextSize.h"
#include "UI/RmlBridge/RmlPointer.h"
#include "UI/RmlBridge/RmlSyncField.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>

using namespace SEASON3B;
using namespace mu::ui::window;
using namespace GameLogic::BattlePass;

namespace
{
// While the window is open, the state is asked for again this often (a quest's plain progress sends no signal).
constexpr auto RefreshInterval = std::chrono::seconds(30);
// After a button, the server's answer to it is asked for this long after (its signal usually comes first).
constexpr auto AfterClickDelay = std::chrono::seconds(2);
// Without an answer this long after opening, the server doesn't have the battle pass.
constexpr auto AnswerTimeout = std::chrono::seconds(5);
int MeasureInReferenceUnits(const std::wstring& text)
{
    return g_pRenderText->MeasureText(text.c_str(), static_cast<int>(text.size())).cx;
}

template <typename... Args> std::wstring Format(const wchar_t* format, Args... args)
{
    wchar_t text[512] = {};
    mu_swprintf_s(text, format, args...);
    return text;
}

Rml::String Narrow(const std::wstring& text)
{
    return StringUtils::WideToNarrow(text.c_str());
}

// 5:12:30, 12:30 under an hour; "12d 05:12" from a day.
std::wstring FormatDuration(long long totalSeconds)
{
    const long long total = std::max<long long>(0, totalSeconds);
    const long long days = total / 86400;
    const long long hours = total % 86400 / 3600;
    const long long minutes = total % 3600 / 60;
    const long long seconds = total % 60;
    if (days > 0)
        return Format(I18N::Game::BattlePassDays, static_cast<int>(days), static_cast<int>(hours),
                      static_cast<int>(minutes));
    if (hours > 0)
        return Format(L"%lld:%02lld:%02lld", hours, minutes, seconds);
    return Format(L"%lld:%02lld", minutes, seconds);
}

// A line of the window's own text, at the size the native renderer would have shrunk it to for its box.
BattlePassLine TextLine(const std::wstring& text, int width, bool bold = false)
{
    if (text.empty())
        return {};
    g_pRenderText->SetFont(bold ? g_hFontBold : g_hFont);
    const auto role = bold ? UI::Scaling::FontRole::Bold : UI::Scaling::FontRole::Normal;
    return {Narrow(text), UI::RmlBridge::NativeTextPxInBox(role, static_cast<float>(MeasureInReferenceUnits(text)),
                                                             static_cast<float>(width))};
}

// The season's points multiplier: 200 -> "x2", 150 -> "x1.5", 125 -> "x1.25".
std::wstring MultiplierText(int percent)
{
    std::wstring text = Format(L"x%d", percent / 100);
    if (percent % 100 != 0)
    {
        std::wstring fraction = Format(L"%02d", percent % 100);
        if (fraction.back() == L'0')
            fraction.pop_back();
        text += L"." + fraction;
    }
    return text;
}

// 17430 -> "17 430".
std::wstring Grouped(int value)
{
    std::wstring digits = std::to_wstring(std::abs(value));
    for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3)
        digits.insert(static_cast<size_t>(i), L" ");
    return value < 0 ? L"-" + digits : digits;
}

// The map of the day in "Kill 1500 monsters in Aida".
std::wstring MapOfDay(const std::wstring& text)
{
    const size_t in = text.rfind(L" in ");
    return in == std::wstring::npos ? std::wstring{} : text.substr(in + 4);
}

// A daily quest's kind: its id's leading letters, without what tells a day's quests of one kind apart
// ("kill2500", "kill-hard" -> "kill").
std::wstring DailyKind(const std::wstring& id)
{
    size_t end = 0;
    while (end < id.size() && id[end] >= L'a' && id[end] <= L'z')
        ++end;
    return id.substr(0, end);
}

// A quest that counts time: "1 h", or "30 min" for a time that isn't whole hours.
std::wstring TimeQuest(const wchar_t* hours, const wchar_t* minutes, int target)
{
    return target >= 60 && target % 60 == 0 ? Format(hours, target / 60) : Format(minutes, std::max(1, target));
}

// A daily quest in the window's language by its kind and the server's numbers, or the server's text for a kind
// this client doesn't know.
std::wstring DailyQuestText(const Quest& quest)
{
    const std::wstring kind = DailyKind(quest.Id);
    const int target = quest.Target;

    if (kind == L"kill")
        return Format(I18N::Game::BattlePassQuestKill, target);
    if (kind == L"reset")
        return target > 1 ? Format(I18N::Game::BattlePassQuestResets, target) : I18N::Game::BattlePassQuestReset;
    if (kind == L"events" || kind == L"event")
        return target > 1 ? Format(I18N::Game::BattlePassQuestEvents, target) : I18N::Game::BattlePassQuestEventOne;
    if (kind == L"map")
    {
        const std::wstring map = MapOfDay(quest.Text);
        if (!map.empty())
            return Format(I18N::Game::BattlePassQuestMap, target, map.c_str());
    }
    if (kind == L"golden")
        return Format(I18N::Game::BattlePassQuestGolden, target);
    if (kind == L"kalima" && target == 1)
        return I18N::Game::BattlePassQuestKalima;
    if (kind == L"jewels")
        return Format(I18N::Game::BattlePassQuestJewels, target);
    if (kind == L"mix")
        return target > 1 ? Format(I18N::Game::BattlePassQuestMix, target) : I18N::Game::BattlePassQuestMixOne;
    if (kind == L"party")
        return TimeQuest(I18N::Game::BattlePassQuestParty, I18N::Game::BattlePassQuestPartyMinutes, target);
    if (kind == L"online")
        return TimeQuest(I18N::Game::BattlePassQuestOnline, I18N::Game::BattlePassQuestOnlineMinutes, target);
    if ((kind == L"bosses" || kind == L"boss") && target > 1)
        return Format(I18N::Game::BattlePassQuestBosses, target);
    if (kind == L"upgrade")
        return target > 1 ? Format(I18N::Game::BattlePassQuestUpgrade, target) : I18N::Game::BattlePassQuestUpgradeOne;
    if ((kind == L"tier" || kind == L"tierwin") && target == 1)
        return quest.Id.find(L"win") != std::wstring::npos ? I18N::Game::BattlePassQuestTierWin
                                                           : I18N::Game::BattlePassQuestTierEnter;
    if (kind == L"goblin")
        return target > 1 ? Format(I18N::Game::BattlePassQuestGoblins, target) : I18N::Game::BattlePassQuestGoblin;
    return quest.Text;
}

// The quest in the window's language: its own wording by its id with the server's numbers, or the server's text
// for a quest this client doesn't know.
std::wstring QuestText(const Quest& quest)
{
    if (quest.Kind == QuestKind::Daily)
        return DailyQuestText(quest);

    const std::wstring& id = quest.Id;
    const int target = quest.Target;

    if (id == L"w-kill" || id == L"s-kill")
        return Format(I18N::Game::BattlePassQuestKill, target);
    if (id == L"w-reset" || id.starts_with(L"s-reset"))
        return Format(I18N::Game::BattlePassQuestResets, target);
    if (id == L"w-golden")
        return Format(I18N::Game::BattlePassQuestGolden, target);
    if (id == L"w-party")
        return TimeQuest(I18N::Game::BattlePassQuestParty, I18N::Game::BattlePassQuestPartyMinutes, target);
    if (id == L"w-runs")
        return Format(I18N::Game::BattlePassQuestRuns, target);
    if (id == L"w-scraps")
        return Format(I18N::Game::BattlePassQuestScraps, target);
    if (id == L"w-bosses")
        return Format(I18N::Game::BattlePassQuestBosses, target);
    if (id == L"w-big")
        return I18N::Game::BattlePassQuestBigEvent;
    if (id == L"w-upgrade")
        return Format(I18N::Game::BattlePassQuestUpgrade, target);
    if (id == L"w-craft")
        return Format(I18N::Game::BattlePassQuestCraft, target);
    if (id == L"w-goblin")
        return Format(I18N::Game::BattlePassQuestGoblins, target);
    if (id.starts_with(L"s-tier") && id.size() > 6)
        return Format(I18N::Game::BattlePassQuestTier, static_cast<int>(std::wcstol(id.c_str() + 6, nullptr, 10)));
    if (id == L"s-cc")
        return I18N::Game::BattlePassQuestChaosCastle;
    if (id == L"s-nightmare")
        return I18N::Game::BattlePassQuestNightmare;
    if (id == L"s-ancient")
        return I18N::Game::BattlePassQuestAncientSet;
    if (id == L"s-joa14")
        return I18N::Game::BattlePassQuestAncient14;
    if (id == L"s-wings3")
        return I18N::Game::BattlePassQuestWings3;
    if (id == L"s-fenrir")
        return I18N::Game::BattlePassQuestFenrir;
    if (id == L"s-login")
        return Format(I18N::Game::BattlePassQuestLogin, target);
    return quest.Text;
}

// A quest's progress: minutes as hours and minutes for the quests that count time.
std::wstring QuestProgress(const Quest& quest)
{
    const std::wstring kind = quest.Kind == QuestKind::Daily ? DailyKind(quest.Id) : quest.Id;
    if (kind == L"party" || kind == L"w-party" || kind == L"online")
        return Format(L"%d:%02d / %d:%02d", quest.Progress / 60, quest.Progress % 60, quest.Target / 60,
                      quest.Target % 60);
    return Grouped(quest.Progress) + L" / " + Grouped(quest.Target);
}

// An item this client has in its item list: a newer server may give one it doesn't know yet (a new item comes with
// its own client update). A slot this client already fills with another item shows that item's name: 14:161 is
// Jewel of Elevation here until the Jewel of Fate's client update renames it.
bool KnownItem(const RewardItem& item)
{
    if (item.Group < 0 || item.Group >= MAX_ITEM_TYPE || item.Number < 0 || item.Number >= MAX_ITEM_INDEX)
        return false;
    return ItemAttribute[item.Group * MAX_ITEM_INDEX + item.Number].Name[0] != 0;
}

// The hint of a season's mark: by the season's name in Data/Seasons.json («Aura «Winter I»»), by its number for a
// season it doesn't have.
std::wstring MarkHint(const RewardItem& item)
{
    auto& marks = GameLogic::SeasonMarks::Marks();
    if (item.Mark == L"title")
    {
        const std::wstring hero = marks.HeroNameOf(item.Season);
        return hero.empty() ? Format(I18N::Game::BattlePassMarkTitle, item.Season)
                            : Format(I18N::Game::BattlePassMarkTitleNamed, hero.c_str());
    }

    const std::wstring name = marks.NameOf(item.Season);
    if (item.Mark == L"aura")
        return name.empty() ? Format(I18N::Game::BattlePassMarkAura, item.Season)
                            : Format(I18N::Game::BattlePassMarkAuraNamed, name.c_str());
    if (item.Mark == L"trophy")
        return name.empty() ? Format(I18N::Game::BattlePassMarkTrophy, item.Season)
                            : Format(I18N::Game::BattlePassMarkTrophyNamed, name.c_str());
    if (item.Mark == L"fenrir")
        return name.empty() ? Format(I18N::Game::BattlePassMarkFenrir, item.Season)
                            : Format(I18N::Game::BattlePassMarkFenrirNamed, name.c_str());
    return name.empty() ? Format(I18N::Game::BattlePassMarkOther, item.Season)
                        : Format(I18N::Game::BattlePassMarkOtherNamed, name.c_str());
}

// The hint of a reward item without a model.
std::wstring SignHint(const RewardItem& item)
{
    switch (item.Kind)
    {
    case ItemKind::Excellent:
        return Format(item.Luck ? I18N::Game::BattlePassExcellentLuck : I18N::Game::BattlePassExcellent, item.Options);
    case ItemKind::Mark:
        return MarkHint(item);
    default:
        return I18N::Game::BattlePassPlaceholder;
    }
}

std::wstring SignText(const RewardItem& item)
{
    switch (item.Kind)
    {
    case ItemKind::Excellent:
        return L"EXC";
    case ItemKind::Mark:
        return L"S";
    default:
        return L"?";
    }
}

// A daily quest's difficulty in words.
std::wstring DifficultyText(int difficulty)
{
    switch (difficulty)
    {
    case 1:
        return I18N::Game::BattlePassDifficultyEasy;
    case 2:
        return I18N::Game::BattlePassDifficultyMedium;
    case 3:
        return I18N::Game::BattlePassDifficultyHard;
    default:
        return {};
    }
}

// A chest of the day by its number: I, II, III.
std::wstring ChestName(int number)
{
    switch (number)
    {
    case 1:
        return L"I";
    case 2:
        return L"II";
    case 3:
        return L"III";
    default:
        return std::to_wstring(number);
    }
}

// A chest's item as a line of its hint ("Box of Kundun +3", "Jewel of Bless x5", "Jewel of Guardian (10%)"); empty for
// a game item this client doesn't have.
std::wstring ItemLine(const RewardItem& item)
{
    std::wstring line;
    if (item.Kind == ItemKind::Item)
    {
        if (!KnownItem(item))
            return {};
        // The longest name the item tables hold, with the level's name ("Box of Kundun +3", "Gold Medal").
        wchar_t name[128] = {};
        ::GetItemName(item.Group * MAX_ITEM_INDEX + item.Number, item.Level, name);
        line = name;
    }
    else
    {
        line = SignHint(item);
    }
    if (item.Count > 1)
        line += Format(L" x%d", item.Count);
    if (item.Chance > 0 && item.Chance < 100)
        line += Format(L" (%d%%)", item.Chance);
    return line;
}

// A chest's hint, a line each: which chest, what it holds, and whether it was given.
std::wstring ChestHint(const Chest& chest, const Day& day)
{
    std::wstring hint;
    if (chest.Kind == ChestKind::Day)
        hint = Format(I18N::Game::BattlePassChestDay, ChestName(chest.Number).c_str(), chest.Need, day.Total);
    else if (day.WeekDayQuests > 0)
        hint = Format(I18N::Game::BattlePassChestWeek, chest.Need, day.WeekDayQuests);
    else
        hint = Format(I18N::Game::BattlePassChestWeekDays, chest.Need);

    // The items by this client's names; the server's own words when it has one this client doesn't.
    std::vector<std::wstring> lines;
    for (const auto& item : chest.Items)
    {
        std::wstring line = ItemLine(item);
        if (line.empty())
        {
            lines.clear();
            break;
        }
        lines.push_back(std::move(line));
    }
    if (lines.empty() && !chest.Text.empty())
    {
        size_t start = 0;
        while (start < chest.Text.size())
        {
            const size_t comma = chest.Text.find(L", ", start);
            lines.push_back(chest.Text.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start));
            if (comma == std::wstring::npos)
                break;
            start = comma + 2;
        }
    }
    for (const auto& line : lines)
        hint += L"\n" + line;
    hint += L"\n";
    hint += chest.Got ? I18N::Game::BattlePassChestGot : I18N::Game::BattlePassChestNotYet;
    return hint;
}

// A chest's box: the day's under its mark on the day's bar (`total` quests), the week's on the week's line.
BattlePassChestBox ChestBox(const Chest& chest, const Day& day, int total)
{
    const bool ofDay = chest.Kind == ChestKind::Day;
    BattlePassChestBox box;
    box.id = ofDay ? "bp_chest_" + std::to_string(chest.Number) : "bp_chest_w";
    box.label = Narrow(ofDay ? ChestName(chest.Number) : std::wstring(I18N::Game::BattlePassChestWeekLabel));
    box.hint = Narrow(ChestHint(chest, day));
    box.at = total > 0 ? 100.f * std::clamp(chest.Need, 0, total) / total : 0.f;
    box.got = chest.Got;
    box.reached = ofDay ? day.Done >= chest.Need : day.WeekDays >= chest.Need;
    return box;
}

// The day's bar's hint: what the day's quests done give in turn (medals), round again after the last, with the one
// the next quest gives (`done` of `total` done); empty when the server sends none.
std::wstring DayRewardsHint(const std::vector<Chest>& chests, int done, int total)
{
    const int rewards = static_cast<int>(
        std::count_if(chests.begin(), chests.end(), [](const Chest& c) { return c.Kind == ChestKind::Quest; }));
    const int next = rewards > 0 && done < total ? done % rewards + 1 : 0;
    std::wstring lines;
    int count = 0;
    for (const auto& chest : chests)
    {
        if (chest.Kind != ChestKind::Quest)
            continue;
        std::wstring items;
        for (const auto& item : chest.Items)
        {
            const std::wstring line = ItemLine(item);
            if (line.empty())
            {
                items.clear();
                break;
            }
            items += (items.empty() ? L"" : L", ") + line;
        }
        if (items.empty())
            items = chest.Text;
        if (items.empty())
            continue;
        lines += Format(L"\n%d. %ls", chest.Number, items.c_str());
        if (chest.Number == next)
            lines += Format(L"  (%ls)", I18N::Game::BattlePassDayRewardsNext);
        ++count;
    }
    if (count == 0)
        return {};
    std::wstring hint = I18N::Game::BattlePassDayRewards + lines;
    if (count < total)
        hint += std::wstring(L"\n") + I18N::Game::BattlePassDayRewardsAgain;
    return hint;
}

// The level the track is opened on: the first that waits to be claimed, else the next to reach.
int FocusLevel(const Status& status, const std::vector<Level>& levels)
{
    for (const auto& level : levels)
    {
        if (level.State == LevelState::Claimable)
            return level.Number;
    }
    return std::clamp(status.Level + 1, 1, std::max(1, status.MaxLevel));
}
} // namespace

mu::ui::window::CBattlePassWindow::CBattlePassWindow() = default;

mu::ui::window::CBattlePassWindow::~CBattlePassWindow()
{
    Release();
}

bool mu::ui::window::CBattlePassWindow::Create(CManager* pNewUIMng)
{
    if (pNewUIMng == nullptr)
        return false;

    m_pNewUIMng = pNewUIMng;
    m_pNewUIMng->AddUIObj(mu::ui::window::INTERFACE_BATTLE_PASS, this);
    m_RmlView.Ensure();
    Show(false);
    return true;
}

void mu::ui::window::CBattlePassWindow::Release()
{
    m_ItemTarget.Disable();
    ReleaseTooltipItem();
    if (m_pNewUIMng)
    {
        m_pNewUIMng->RemoveUIObj(this);
        m_pNewUIMng = nullptr;
    }

    m_RmlView.Release();
}

float mu::ui::window::CBattlePassWindow::GetLayerDepth()
{
    return LayerDepth;
}

float mu::ui::window::CBattlePassWindow::GetKeyEventOrder()
{
    return 10.f;
}

void mu::ui::window::CBattlePassWindow::OpenningProcess()
{
    m_openedAt = std::chrono::steady_clock::now();
    m_refreshDue = {};
    m_centerPending = true;
    m_builtFrom.reset();
    m_timersAt = -1;
    Pass().Request();
}

void mu::ui::window::CBattlePassWindow::ClosingProcess()
{
    m_hoveredItem = -1;
    ReleaseTooltipItem();
    UI::Tooltip::HideLegacyTextList();
}

bool mu::ui::window::CBattlePassWindow::UpdateMouseEvent()
{
    // The buttons, tabs and panes are RmlUi's (see Update()); a click or the wheel on the window stops here.
    return !UI::RmlBridge::IsPointerOver(m_RmlView.Document());
}

bool mu::ui::window::CBattlePassWindow::UpdateKeyEvent()
{
    if (!g_pNewUISystem->IsVisible(mu::ui::window::INTERFACE_BATTLE_PASS))
        return true;

    if (IsPress(VK_ESCAPE))
    {
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_BATTLE_PASS);
        PlayBuffer(SOUND_CLICK01);
        return false;
    }

    return true;
}

bool mu::ui::window::CBattlePassWindow::Update()
{
    const Click click = m_pendingClick;
    const int value = m_pendingValue;
    m_pendingClick = Click::None;
    m_pendingValue = 0;
    if (IsVisible())
    {
        if (click != Click::None)
            HandleClick(click, value);
        if (IsVisible())
            RefreshIfDue();
    }

    SyncRmlModel();
    SyncTooltipItem();
    if (IsVisible() && m_tooltipItem == nullptr)
        UI::Tooltip::HideLegacyTextList();
    return true;
}

bool mu::ui::window::CBattlePassWindow::Render()
{
    // The window is RmlUi and its items are drawn by m_ItemTarget into #bp_items; only the hovered item's
    // tooltip is a native call, at the top middle of its box like the inventory's.
    if (!IsVisible() || m_tooltipItem == nullptr || m_tooltipFor < 0 || m_tooltipFor >= static_cast<int>(m_shownItems.size()))
        return true;

    Rml::ElementDocument* document = m_RmlView.Document();
    Rml::Element* icon = document != nullptr ? document->GetElementById(m_shownItems[m_tooltipFor].id) : nullptr;
    Rml::Vector2f offset;
    Rml::Vector2f size;
    if (icon != nullptr && UI::RmlBridge::DrawnBox(*icon, Rml::BoxArea::Border, offset, size))
        ::RenderItemInfoAtPx(offset.x + size.x / 2.f, offset.y, m_tooltipItem, false, 0);
    return true;
}

void mu::ui::window::CBattlePassWindow::HandleClick(Click click, int value)
{
    auto& pass = Pass();
    const auto now = std::chrono::steady_clock::now();
    switch (click)
    {
    case Click::Exit:
        g_pNewUISystem->Hide(mu::ui::window::INTERFACE_BATTLE_PASS);
        PlayBuffer(SOUND_CLICK01);
        return;
    case Click::TabRewards:
    case Click::TabQuests:
        m_tab = click == Click::TabRewards ? 0 : 1;
        m_centerPending = m_tab == 0;
        PlayBuffer(SOUND_CLICK01);
        return;
    case Click::Claim:
        if (!pass.HasInfo())
            return;
        if (pass.GetStatus().Claimable > 0)
            pass.Claim();
        else if (pass.GetStatus().Waiting > 0)
            pass.TakeWaiting();
        else
            return;
        break;
    case Click::ClaimLevel:
    {
        // A click on any row arrives here; only a level waiting for its claim is sent.
        const auto& levels = pass.Levels();
        const auto level = std::find_if(levels.begin(), levels.end(),
                                        [value](const GameLogic::BattlePass::Level& l) { return l.Number == value; });
        if (level == levels.end() || level->State != GameLogic::BattlePass::LevelState::Claimable)
            return;
        pass.Claim(value);
        break;
    }
    case Click::Take:
        pass.TakeWaiting();
        break;
    case Click::Reroll:
        pass.Reroll(value);
        break;
    default:
        return;
    }

    PlayBuffer(SOUND_CLICK01);
    m_refreshDue = now + AfterClickDelay;
}

void mu::ui::window::CBattlePassWindow::RefreshIfDue()
{
    auto& pass = Pass();
    const auto now = std::chrono::steady_clock::now();

    if (pass.RefreshWanted())
        pass.Request();

    // A refresh asked for after a button: asked until one went out after the time it was due.
    if (m_refreshDue != std::chrono::steady_clock::time_point{} && now >= m_refreshDue)
    {
        if (pass.LastRequest() < m_refreshDue)
            pass.Request();
        m_refreshDue = {};
    }
    else if (now - pass.LastRequest() >= RefreshInterval)
    {
        pass.Request();
    }

    pass.Pump();
}

void mu::ui::window::CBattlePassWindow::BindRmlModel(Rml::DataModelConstructor& c, BattlePassRmlModel& model)
{
    auto lineType = c.RegisterStruct<BattlePassLine>();
    lineType.RegisterMember("text", &BattlePassLine::text);
    lineType.RegisterMember("text_px", &BattlePassLine::textPx);

    auto iconType = c.RegisterStruct<BattlePassIcon>();
    iconType.RegisterMember("sign", &BattlePassIcon::sign);
    iconType.RegisterMember("count", &BattlePassIcon::count);
    iconType.RegisterMember("hint", &BattlePassIcon::hint);
    iconType.RegisterMember("id", &BattlePassIcon::id);
    iconType.RegisterMember("index", &BattlePassIcon::index);
    c.RegisterArray<std::vector<BattlePassIcon>>();

    auto levelType = c.RegisterStruct<BattlePassLevelRow>();
    levelType.RegisterMember("level", &BattlePassLevelRow::level);
    levelType.RegisterMember("label", &BattlePassLevelRow::label);
    levelType.RegisterMember("id", &BattlePassLevelRow::id);
    levelType.RegisterMember("icons", &BattlePassLevelRow::icons);
    levelType.RegisterMember("note", &BattlePassLevelRow::note);
    levelType.RegisterMember("hint", &BattlePassLevelRow::hint);
    levelType.RegisterMember("claimed", &BattlePassLevelRow::claimed);
    levelType.RegisterMember("claimable", &BattlePassLevelRow::claimable);
    levelType.RegisterMember("next", &BattlePassLevelRow::next);
    c.RegisterArray<std::vector<BattlePassLevelRow>>();

    auto chestType = c.RegisterStruct<BattlePassChestBox>();
    chestType.RegisterMember("label", &BattlePassChestBox::label);
    chestType.RegisterMember("hint", &BattlePassChestBox::hint);
    chestType.RegisterMember("id", &BattlePassChestBox::id);
    chestType.RegisterMember("at", &BattlePassChestBox::at);
    chestType.RegisterMember("got", &BattlePassChestBox::got);
    chestType.RegisterMember("reached", &BattlePassChestBox::reached);
    c.RegisterArray<std::vector<BattlePassChestBox>>();

    auto questType = c.RegisterStruct<BattlePassQuestRow>();
    questType.RegisterMember("type", &BattlePassQuestRow::type);
    questType.RegisterMember("text", &BattlePassQuestRow::text);
    questType.RegisterMember("detail", &BattlePassQuestRow::detail);
    questType.RegisterMember("points", &BattlePassQuestRow::points);
    questType.RegisterMember("fill", &BattlePassQuestRow::fill);
    questType.RegisterMember("done", &BattlePassQuestRow::done);
    questType.RegisterMember("timer", &BattlePassQuestRow::timer);
    questType.RegisterMember("difficulty", &BattlePassQuestRow::difficulty);
    questType.RegisterMember("difficulty_label", &BattlePassQuestRow::difficultyLabel);
    questType.RegisterMember("can_reroll", &BattlePassQuestRow::canReroll);
    questType.RegisterMember("slot", &BattlePassQuestRow::slot);
    questType.RegisterMember("hint", &BattlePassQuestRow::hint);
    questType.RegisterMember("chests", &BattlePassQuestRow::chests);
    c.RegisterArray<std::vector<BattlePassQuestRow>>();

    c.Bind("text_px", &model.textPx);
    c.Bind("bold_px", &model.boldPx);
    c.Bind("title", &model.title);
    c.Bind("season", &model.season);
    c.Bind("time", &model.time);
    c.Bind("level", &model.level);
    c.Bind("points", &model.points);
    c.Bind("gauge_fill", &model.gaugeFill);
    c.Bind("gauge", &model.gauge);
    c.Bind("tab", &model.tab);
    c.Bind("tab_rewards", &model.tabRewards);
    c.Bind("tab_quests", &model.tabQuests);
    c.Bind("status", &model.status);
    c.Bind("has_info", &model.hasInfo);
    c.Bind("levels", &model.levels);
    c.Bind("bonus", &model.bonus);
    c.Bind("quests", &model.quests);
    c.Bind("day_timer", &model.dayTimer);
    c.Bind("week_timer", &model.weekTimer);
    c.Bind("season_timer", &model.seasonTimer);
    c.Bind("can_take", &model.canTake);
    c.Bind("take_label", &model.takeLabel);
    c.Bind("can_claim", &model.canClaim);
    c.Bind("claim_label", &model.claimLabel);
    c.Bind("reroll_label", &model.rerollLabel);
    c.Bind("reroll_tooltip", &model.rerollTooltip);
    c.Bind("exit_tooltip", &model.exitTooltip);

    auto queue = [this](Click click)
    {
        return [this, click](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments)
        {
            m_pendingClick = click;
            m_pendingValue = arguments.empty() ? 0 : arguments[0].Get<int>();
        };
    };
    c.BindEventCallback("battle_pass_exit", queue(Click::Exit));
    c.BindEventCallback("battle_pass_claim", queue(Click::Claim));
    c.BindEventCallback("battle_pass_claim_level", queue(Click::ClaimLevel));
    c.BindEventCallback("battle_pass_take", queue(Click::Take));
    c.BindEventCallback("battle_pass_reroll", queue(Click::Reroll));
    c.BindEventCallback("battle_pass_tab_rewards", queue(Click::TabRewards));
    c.BindEventCallback("battle_pass_tab_quests", queue(Click::TabQuests));
    // The pointer over a reward's box (its index among the drawn items) and off it (-1).
    c.BindEventCallback("battle_pass_hover",
                        [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments)
                        { m_hoveredItem = arguments.empty() ? -1 : arguments[0].Get<int>(); });
}

void mu::ui::window::CBattlePassWindow::SyncRmlModel()
{
    m_RmlView.Ensure();
    Rml::ElementDocument* document = m_RmlView.Document();
    m_ItemTarget.Sync(document ? document->GetElementById("bp_items") : nullptr, IsVisible() && m_tab == 0);
    if (!document)
        return;

    // Layer depth UI::RmlBridge::ForegroundPanelLayerDepth: over the HUD and its logs.
    UI::RmlBridge::SyncDocumentVisibilityInFront(document, IsVisible());
    if (!IsVisible())
        return;

    const bool built = SyncContent();
    SyncTimers(built);
    CenterTrack();
}

bool mu::ui::window::CBattlePassWindow::SyncContent()
{
    auto& pass = Pass();
    const auto& status = pass.GetStatus();
    const auto now = std::chrono::steady_clock::now();

    // Built again only when what it is built from changed: every frame would remeasure every text and compare the
    // whole track and quest list.
    BuildKey key;
    key.revision = pass.Revision();
    key.tab = m_tab;
    key.state = pass.IsOff() ? 1 : pass.HasInfo() ? 0 : now - m_openedAt < AnswerTimeout ? 2 : 3;
    key.textPx = UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Normal);
    key.boldPx = UI::RmlBridge::NativeTextPx(UI::Scaling::FontRole::Bold);
    key.language = I18N::Game::BattlePassTitle;
    if (m_builtFrom == key)
        return false;
    m_builtFrom = key;

    auto line = [](const std::wstring& text, int width, bool bold = false) { return TextLine(text, width, bold); };
    BattlePassRmlModel updated;
    updated.textPx = key.textPx;
    updated.boldPx = key.boldPx;
    updated.title = line(I18N::Game::BattlePassTitle, WINDOW_WIDTH, true);
    updated.tab = m_tab;
    updated.tabRewards = line(I18N::Game::BattlePassTabRewards, TAB_WIDTH, true);
    updated.tabQuests = line(I18N::Game::BattlePassTabQuests, TAB_WIDTH, true);
    updated.rerollLabel = Narrow(I18N::Game::BattlePassReroll);
    updated.rerollTooltip = Narrow(I18N::Game::BattlePassRerollTooltip);
    updated.claimLabel = line(I18N::Game::BattlePassClaimNone, BUTTON_WIDTH, true);

    std::vector<ShownItem> shownItems;
    if (key.state != 0)
    {
        const wchar_t* text = key.state == 1   ? I18N::Game::BattlePassOff
                              : key.state == 2 ? I18N::Game::BattlePassAsking
                                               : I18N::Game::BattlePassNoAnswer;
        updated.status = line(text, CONTENT_WIDTH);
    }
    else
    {
        updated.hasInfo = true;

        // The season (its time left is SyncTimers()').
        updated.season = line(status.Name, HALF_WIDTH);

        // The level and the gauge of the next one; once the track is done, of the next bonus level. The season's
        // points multiplier goes before the points (beside the season's name it ran into the time left).
        updated.level = line(Format(I18N::Game::BattlePassLevel, status.Level, status.MaxLevel), HALF_WIDTH, true);
        std::wstring points = Format(I18N::Game::BattlePassPoints, Grouped(status.Points).c_str());
        if (status.MultiplierPercent > 100)
            points = MultiplierText(status.MultiplierPercent) + L"  " + points;
        updated.points = line(points, HALF_WIDTH);
        std::wstring gauge;
        if (status.Level < status.MaxLevel)
        {
            updated.gaugeFill = 100.f * std::clamp(status.LevelPoints, 0, status.PerLevel) / status.PerLevel;
            gauge = Grouped(status.LevelPoints) + L" / " + Grouped(status.PerLevel);
        }
        else if (status.Bonus < status.BonusMax)
        {
            updated.gaugeFill = 100.f * std::clamp(status.LevelPoints, 0, status.BonusPer) / status.BonusPer;
            gauge = Format(I18N::Game::BattlePassGaugeBonus, status.Bonus, status.BonusMax,
                           Grouped(status.LevelPoints).c_str(), Grouped(status.BonusPer).c_str());
        }
        else
        {
            updated.gaugeFill = 100.f;
            gauge = I18N::Game::BattlePassGaugeDone;
        }
        updated.gauge = line(gauge, GAUGE_WIDTH);

        // The track.
        const int focus = FocusLevel(status, pass.Levels());
        const bool darkLord = Hero != nullptr && gCharacterManager.GetBaseClass(Hero->Class) == CLASS_DARK_LORD;
        for (const auto& level : pass.Levels())
        {
            BattlePassLevelRow row;
            row.level = level.Number;
            row.label = Narrow(std::to_wstring(level.Number));
            row.id = "bp_lv_" + std::to_string(level.Number);
            row.claimed = level.State == LevelState::Claimed;
            row.claimable = level.State == LevelState::Claimable;
            row.next = level.Number == focus;
            const wchar_t* hint = row.claimed     ? I18N::Game::BattlePassLevelClaimed
                                  : row.claimable ? I18N::Game::BattlePassLevelClaim
                                                  : I18N::Game::BattlePassLevelLocked;
            row.hint = Narrow(Format(hint, level.Number));

            for (const auto& item : level.Items)
            {
                BattlePassIcon icon;
                icon.index = static_cast<int>(shownItems.size());
                icon.id = "bp_it_" + std::to_string(icon.index);
                if (item.Count > 1)
                    icon.count = "x" + std::to_string(item.Count);
                if (item.Kind == ItemKind::Item && KnownItem(item))
                {
                    ShownItem shown;
                    shown.id = icon.id;
                    shown.type = item.Group * MAX_ITEM_INDEX + item.Number;
                    shown.level = item.Level;
                    shown.count = item.Count;
                    // A Dark Lord gets the Crest of Monarch (Loch's Feather of level 1) instead of the feather.
                    if (darkLord && item.Group == 13 && item.Number == 14 && item.Level == 0)
                        shown.level = 1;
                    shownItems.push_back(std::move(shown));
                }
                else
                {
                    icon.sign = Narrow(SignText(item));
                    // An item unknown here: the level's own text (the server's names of its rewards) says what it is.
                    icon.hint = Narrow(item.Kind == ItemKind::Item && !level.Text.empty() ? level.Text : SignHint(item));
                    // Signs are counted among the drawn items too, so the hover index stays one per box.
                    ShownItem none;
                    none.id = icon.id;
                    none.type = -1;
                    shownItems.push_back(std::move(none));
                }
                row.icons.push_back(std::move(icon));
            }
            if (level.Items.empty())
                row.note = Narrow(level.Text.empty() ? std::wstring(L"-") : level.Text);
            updated.levels.push_back(std::move(row));
        }
        if (status.BonusMax > 0)
            updated.bonus = Narrow(Format(I18N::Game::BattlePassBonusLevels, status.Bonus, status.BonusMax,
                                          Grouped(status.BonusPer).c_str()));

        // The quests by kind, the weekly ones by week, and the season's result.
        const auto& quests = pass.Quests();
        const auto& day = pass.GetDay();
        // A line that isn't a quest: a heading (with the countdown `timer` names), a week's heading, a note.
        auto heading = [&](int type, const std::wstring& text, int timer = 0, bool done = false)
        {
            BattlePassQuestRow row;
            row.type = type;
            row.text = Narrow(text);
            row.timer = timer;
            row.done = done;
            updated.quests.push_back(std::move(row));
        };
        int dailyDone = 0, dailyCount = 0, weeklyDone = 0, weeklyCount = 0, seasonalDone = 0, seasonalCount = 0;
        for (const auto& quest : quests)
        {
            int& done = quest.Kind == QuestKind::Daily ? dailyDone : quest.Kind == QuestKind::Weekly ? weeklyDone : seasonalDone;
            int& count = quest.Kind == QuestKind::Daily ? dailyCount : quest.Kind == QuestKind::Weekly ? weeklyCount : seasonalCount;
            ++count;
            done += quest.Done ? 1 : 0;
        }
        // The day as the server counts it (a replaced quest's progress stays its own), else as the list shows it.
        const int dayDone = day.Present ? day.Done : dailyDone;
        const int dayTotal = day.Present && day.Total > 0 ? day.Total : dailyCount;

        if (status.SeasonPhase != Phase::Running || quests.empty())
        {
            heading(1, status.SeasonPhase == Phase::Ended ? I18N::Game::BattlePassEnded : I18N::Game::BattlePassNoQuests);
        }
        else
        {
            QuestKind kind = QuestKind::Daily;
            int week = -1;
            bool first = true;
            const bool manyWeeks = std::any_of(quests.begin(), quests.end(), [&](const Quest& q)
                                               { return q.Kind == QuestKind::Weekly && q.Week != status.Week; });
            for (const auto& quest : quests)
            {
                if (first || quest.Kind != kind)
                {
                    kind = quest.Kind;
                    first = false;
                    week = -1;
                    if (kind == QuestKind::Daily)
                    {
                        heading(1, Format(L"%ls  %d/%d", I18N::Game::BattlePassDaily, dayDone, dayTotal), 1);
                        if (day.Present)
                        {
                            // The day's bar: the quests done of the day's, with a mark and a chest per chest of the day.
                            BattlePassQuestRow bar;
                            bar.type = 4;
                            bar.detail = Narrow(Format(L"%d / %d", dayDone, dayTotal));
                            bar.fill = dayTotal > 0 ? 100.f * std::clamp(dayDone, 0, dayTotal) / dayTotal : 0.f;
                            bar.done = dayTotal > 0 && dayDone >= dayTotal;
                            bar.hint = Narrow(DayRewardsHint(pass.Chests(), dayDone, dayTotal));
                            std::vector<BattlePassChestBox> weekChests;
                            for (const auto& chest : pass.Chests())
                            {
                                if (chest.Kind != ChestKind::Quest)
                                    (chest.Kind == ChestKind::Day ? bar.chests : weekChests)
                                        .push_back(ChestBox(chest, day, dayTotal));
                            }
                            updated.quests.push_back(std::move(bar));

                            // The week's days with enough quests done, with the week's chest.
                            if (day.WeekNeed > 0)
                            {
                                const int days = std::min(day.WeekDays, day.WeekNeed);
                                heading(6,
                                        day.WeekDayQuests > 0
                                            ? Format(I18N::Game::BattlePassDayWeek, day.WeekDayQuests, days, day.WeekNeed)
                                            : Format(I18N::Game::BattlePassDayWeekDays, days, day.WeekNeed),
                                        0, day.WeekGot);
                                updated.quests.back().chests = std::move(weekChests);
                            }
                            // Which of the day's quests give the pass' points.
                            if (day.PointQuests > 0)
                                heading(5, Format(I18N::Game::BattlePassDayPoints, day.PointQuests,
                                                  std::min(dayDone, day.PointQuests), day.PointQuests));
                        }
                    }
                    else if (kind == QuestKind::Weekly)
                    {
                        heading(1, Format(L"%ls  %d/%d", I18N::Game::BattlePassWeekly, weeklyDone, weeklyCount), 2);
                    }
                    else
                    {
                        heading(1, Format(L"%ls  %d/%d", I18N::Game::BattlePassSeasonal, seasonalDone, seasonalCount), 3);
                    }
                }
                if (kind == QuestKind::Weekly && manyWeeks && quest.Week != week)
                {
                    week = quest.Week;
                    heading(2, Format(I18N::Game::BattlePassWeek, week));
                }

                BattlePassQuestRow row;
                row.type = 0;
                row.text = Narrow(QuestText(quest));
                row.detail = Narrow(QuestProgress(quest));
                // A quest past the day's first ones that give points gives none.
                if (quest.Points > 0)
                    row.points = Narrow(L"+" + Grouped(quest.Points));
                row.fill = quest.Done ? 100.f : 100.f * std::clamp(quest.Progress, 0, quest.Target) / quest.Target;
                row.done = quest.Done;
                row.difficulty = std::clamp(quest.Difficulty, 0, 3);
                row.difficultyLabel = Narrow(DifficultyText(row.difficulty));
                row.slot = quest.Slot;
                row.canReroll = quest.Kind == QuestKind::Daily && !quest.Done && status.RerollAvailable;
                updated.quests.push_back(std::move(row));
            }
        }

        auto result = [&](const wchar_t* label, const std::wstring& value)
        {
            BattlePassQuestRow row;
            row.type = 3;
            row.text = Narrow(label);
            row.detail = Narrow(value);
            updated.quests.push_back(std::move(row));
        };
        heading(1, I18N::Game::BattlePassResult);
        result(I18N::Game::BattlePassResultPoints, Grouped(status.Points));
        result(I18N::Game::BattlePassResultLevel, Format(L"%d / %d", status.Level, status.MaxLevel));
        if (status.BonusMax > 0)
            result(I18N::Game::BattlePassResultBonus, Format(L"%d / %d", status.Bonus, status.BonusMax));
        if (dailyCount > 0)
            result(I18N::Game::BattlePassResultDaily, Format(L"%d / %d", dailyDone, dailyCount));
        if (weeklyCount > 0)
            result(I18N::Game::BattlePassResultWeekly, Format(L"%d / %d", weeklyDone, weeklyCount));
        if (seasonalCount > 0)
            result(I18N::Game::BattlePassResultSeasonal, Format(L"%d / %d", seasonalDone, seasonalCount));
        result(I18N::Game::BattlePassResultClaimable, std::to_wstring(status.Claimable));
        result(I18N::Game::BattlePassResultWaiting, std::to_wstring(status.Waiting));
        if (status.Waiting > 0)
        {
            updated.canTake = true;
            updated.takeLabel = Narrow(Format(I18N::Game::BattlePassTake, status.Waiting));
        }

        // The button at the bottom.
        if (status.Claimable > 0)
        {
            updated.canClaim = true;
            updated.claimLabel = line(Format(I18N::Game::BattlePassClaim, status.Claimable), BUTTON_WIDTH, true);
        }
        else if (status.Waiting > 0)
        {
            updated.canClaim = true;
            updated.claimLabel = line(Format(I18N::Game::BattlePassTake, status.Waiting), BUTTON_WIDTH, true);
        }
    }

    // A new answer may move the boxes: the tooltip follows the pointer again from the next hover.
    if (pass.Revision() != m_shownRevision)
    {
        m_shownRevision = pass.Revision();
        if (m_hoveredItem >= static_cast<int>(shownItems.size()))
            m_hoveredItem = -1;
        ReleaseTooltipItem();
    }
    m_shownItems = std::move(shownItems);

    auto& binder = m_RmlView.Binder();
    SyncFieldFrom(binder, &BattlePassRmlModel::textPx, "text_px", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::boldPx, "bold_px", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::title, "title", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::season, "season", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::level, "level", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::points, "points", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::gaugeFill, "gauge_fill", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::gauge, "gauge", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::tab, "tab", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::tabRewards, "tab_rewards", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::tabQuests, "tab_quests", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::status, "status", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::hasInfo, "has_info", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::levels, "levels", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::bonus, "bonus", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::quests, "quests", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::canTake, "can_take", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::takeLabel, "take_label", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::canClaim, "can_claim", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::claimLabel, "claim_label", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::rerollLabel, "reroll_label", updated);
    SyncFieldFrom(binder, &BattlePassRmlModel::rerollTooltip, "reroll_tooltip", updated);

    wchar_t closeText[256] = {};
    mu_swprintf_s(closeText, I18N::Game::CloseS, L"X");
    SyncField(binder, &BattlePassRmlModel::exitTooltip, "exit_tooltip", Narrow(closeText));
    return true;
}

void mu::ui::window::CBattlePassWindow::SyncTimers(bool force)
{
    auto& pass = Pass();
    const auto& status = pass.GetStatus();
    const bool shown = pass.HasInfo() && !pass.IsOff();
    // The seconds since the answer: the server's countdowns run on from it, so the PC's clock doesn't matter.
    const long long elapsed =
        shown ? std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - status.ReceivedAt).count()
              : -1;
    if (!force && elapsed == m_timersAt)
        return;
    m_timersAt = elapsed;

    BattlePassLine time;
    std::wstring dayTimer;
    std::wstring weekTimer;
    std::wstring seasonTimer;
    if (shown)
    {
        const long long seasonLeft = status.SecondsLeft - elapsed;
        switch (status.SeasonPhase)
        {
        case Phase::NotStarted:
            time = TextLine(Format(I18N::Game::BattlePassStartsIn, FormatDuration(seasonLeft).c_str()), HALF_WIDTH);
            break;
        case Phase::Running:
            seasonTimer = Format(I18N::Game::BattlePassEndsIn, FormatDuration(seasonLeft).c_str());
            time = TextLine(seasonTimer, HALF_WIDTH);
            // New quests come only before the season ends; a server that doesn't say shows no time.
            if (status.DayLeft > 0 && status.DayLeft < status.SecondsLeft)
                dayTimer = Format(I18N::Game::BattlePassNewIn, FormatDuration(status.DayLeft - elapsed).c_str());
            if (status.WeekLeft > 0 && status.WeekLeft < status.SecondsLeft)
                weekTimer = Format(I18N::Game::BattlePassNewIn, FormatDuration(status.WeekLeft - elapsed).c_str());
            break;
        default:
            time = TextLine(I18N::Game::BattlePassEnded, HALF_WIDTH);
            break;
        }

        // A countdown that ran out (the day, the week, the season's start or end) asks again, once per answer, a
        // second late so the server's day has turned.
        auto ranOut = [elapsed](int left) { return left > 0 && elapsed > left; };
        if (status.SeasonPhase != Phase::Ended && m_ranOutAskedFor != pass.Revision()
            && (ranOut(status.SecondsLeft) || ranOut(status.DayLeft) || ranOut(status.WeekLeft)))
        {
            m_ranOutAskedFor = pass.Revision();
            pass.Request();
        }
    }

    auto& binder = m_RmlView.Binder();
    SyncField(binder, &BattlePassRmlModel::time, "time", std::move(time));
    SyncField(binder, &BattlePassRmlModel::dayTimer, "day_timer", Narrow(dayTimer));
    SyncField(binder, &BattlePassRmlModel::weekTimer, "week_timer", Narrow(weekTimer));
    SyncField(binder, &BattlePassRmlModel::seasonTimer, "season_timer", Narrow(seasonTimer));
}

void mu::ui::window::CBattlePassWindow::CenterTrack()
{
    if (!m_centerPending || m_tab != 0 || !Pass().HasInfo())
        return;

    Rml::ElementDocument* document = m_RmlView.Document();
    Rml::Element* pane = document != nullptr ? document->GetElementById("bp_levels") : nullptr;
    const int focus = FocusLevel(Pass().GetStatus(), Pass().Levels());
    Rml::Element* row = document != nullptr ? document->GetElementById("bp_lv_" + std::to_string(focus)) : nullptr;
    // Laid out once the rows have a height; until then it waits for the next frame.
    if (pane == nullptr || row == nullptr || row->GetOffsetHeight() <= 0.f || pane->GetClientHeight() <= 0.f)
        return;

    const float top = row->GetOffsetTop() - (pane->GetClientHeight() - row->GetOffsetHeight()) / 2.f;
    pane->SetScrollTop(std::max(0.f, top));
    m_centerPending = false;
}

void mu::ui::window::CBattlePassWindow::RenderItems(const Rml::Vector2f& offset, const Rml::Vector2f& size)
{
    Rml::ElementDocument* document = m_RmlView.Document();
    if (document == nullptr)
        return;

    for (const auto& shown : m_shownItems)
    {
        if (shown.type < 0)
            continue;
        Rml::Element* icon = document->GetElementById(shown.id);
        Rml::Vector2f boxOffset;
        Rml::Vector2f boxSize;
        if (icon == nullptr || !UI::RmlBridge::DrawnBox(*icon, Rml::BoxArea::Border, boxOffset, boxSize))
            continue;
        // Only the boxes the pane shows (the target crops the ones it cuts).
        if (boxOffset.y + boxSize.y <= offset.y || boxOffset.y >= offset.y + size.y)
            continue;
        ::RenderItem3D(boxOffset.x, boxOffset.y, boxSize.x, boxSize.y, shown.type, shown.level, 0, 0, false);
    }
}

void mu::ui::window::CBattlePassWindow::SyncTooltipItem()
{
    const int hovered = IsVisible() && m_tab == 0 && m_hoveredItem >= 0 && m_hoveredItem < static_cast<int>(m_shownItems.size())
                            && m_shownItems[m_hoveredItem].type >= 0
                            && UI::RmlBridge::IsPointerOver(m_RmlView.Document())
                        ? m_hoveredItem
                        : -1;
    if (hovered == m_tooltipFor)
        return;

    ReleaseTooltipItem();
    if (hovered < 0)
        return;

    const ShownItem& shown = m_shownItems[hovered];
    ItemCreationParams params = {};
    params.Group = shown.type / MAX_ITEM_INDEX;
    params.Number = shown.type % MAX_ITEM_INDEX;
    params.Level = static_cast<BYTE>(shown.level);
    // Stacked items (jewels, potions) show their count as their durability.
    params.Durability = static_cast<BYTE>(std::clamp(shown.count, 1, 255));
    m_tooltipItem = g_pNewItemMng->CreateItemByParameters(&params);
    m_tooltipFor = m_tooltipItem != nullptr ? hovered : -1;
}

void mu::ui::window::CBattlePassWindow::ReleaseTooltipItem()
{
    if (m_tooltipItem != nullptr)
    {
        g_pNewItemMng->DeleteItem(m_tooltipItem);
        m_tooltipItem = nullptr;
    }
    m_tooltipFor = -1;
}
