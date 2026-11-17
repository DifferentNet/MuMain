#include "Core/Input/KeyBindings.h"

#include <algorithm>

namespace Core::Input::KeyBindings
{
namespace
{
// Letters the game keeps for itself: A (the master skill tree's old key), H, K (the jewel bank), L, S and Y (the GM
// panel). With them out there are as many bindable keys as actions.
constexpr std::string_view ReservedLetters = "AHKLSY";
constexpr std::string_view HomeName = "Home";
constexpr int HotKeyQ = 0;

constexpr std::array<ActionInfo, ActionCount> ActionTable{{
    {Action::Inventory, "Inventory", 'I', Group::Windows},
    {Action::InventoryAlt, "InventoryAlt", 'V', Group::Windows},
    {Action::Character, "Character", 'C', Group::Windows},
    {Action::Party, "Party", 'P', Group::Windows},
    {Action::Guild, "Guild", 'G', Group::Windows},
    {Action::Friends, "Friends", 'F', Group::Windows},
    {Action::Quests, "Quests", 'T', Group::Windows},
    {Action::Move, "Move", 'M', Group::Windows},
    {Action::WindowMenu, "WindowMenu", 'U', Group::Windows},
    {Action::Command, "Command", 'D', Group::Windows},
    {Action::CommandList, "CommandList", 'J', Group::Windows},
    {Action::Events, "Events", 'O', Group::Windows},
    {Action::DailyRewards, "DailyRewards", 'N', Group::Windows},
    {Action::BattlePass, "BattlePass", 'X', Group::Windows},
    {Action::Gens, "Gens", 'B', Group::Windows},
    {Action::MuHelperSettings, "MuHelperSettings", 'Z', Group::Windows},
    {Action::MuHelperToggle, "MuHelperToggle", KeyHome, Group::Windows},
    {Action::Potion1, "Potion1", 'Q', Group::Potions},
    {Action::Potion2, "Potion2", 'W', Group::Potions},
    {Action::Potion3, "Potion3", 'E', Group::Potions},
    {Action::Potion4, "Potion4", 'R', Group::Potions},
    {Action::SkillSlot1, "SkillSlot1", '1', Group::Skills},
    {Action::SkillSlot2, "SkillSlot2", '2', Group::Skills},
    {Action::SkillSlot3, "SkillSlot3", '3', Group::Skills},
    {Action::SkillSlot4, "SkillSlot4", '4', Group::Skills},
    {Action::SkillSlot5, "SkillSlot5", '5', Group::Skills},
    {Action::SkillSlot6, "SkillSlot6", '6', Group::Skills},
    {Action::SkillSlot7, "SkillSlot7", '7', Group::Skills},
    {Action::SkillSlot8, "SkillSlot8", '8', Group::Skills},
    {Action::SkillSlot9, "SkillSlot9", '9', Group::Skills},
    {Action::SkillSlot0, "SkillSlot0", '0', Group::Skills},
}};

constexpr bool TableFollowsActionOrder()
{
    for (int i = 0; i < ActionCount; ++i)
    {
        if (static_cast<int>(ActionTable[static_cast<size_t>(i)].action) != i)
            return false;
    }
    return true;
}
static_assert(TableFollowsActionOrder(), "ActionTable must list the actions in Action order");

bool IsLetter(int key)
{
    return key >= 'A' && key <= 'Z';
}

bool IsDigit(int key)
{
    return key >= '0' && key <= '9';
}

char ToUpper(char c)
{
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

bool EqualsIgnoringCase(std::string_view a, std::string_view b)
{
    return a.size() == b.size()
        && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return ToUpper(x) == ToUpper(y); });
}

int IndexOf(Action action)
{
    return static_cast<int>(action);
}

bool IsValid(Action action)
{
    return IndexOf(action) >= 0 && IndexOf(action) < ActionCount;
}

std::optional<Action> ActionFromConfigName(std::string_view name)
{
    for (const ActionInfo& info : ActionTable)
    {
        if (EqualsIgnoringCase(info.configName, name))
            return info.action;
    }
    return std::nullopt;
}

// The bindable keys, lowest code first.
std::vector<int> AssignableKeys()
{
    std::vector<int> keys;
    keys.push_back(KeyHome);
    for (int key = '0'; key <= '9'; ++key)
        keys.push_back(key);
    for (int key = 'A'; key <= 'Z'; ++key)
    {
        if (IsAssignable(key))
            keys.push_back(key);
    }
    return keys;
}
} // namespace

std::span<const ActionInfo> Actions()
{
    return ActionTable;
}

const ActionInfo& InfoOf(Action action)
{
    return ActionTable[static_cast<size_t>(IsValid(action) ? IndexOf(action) : 0)];
}

Action DefaultActionOf(int key)
{
    for (const ActionInfo& info : ActionTable)
    {
        if (info.defaultKey == key)
            return info.action;
    }
    return Action::None;
}

bool IsAssignable(int key)
{
    if (key == KeyHome || IsDigit(key))
        return true;
    return IsLetter(key) && ReservedLetters.find(static_cast<char>(key)) == std::string_view::npos;
}

std::string KeyName(int key)
{
    if (key == KeyHome)
        return std::string(HomeName);
    if (IsLetter(key) || IsDigit(key))
        return std::string(1, static_cast<char>(key));
    return {};
}

std::optional<int> KeyFromName(std::string_view name)
{
    if (EqualsIgnoringCase(name, HomeName))
        return KeyHome;
    if (name.size() != 1)
        return std::nullopt;
    const int key = ToUpper(name.front());
    if (IsLetter(key) || IsDigit(key))
        return key;
    return std::nullopt;
}

Action PotionAction(int hotKeyIndex)
{
    if (hotKeyIndex < HotKeyQ || hotKeyIndex >= HotKeyQ + PotionSlotCount)
        return Action::None;
    return static_cast<Action>(IndexOf(Action::Potion1) + hotKeyIndex - HotKeyQ);
}

Action SkillSlotAction(int hotKey)
{
    if (hotKey < 0 || hotKey >= SkillSlotCount)
        return Action::None;
    if (hotKey == 0)
        return Action::SkillSlot0;
    return static_cast<Action>(IndexOf(Action::SkillSlot1) + hotKey - 1);
}

Table::Table()
{
    Reset();
}

int Table::KeyOf(Action action) const
{
    return IsValid(action) ? m_keys[static_cast<size_t>(IndexOf(action))] : 0;
}

Action Table::ActionOf(int key) const
{
    const auto found = std::find(m_keys.begin(), m_keys.end(), key);
    if (found == m_keys.end())
        return Action::None;
    return static_cast<Action>(found - m_keys.begin());
}

Table::Change Table::Assign(Action action, int key)
{
    if (!IsValid(action) || !IsAssignable(key) || KeyOf(action) == key)
        return {};

    const Action holder = ActionOf(key);
    const int oldKey = KeyOf(action);
    m_keys[static_cast<size_t>(IndexOf(action))] = key;
    if (holder != Action::None)
        m_keys[static_cast<size_t>(IndexOf(holder))] = oldKey;
    return {action, holder};
}

void Table::Reset()
{
    for (const ActionInfo& info : ActionTable)
        m_keys[static_cast<size_t>(IndexOf(info.action))] = info.defaultKey;
}

bool Table::IsDefault() const
{
    return *this == Table();
}

Table::Entries Table::ToEntries() const
{
    Entries entries;
    for (const ActionInfo& info : ActionTable)
    {
        const int key = KeyOf(info.action);
        if (key != info.defaultKey)
            entries.emplace_back(std::string(info.configName), KeyName(key));
    }
    return entries;
}

Table Table::FromEntries(const Entries& entries)
{
    constexpr int Unplaced = 0;
    std::array<int, ActionCount> wanted{};
    for (const auto& [name, keyName] : entries)
    {
        const std::optional<Action> action = ActionFromConfigName(name);
        const std::optional<int> key = KeyFromName(keyName);
        if (action && key && IsAssignable(*key))
            wanted[static_cast<size_t>(IndexOf(*action))] = *key;
    }

    Table table;
    table.m_keys.fill(Unplaced);
    const auto taken = [&table](int key)
    { return std::find(table.m_keys.begin(), table.m_keys.end(), key) != table.m_keys.end(); };

    for (int i = 0; i < ActionCount; ++i)
    {
        const int key = wanted[static_cast<size_t>(i)];
        if (key != Unplaced && !taken(key))
            table.m_keys[static_cast<size_t>(i)] = key;
    }

    // Defaults first for every action left, so a lowest-free pick never takes a later action's free default.
    for (const ActionInfo& info : ActionTable)
    {
        int& key = table.m_keys[static_cast<size_t>(IndexOf(info.action))];
        if (key == Unplaced && !taken(info.defaultKey))
            key = info.defaultKey;
    }

    const std::vector<int> freeOrder = AssignableKeys();
    for (int& key : table.m_keys)
    {
        if (key != Unplaced)
            continue;
        const auto free = std::find_if(freeOrder.begin(), freeOrder.end(), [&taken](int k) { return !taken(k); });
        if (free != freeOrder.end())
            key = *free;
    }
    return table;
}
} // namespace Core::Input::KeyBindings
