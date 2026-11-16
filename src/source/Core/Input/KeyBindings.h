#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The player's key bindings (Options > Gameplay > Key bindings): which key opens each window, drinks each potion
// slot and casts each skill slot. Escape, Enter, Tab, Space, the F keys, the arrows and the keys inside windows
// stay fixed. The bindable keys are exactly as many as the actions, so every key belongs to one action and giving
// an action a key that another one holds swaps the two. Key codes are Windows virtual-key codes: 'A'..'Z',
// '0'..'9' and Home. This file holds the pure rules; KeyBindingsRuntime.h keeps the player's table in config.ini.
namespace Core::Input::KeyBindings
{
enum class Action : std::uint8_t
{
    // Windows and switches.
    Inventory,
    InventoryAlt,
    Character,
    Party,
    Guild,
    Friends,
    Quests,
    Move,
    WindowMenu,
    Command,
    CommandList,
    Events,
    DailyRewards,
    BattlePass,
    Gens,
    MuHelperSettings,
    MuHelperToggle,
    // The potion slots Q, W, E, R by default (CItemHotKey's HOTKEY_Q..HOTKEY_R).
    Potion1,
    Potion2,
    Potion3,
    Potion4,
    // The skill slots, keyboard order: 1..9, then 0 (CSkillList's hot key 1..9, then 0).
    SkillSlot1,
    SkillSlot2,
    SkillSlot3,
    SkillSlot4,
    SkillSlot5,
    SkillSlot6,
    SkillSlot7,
    SkillSlot8,
    SkillSlot9,
    SkillSlot0,
    Count,
    None = Count,
};

inline constexpr int ActionCount = static_cast<int>(Action::Count);
inline constexpr int PotionSlotCount = 4;
inline constexpr int SkillSlotCount = 10;

// The Home key's code (VK_HOME); KeyBindingsRuntime.cpp checks that they agree.
inline constexpr int KeyHome = 0x24;

// The groups the window shows the actions in.
enum class Group : std::uint8_t
{
    Windows,
    Potions,
    Skills,
};

struct ActionInfo
{
    Action action;
    // The key's name in config.ini's [KeyBindings] section.
    std::string_view configName;
    int defaultKey;
    Group group;
};

// Every action, in Action order.
[[nodiscard]] std::span<const ActionInfo> Actions();
[[nodiscard]] const ActionInfo& InfoOf(Action action);

// The action whose default key this is; None for any other key (Escape, Tab, a reserved letter...).
[[nodiscard]] Action DefaultActionOf(int key);

// Whether a key may be bound: the letters but A, H, K, L, S and Y (kept for the game's own uses), the digits and Home.
[[nodiscard]] bool IsAssignable(int key);

// "I", "7", "Home"; "" for a key without a name here.
[[nodiscard]] std::string KeyName(int key);
// The key a name stands for, any letter case; nullopt for an unknown name.
[[nodiscard]] std::optional<int> KeyFromName(std::string_view name);

// The action of potion slot HOTKEY_Q..HOTKEY_R (0..3); None outside.
[[nodiscard]] Action PotionAction(int hotKeyIndex);
// The action of skill hot key 0..9 (CSkillList: 1..9 and 0); None outside.
[[nodiscard]] Action SkillSlotAction(int hotKey);

class Table
{
public:
    // The action and key name of every action not on its default key, as config.ini stores them.
    using Entries = std::vector<std::pair<std::string, std::string>>;

    // What one assignment changed: the action given the key, and the one that had it and took the other's key.
    struct Change
    {
        Action first = Action::None;
        Action swapped = Action::None;
    };

    // The default keys.
    Table();

    [[nodiscard]] int KeyOf(Action action) const;
    // The action bound to the key; None when no action has it.
    [[nodiscard]] Action ActionOf(int key) const;

    // Gives the action the key; the action that held it takes the action's old key. Nothing changes (an empty
    // Change) for a key that may not be bound or the key the action already has.
    Change Assign(Action action, int key);
    void Reset();
    [[nodiscard]] bool IsDefault() const;

    [[nodiscard]] Entries ToEntries() const;
    // Reads saved entries and repairs what does not fit: unknown actions and key names are skipped, a key that may
    // not be bound or that an earlier action (Action order) took is dropped; then every action left without a key
    // takes its default key if that is free, and those still left take the lowest free bindable keys. The result
    // never binds one key twice.
    [[nodiscard]] static Table FromEntries(const Entries& entries);

    bool operator==(const Table&) const = default;

private:
    std::array<int, ActionCount> m_keys{};
};
} // namespace Core::Input::KeyBindings
